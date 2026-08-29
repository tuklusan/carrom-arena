#include "app.h"
#include "common/types.h"
#include "common/rng.h"
#include "common/strategy_profiles.h"
#include "platform/platform.h"
#include "game/match.h"
#include "game/board.h"
#include "game/rules.h"
#include "physics/physics.h"
#include "ai/controller.h"
#include "telemetry/trace.h"
#include "render/renderer.h"
#include "render/effects.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* -----------------------------------------------------------------------------
 * Application Context
 * --------------------------------------------------------------------------- */
struct AppContext {
    AppConfig config;
    RNGContext rng;
    MatchState match;
    GameState game;
    PhysicsWorld* physics;
    TraceWriter* trace;
    Renderer* renderer;
    Controller* controllers[4];  // One per seat
    uint64_t frame_count;
    uint64_t shot_count;
    bool running;
    bool paused;
    double last_frame_time;
    double accumulator;
};

/* -----------------------------------------------------------------------------
 * Helpers
 * --------------------------------------------------------------------------- */
static void app_init_controllers(AppContext* ctx) {
    for (int i = 0; i < 4; i++) {
        const StrategyProfile* profile = strategy_by_index(seat_to_strategy((Seat)i));
        RNGSnapshot snap = rng_snapshot(&ctx->rng.streams[i]);
        ctx->controllers[i] = arena_controller_create((Seat)i, profile, &ctx->rng.streams[i]);
    }
}

static void app_cleanup_controllers(AppContext* ctx) {
    for (int i = 0; i < 4; i++) {
        if (ctx->controllers[i]) {
            controller_destroy(ctx->controllers[i]);
            ctx->controllers[i] = NULL;
        }
    }
}

static void app_init_match(AppContext* ctx) {
    match_state_init(&ctx->match);
    game_state_init(&ctx->game, ctx->rng.master_seed);
    ctx->game.turn_seat = SEAT_NORTH;
    ctx->frame_count = 0;
    ctx->shot_count = 0;
}

static void app_setup_trace(AppContext* ctx) {
    if (ctx->config.trace_dir) {
        platform_mkdir(ctx->config.trace_dir);
        char trace_path[512];
        snprintf(trace_path, sizeof(trace_path), "%s/trace_%llu.jsonl", 
                 ctx->config.trace_dir, (unsigned long long)ctx->rng.master_seed);
        ctx->trace = trace_open(trace_path, ctx->config.trace_dir, ctx->config.verbose, ctx->rng.master_seed);
    }
}

static void app_setup_renderer(AppContext* ctx) {
    if (ctx->config.mode == APP_MODE_RENDERED) {
        if (!ctx->config.headless) {
            ctx->renderer = renderer_create(ctx->config.window_width, ctx->config.window_height, 
                                             "Carrom Arena", false);
        }
    } else if (ctx->config.mode == APP_MODE_CAPTURE) {
        // Capture mode always needs a renderer (windowed)
        ctx->renderer = renderer_create(ctx->config.window_width, ctx->config.window_height, 
                                         "Carrom Arena", true);
    }
}

/* -----------------------------------------------------------------------------
 * Core Simulation Step (Fixed Timestep)
 * --------------------------------------------------------------------------- */
// Use physics.h definitions: PHYSICS_HZ, PHYSICS_DT, MAX_SUBSTEPS

static void app_simulation_step(AppContext* ctx, double dt) {
    ctx->accumulator += dt;
    
    int substeps = 0;
    while (ctx->accumulator >= PHYSICS_DT && substeps < MAX_SUBSTEPS) {
        physics_step(ctx->physics, PHYSICS_DT);
        ctx->accumulator -= PHYSICS_DT;
        substeps++;
    }
}

static bool app_is_shot_settled(AppContext* ctx) {
    return physics_is_settled(ctx->physics);
}

static ShotResult app_collect_shot_result(AppContext* ctx) {
    ShotResult result;
    shot_result_init(&result);
    
    // Extract pocketed pieces from physics
    physics_collect_pocketed(ctx->physics, &result);
    
    // Get final settled positions
    physics_get_final_positions(ctx->physics, result.final_positions);
    result.sim_time = physics_get_sim_time(ctx->physics);
    
    return result;
}

/* -----------------------------------------------------------------------------
 * Shot Execution Flow
 * --------------------------------------------------------------------------- */
static void app_execute_shot(AppContext* ctx) {
    Seat seat = ctx->game.turn_seat;
    Controller* controller = ctx->controllers[seat];
    
    // Create decision snapshot (immutable view for AI)
    DecisionSnapshot snap = {
        .match = &ctx->match,
        .game = &ctx->game,
        .board = &ctx->game.board,
        .physics = physics_snapshot(ctx->physics),
        .active_seat = seat
    };
    
    // AI decides shot plan
    ShotPlan plan = controller_decide(controller, &snap, &ctx->rng.streams[seat]);
    
    // Validate shot plan
    if (!match_validate_shot(&ctx->game, &plan)) {
        // Fallback: minimal legal shot
        plan = controller_fallback_shot(controller, &snap, &ctx->rng.streams[seat]);
    }
    
    // Execute in live physics
    physics_place_striker(ctx->physics, seat, plan.placement);
    physics_apply_shot(ctx->physics, plan.aim_angle, plan.power);
    
    // Update game phase
    ctx->game.phase = PHASE_SHOT_EXECUTION;
    
    // Log shot plan
    if (ctx->trace) {
        trace_write_shot_start(ctx->trace, &ctx->match, &ctx->game, 
                               ctx->shot_count, seat, &plan);
    }
    
    ctx->shot_count++;
}

static void app_resolve_shot(AppContext* ctx, const ShotResult* result) {
    // Extract facts for rules engine
    ShotFacts facts;
    match_extract_facts(&ctx->game, result, &facts);
    
    // Resolve through rules engine
    RulesOutcome outcome = rules_resolve(&ctx->match, &ctx->game, &facts);
    
    // Apply outcome
    ctx->game = outcome.next_game_state;
    ctx->match = outcome.next_match_state;
    
    // Set phase for next turn based on turn decision
    switch (outcome.turn_decision) {
        case TURN_CONTINUE:
        case TURN_ADVANCE:
            ctx->game.phase = PHASE_PLACEMENT;
            break;
        case TURN_BOARD_OVER:
            // match_start_board will set PHASE_PLACEMENT
            break;
        case TURN_GAME_OVER:
        case TURN_MATCH_OVER:
            ctx->running = false;
            break;
        default:
            ctx->game.phase = PHASE_IDLE;
            break;
    }
    
    // Emit events
    for (int i = 0; i < outcome.event_count; i++) {
        if (ctx->trace) {
            trace_write_event(ctx->trace, &outcome.events[i]);
        }
    }
    
    // Write shot result to trace
    if (ctx->trace) {
        trace_write_shot_end(ctx->trace, result, &outcome);
    }
    
    // Trigger pocket fade effects
    if (ctx->renderer) {
        for (int i = 0; i < result->pocketed_count; i++) {
            // Find which pocket the piece went into (simplified - use first pocket for now)
            effects_trigger_pocket_fade(0);
        }
        if (result->queen_pocketed) {
            effects_trigger_pocket_fade(0);
        }
    }
    
    // Start next board if needed
    if (outcome.turn_decision == TURN_BOARD_OVER) {
        if (!match_is_over(&ctx->match)) {
            match_start_board(&ctx->match, &ctx->game, &ctx->rng);
            physics_sync_from_board(ctx->physics, &ctx->game.board, ctx->game.turn_seat);
        }
    }
}

/* -----------------------------------------------------------------------------
 * Main Simulation Loop
 * --------------------------------------------------------------------------- */
int app_run_simulation(AppContext* ctx) {
    ctx->running = true;
    ctx->paused = false;
    ctx->last_frame_time = platform_time_now();
    ctx->accumulator = 0.0;
    uint64_t debug_frame = 0;
    
    // Initialize first board
    match_start_board(&ctx->match, &ctx->game, &ctx->rng);
    physics_sync_from_board(ctx->physics, &ctx->game.board, ctx->game.turn_seat);
    
    if (ctx->config.verbose) {
        printf("[DEBUG] Starting simulation, seed=%llu\n", (unsigned long long)ctx->rng.master_seed);
        fflush(stdout);
    }
    
    while (ctx->running) {
        double now = platform_time_now();
        double dt = now - ctx->last_frame_time;
        ctx->last_frame_time = now;
        
        // Clamp dt to avoid spiral of death
        if (dt > 0.25) dt = 0.25;
        
        // Handle input (mode-specific)
        if (ctx->renderer) {
            renderer_poll_events(ctx->renderer);
            if (renderer_should_close(ctx->renderer)) {
                ctx->running = false;
                break;
            }
            ctx->paused = renderer_is_paused(ctx->renderer);
        }
        
        if (!ctx->paused) {
            // Periodic debug output
            if (ctx->config.verbose && (ctx->frame_count % 1000 == 0)) {
                printf("[DEBUG] Frame %llu: phase=%d, sim_time=%.2f, shots=%llu\n", 
                       (unsigned long long)ctx->frame_count, ctx->game.phase, 
                       physics_get_sim_time(ctx->physics), (unsigned long long)ctx->shot_count);
                fflush(stdout);
            }
            
            // Fixed timestep physics
            app_simulation_step(ctx, dt);
            
            // State machine
            switch (ctx->game.phase) {
                case PHASE_IDLE:
                case PHASE_PLACEMENT:
                    // AI plans shot during placement phase
                    if (ctx->game.phase == PHASE_PLACEMENT) {
                        if (ctx->config.verbose) {
                            printf("[DEBUG] Frame %llu: Executing shot for seat %d\n", 
                                   (unsigned long long)ctx->frame_count, ctx->game.turn_seat);
                            fflush(stdout);
                        }
                        app_execute_shot(ctx);
                    }
                    break;
                    
                case PHASE_SHOT_EXECUTION:
                    if (app_is_shot_settled(ctx)) {
                        ctx->game.phase = PHASE_SETTLING;
                    }
                    break;
                    
                case PHASE_SETTLING:
                    // Double-check settling
                    if (app_is_shot_settled(ctx)) {
                        ctx->game.phase = PHASE_RESOLVING;
                        ShotResult result = app_collect_shot_result(ctx);
                        app_resolve_shot(ctx, &result);
                        // app_resolve_shot now handles phase transitions
                    }
                    break;
                    
                case PHASE_RESOLVING:
                    // Handled above
                    break;
                    
                default:
                    break;
            }
        }
        
        // Render (mode-specific)
        if (ctx->renderer) {
            renderer_begin(ctx->renderer);
            renderer_draw_board(ctx->renderer, &ctx->game.board, ctx->physics);
            renderer_draw_hud(ctx->renderer, &ctx->match, &ctx->game);
            renderer_draw_effects(ctx->renderer, &ctx->game);
            renderer_end(ctx->renderer);
            
            // Capture frames if in capture mode
            if (ctx->config.mode == APP_MODE_CAPTURE && ctx->frame_count < ctx->config.frames) {
                renderer_capture_frame(ctx->renderer, ctx->config.capture_dir, ctx->frame_count);
            }
            ctx->frame_count++;
        }
        
        // Soak mode: run as fast as possible (no sleep)
        if (ctx->config.mode == APP_MODE_SOAK) {
            // No frame limiting
        } else if (ctx->config.mode == APP_MODE_DIAGNOSTIC) {
            // Step-through mode handled by renderer
        } else {
            // Rendered mode: limit to ~60 FPS
            double frame_time = platform_time_now() - now;
            double target_frame_time = 1.0 / 60.0;
            if (frame_time < target_frame_time) {
                platform_sleep_ms((uint32_t)((target_frame_time - frame_time) * 1000));
            }
        }
    }
    
    return 0;
}

/* -----------------------------------------------------------------------------
 * Public API
 * --------------------------------------------------------------------------- */
AppContext* app_create(const AppConfig* config) {
    AppContext* ctx = calloc(1, sizeof(AppContext));
    if (!ctx) return NULL;
    
    ctx->config = *config;
    
    // Initialize RNG
    uint64_t seed = config->seed;
    printf("[DEBUG] CLI parsed seed: %llu\n", (unsigned long long)seed);
    fflush(stdout);
    if (seed == 0) {
        seed = platform_time_us();
    }
    rng_context_init(&ctx->rng, seed);
    
    // Initialize physics
    ctx->physics = physics_create();
    
    // Setup subsystems
    app_setup_trace(ctx);
    app_setup_renderer(ctx);
    app_init_controllers(ctx);
    app_init_match(ctx);
    
    return ctx;
}

void app_destroy(AppContext* ctx) {
    if (!ctx) return;
    
    app_cleanup_controllers(ctx);
    
    if (ctx->trace) {
        trace_close(ctx->trace);
    }
    
    if (ctx->renderer) {
        renderer_destroy(ctx->renderer);
    }
    
    if (ctx->physics) {
        physics_destroy(ctx->physics);
    }
    
    free(ctx);
}

int app_run(AppContext* ctx) {
    switch (ctx->config.mode) {
        case APP_MODE_RENDERED:
            return app_run_rendered(ctx);
        case APP_MODE_DIAGNOSTIC:
            return app_run_diagnostic(ctx);
        case APP_MODE_SOAK:
            return app_run_soak(ctx);
        case APP_MODE_CAPTURE:
            return app_run_capture(ctx);
        default:
            return app_run_simulation(ctx);
    }
}

int app_run_rendered(AppContext* ctx) {
    return app_run_simulation(ctx);
}

int app_run_diagnostic(AppContext* ctx) {
    ctx->config.verbose = true;
    return app_run_simulation(ctx);
}

int app_run_soak(AppContext* ctx) {
    // Soak mode: run multiple boards/seeds/matches headless
    printf("Soak test: %u boards x %u seeds x %u matches\n", 
           ctx->config.boards, ctx->config.seeds, ctx->config.matches);
    
    for (uint32_t s = 0; s < ctx->config.seeds; s++) {
        rng_context_init(&ctx->rng, ctx->config.seed + s);
        app_init_controllers(ctx);  // Recreate controllers with new RNG
        
        for (uint32_t b = 0; b < ctx->config.boards; b++) {
            for (uint32_t m = 0; m < ctx->config.matches; m++) {
                app_init_match(ctx);
                match_start_board(&ctx->match, &ctx->game, &ctx->rng);
                app_run_simulation(ctx);
            }
        }
    }
    
    printf("Soak test complete.\n");
    return 0;
}

int app_run_capture(AppContext* ctx) {
    platform_mkdir(ctx->config.capture_dir);
    return app_run_simulation(ctx);
}

/* -----------------------------------------------------------------------------
 * CLI Parsing
 * --------------------------------------------------------------------------- */
static const char* get_arg_value(const char* arg, const char* prefix) {
    size_t len = strlen(prefix);
    if (strncmp(arg, prefix, len) == 0) {
        if (arg[len] == '=') {
            return arg + len + 1;
        }
    }
    return NULL;
}

AppConfig app_parse_args(int argc, char* argv[]) {
    AppConfig config = app_config_default();
    
    printf("[DEBUG] argc=%d\n", argc);
    for (int i = 0; i < argc; i++) {
        printf("[DEBUG] argv[%d]=%s\n", i, argv[i]);
    }
    fflush(stdout);
    
    for (int i = 1; i < argc; i++) {
        const char* val;
        if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
            app_print_usage(argv[0]);
            exit(0);
        } else if (strcmp(argv[i], "--version") == 0 || strcmp(argv[i], "-v") == 0) {
            app_print_version();
            exit(0);
        } else if ((val = get_arg_value(argv[i], "--mode")) != 0) {
            if (strcmp(val, "rendered") == 0) config.mode = APP_MODE_RENDERED;
            else if (strcmp(val, "diagnostic") == 0) config.mode = APP_MODE_DIAGNOSTIC;
            else if (strcmp(val, "soak") == 0) config.mode = APP_MODE_SOAK;
            else if (strcmp(val, "capture") == 0) config.mode = APP_MODE_CAPTURE;
        } else if ((val = get_arg_value(argv[i], "--seed")) != 0) {
            config.seed = strtoull(val, NULL, 10);
        } else if ((val = get_arg_value(argv[i], "--boards")) != 0) {
            config.boards = (uint32_t)strtoul(val, NULL, 10);
        } else if ((val = get_arg_value(argv[i], "--seeds")) != 0) {
            config.seeds = (uint32_t)strtoul(val, NULL, 10);
        } else if ((val = get_arg_value(argv[i], "--matches")) != 0) {
            config.matches = (uint32_t)strtoul(val, NULL, 10);
        } else if ((val = get_arg_value(argv[i], "--frames")) != 0) {
            config.frames = (uint32_t)strtoul(val, NULL, 10);
        } else if ((val = get_arg_value(argv[i], "--trace-dir")) != 0) {
            config.trace_dir = val;
        } else if ((val = get_arg_value(argv[i], "--capture-dir")) != 0) {
            config.capture_dir = val;
        } else if ((val = get_arg_value(argv[i], "--width")) != 0) {
            config.window_width = atoi(val);
        } else if ((val = get_arg_value(argv[i], "--height")) != 0) {
            config.window_height = atoi(val);
        } else if ((val = get_arg_value(argv[i], "--replay")) != 0) {
            config.replay_file = val;
        } else if (strcmp(argv[i], "--verbose") == 0) {
            config.verbose = true;
        } else if (strcmp(argv[i], "--headless") == 0) {
            config.headless = true;
        }
    }
    
    return config;
}

void app_print_usage(const char* prog_name) {
    printf("Carrom Arena - Autonomous Four-Player Carrom Simulation\n\n");
    printf("Usage: %s [options]\n\n", prog_name);
    printf("Options:\n");
    printf("  --mode <mode>       Mode: rendered, diagnostic, soak, capture (default: rendered)\n");
    printf("  --seed <n>          Master RNG seed (0 = random)\n");
    printf("  --boards <n>        Boards per seed (soak mode, default: 100)\n");
    printf("  --seeds <n>         Number of seeds (soak mode, default: 100)\n");
    printf("  --matches <n>       Matches per board/seed (soak mode, default: 10)\n");
    printf("  --frames <n>        Frames to capture (capture mode, default: 300)\n");
    printf("  --trace-dir <path>  Trace output directory (default: traces)\n");
    printf("  --capture-dir <path> Capture output directory (default: captures)\n");
    printf("  --verbose           Verbose logging\n");
    printf("  --headless          Force headless mode\n");
    printf("  --width <n>         Window width (default: 1280)\n");
    printf("  --height <n>        Window height (default: 720)\n");
    printf("  --replay <file>     Replay trace file\n");
    printf("  --help, -h          Show this help\n");
    printf("  --version, -v       Show version\n");
}

void app_print_version(void) {
    printf("Carrom Arena v1.0.0 (Build: %s)\n", BUILD_ID);
}
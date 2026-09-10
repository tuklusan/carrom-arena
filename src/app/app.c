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
    uint64_t capture_frame_count;  // Persists across boards in capture mode
    bool running;
    bool paused;
    double last_frame_time;
    double accumulator;
    float playback_speed;  // Current playback speed multiplier
    bool speed_paused;     // Temporary pause from space key (speed = 0)
    double placement_timer;  // Timer for striker placement phase (seconds)
    bool placement_phase_active;  // Whether we're in the placement hold phase
    
    /* THINKING phase (R6) */
    bool thinking_phase_active;
    double thinking_timer;  // Wall-time budget for AI thinking
    int candidates_evaluated;  // Number of candidates evaluated so far (for HUD)
    int max_candidates;  // Max candidates for current mode (R5)
    ShotPlan pending_shot_plan;  // Shot plan decided during THINKING phase
    bool pending_shot_valid;     // Whether pending_shot_plan is valid
    
    /* AIM_PREVIEW phase */
    double aim_preview_timer;        // Wall-time timer for aim preview (5 seconds)
    bool aim_preview_active;         // Whether we're in the aim preview phase
    double aim_preview_start_wall;   // Wall time when aim preview started (for figure animation)
    double thinking_min_wall;        // Minimum wall time for THINKING phase visualization
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
    
    // Set max candidates based on mode (R5)
    // For capture/rendered mode, scale candidates with ai_budget_ms to make THINKING phase visible
    if (ctx->config.mode == APP_MODE_RENDERED || ctx->config.mode == APP_MODE_CAPTURE) {
        if (ctx->config.ai_budget_ms >= 1000) {
            ctx->max_candidates = 12;  // More candidates for visualization
        } else if (ctx->config.ai_budget_ms >= 500) {
            ctx->max_candidates = 6;
        } else {
            ctx->max_candidates = 3;  // Reduced for fast mode
        }
    } else {
        ctx->max_candidates = MAX_CANDIDATES;  // Full for soak/diagnostic
    }
    
    ctx->thinking_phase_active = false;
    ctx->thinking_timer = 0.0;
    ctx->candidates_evaluated = 0;
    ctx->aim_preview_active = false;
    ctx->aim_preview_timer = 0.0;
    ctx->aim_preview_start_wall = 0.0;
    ctx->thinking_min_wall = 0.0;
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
                                             "Carrom Arena", false, false);
        }
    } else if (ctx->config.mode == APP_MODE_CAPTURE) {
        // Capture mode always needs a renderer (windowed or hidden)
        ctx->renderer = renderer_create(ctx->config.window_width, ctx->config.window_height, 
                                         "Carrom Arena", true, ctx->config.headless);
    }
}

/* -----------------------------------------------------------------------------
 * Core Simulation Step (Fixed Timestep)
 * --------------------------------------------------------------------------- */
// Use physics.h definitions: PHYSICS_HZ, PHYSICS_DT, MAX_SUBSTEPS

static void app_simulation_step(AppContext* ctx, double dt) {
    // Apply playback speed: at 0.5x, 1 wall-clock second advances 0.5s of simulation
    // Space key pause sets speed_paused=true, which forces dt=0
    if (ctx->speed_paused) {
        dt = 0.0;
    }
    ctx->accumulator += dt * ctx->playback_speed;
    
    int substeps = 0;
    while (ctx->accumulator >= PHYSICS_DT && substeps < MAX_SUBSTEPS) {
        // Store previous positions BEFORE stepping (for render interpolation)
        physics_get_positions(ctx->physics, ctx->game.board.prev_piece_positions);
        physics_get_striker_position(ctx->physics, &ctx->game.board.prev_striker_position);
        
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
        .active_seat = seat,
        .ai_budget_ms = ctx->config.ai_budget_ms,
        .max_candidates = ctx->max_candidates
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
    ctx->thinking_phase_active = false;
    ctx->candidates_evaluated = 0;
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
            ctx->game.phase = PHASE_THINKING;
            ctx->thinking_phase_active = true;
            ctx->thinking_timer = 0.0;
            ctx->candidates_evaluated = 0;
            ctx->pending_shot_valid = false;
            // Minimum thinking time for visualization
            double budget_sec = (ctx->config.ai_budget_ms > 0) ? (ctx->config.ai_budget_ms / 1000.0) : 0.15;
            ctx->thinking_min_wall = budget_sec * 0.2;
            if (ctx->thinking_min_wall < 2.0) ctx->thinking_min_wall = 2.0;  // At least 2 seconds for verification
            ctx->thinking_min_wall += platform_time_now();
            break;
        case TURN_BOARD_OVER:
            // match_start_board will set PHASE_THINKING for new board
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
    ctx->thinking_phase_active = false;
    ctx->thinking_timer = 0.0;
    ctx->candidates_evaluated = 0;
    ctx->pending_shot_valid = false;
    ctx->aim_preview_active = false;
    ctx->aim_preview_timer = 0.0;
    uint64_t debug_frame = 0;
    
    // Wall-time budget for capture mode (hard timeout to prevent hangs)
    double capture_start_wall = 0.0;
    double capture_max_wall = 0.0;
    if (ctx->config.mode == APP_MODE_CAPTURE) {
        capture_start_wall = platform_time_now();
        capture_max_wall = (ctx->config.frames * 0.5 > 30.0) ? ctx->config.frames * 0.5 : 30.0;
    }
    
    // Initialize first board - start with THINKING for first turn
    match_start_board(&ctx->match, &ctx->game, &ctx->rng);
    physics_sync_from_board(ctx->physics, &ctx->game.board, ctx->game.turn_seat);
    ctx->game.phase = PHASE_THINKING;
    ctx->thinking_phase_active = true;
    ctx->thinking_timer = 0.0;
    // Minimum thinking time for visualization (flash animation visibility)
    // Use 2 seconds for verification, or 20% of ai_budget_ms, whichever is larger
    double budget_sec = (ctx->config.ai_budget_ms > 0) ? (ctx->config.ai_budget_ms / 1000.0) : 0.15;
    ctx->thinking_min_wall = budget_sec * 0.2;
    if (ctx->thinking_min_wall < 2.0) ctx->thinking_min_wall = 2.0;  // At least 2 seconds for verification
    ctx->thinking_min_wall += platform_time_now();  // Absolute wall time when min thinking ends
    
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
            
            // Sync playback speed from renderer (modified by +/- keys)
            ctx->playback_speed = renderer_get_playback_speed(ctx->renderer);
            ctx->speed_paused = ctx->paused;
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
                    break;
                    
                case PHASE_THINKING:
                    // AI thinking phase: compute shot plan
                    if (ctx->thinking_phase_active) {
                        ctx->thinking_timer += dt;
                        
                        // Check for transition at START of frame (deferred from previous frame)
                        double wall_now = platform_time_now();
                        if (ctx->pending_shot_valid && wall_now >= ctx->thinking_min_wall) {
                            ctx->thinking_phase_active = false;
                            ctx->game.phase = PHASE_PLACEMENT;
                            ctx->placement_phase_active = false;
                            ctx->thinking_timer = 0.0;
                            break;  // Exit switch, will re-enter with new phase next frame
                        }
                        
                        // Run AI decision (this may take multiple frames due to budget)
                        if (!ctx->pending_shot_valid) {
                            Seat seat = ctx->game.turn_seat;
                            Controller* controller = ctx->controllers[seat];
                            
                            DecisionSnapshot snap = {
                                .match = &ctx->match,
                                .game = &ctx->game,
                                .board = &ctx->game.board,
                                .physics = physics_snapshot(ctx->physics),
                                .active_seat = seat,
                                .ai_budget_ms = ctx->config.ai_budget_ms,
                                .max_candidates = ctx->max_candidates
                            };
                            
                            // AI decides shot plan
                            ShotPlan plan = controller_decide(controller, &snap, &ctx->rng.streams[seat]);
                            
                            // Validate shot plan
                            if (!match_validate_shot(&ctx->game, &plan)) {
                                plan = controller_fallback_shot(controller, &snap, &ctx->rng.streams[seat]);
                            }
                            
                            ctx->pending_shot_plan = plan;
                            ctx->pending_shot_valid = true;
                            
                            if (ctx->config.verbose) {
                                printf("[DEBUG] Frame %llu: THINKING complete for seat %d, tactic=%d\n", 
                                       (unsigned long long)ctx->frame_count, seat, plan.tactic);
                                fflush(stdout);
                            }
                        }
                        
                        // For HUD: estimate candidates evaluated based on time
                        ctx->candidates_evaluated = (int)(ctx->thinking_timer * 1000.0 / 
                            ((ctx->config.ai_budget_ms > 0) ? ctx->config.ai_budget_ms : 150) * ctx->max_candidates);
                        if (ctx->candidates_evaluated > ctx->max_candidates) {
                            ctx->candidates_evaluated = ctx->max_candidates;
                        }
                    }
                    break;
                    
                case PHASE_PLACEMENT:
                    // Striker placement phase: hold for 1.0s / playback_speed before striking
                    if (!ctx->placement_phase_active) {
                        // Just entered placement phase - start timer
                        // Use pre-computed shot plan from THINKING phase
                        ctx->placement_timer = 1.0 / fmaxf(ctx->playback_speed, 0.05f);
                        ctx->placement_phase_active = true;
                        if (ctx->config.verbose) {
                            printf("[DEBUG] Frame %llu: Placement phase started for seat %d, timer=%.2fs\n", 
                                   (unsigned long long)ctx->frame_count, ctx->game.turn_seat, ctx->placement_timer);
                            fflush(stdout);
                        }
                    }
                    
                    // Count down timer (only if not paused)
                    if (!ctx->paused && !ctx->speed_paused) {
                        ctx->placement_timer -= dt;
                    }
                    
                    // Timer expired - transition to AIM_PREVIEW with the pre-computed shot plan
                    if (ctx->placement_timer <= 0.0) {
                        ctx->placement_phase_active = false;
                        if (ctx->config.verbose) {
                            printf("[DEBUG] Frame %llu: Placement complete, entering AIM_PREVIEW for seat %d\n", 
                                   (unsigned long long)ctx->frame_count, ctx->game.turn_seat);
                            fflush(stdout);
                        }
                        
                        // Store the computed shot plan in GameState for renderer access
                        ctx->game.computed_shot_plan = ctx->pending_shot_plan;
                        ctx->game.computed_shot_valid = true;
                        
                        // Start AIM_PREVIEW phase (5 seconds wall time, unaffected by playback_speed)
                        ctx->aim_preview_timer = 5.0;
                        ctx->aim_preview_active = true;
                        ctx->aim_preview_start_wall = platform_time_now();  // Record start for figure animation
                        ctx->game.phase = PHASE_AIM_PREVIEW;
                        ctx->pending_shot_valid = false;
                    }
                    break;
                    
                case PHASE_AIM_PREVIEW:
                    // AIM_PREVIEW phase: 5 seconds wall time (unaffected by playback_speed)
                    if (ctx->aim_preview_active) {
                        // Use wall time (GetTime() equivalent) - not simulation time
                        double wall_now = platform_time_now();
                        double elapsed_wall = wall_now - ctx->aim_preview_start_wall;
                        
                        // Store progress in game state for renderer (0.0 to 1.0 over 0.3s)
                        ctx->game.aim_preview_progress = (float)(elapsed_wall / 0.3);
                        if (ctx->game.aim_preview_progress > 1.0f) ctx->game.aim_preview_progress = 1.0f;
                        
                        if (elapsed_wall >= 5.0) {
                            // 5 seconds elapsed - execute the shot
                            ctx->aim_preview_active = false;
                            if (ctx->config.verbose) {
                                printf("[DEBUG] Frame %llu: AIM_PREVIEW complete, executing shot for seat %d\n", 
                                       (unsigned long long)ctx->frame_count, ctx->game.turn_seat);
                                fflush(stdout);
                            }
                            
                            // Execute the pre-computed shot plan
                            Seat seat = ctx->game.turn_seat;
                            physics_place_striker(ctx->physics, seat, ctx->game.computed_shot_plan.placement);
                            physics_apply_shot(ctx->physics, ctx->game.computed_shot_plan.aim_angle, ctx->game.computed_shot_plan.power);
                            ctx->game.phase = PHASE_SHOT_EXECUTION;
                            
                            // Log shot plan
                            if (ctx->trace) {
                                trace_write_shot_start(ctx->trace, &ctx->match, &ctx->game, 
                                                       ctx->shot_count, seat, &ctx->game.computed_shot_plan);
                            }
                            ctx->shot_count++;
                            ctx->game.computed_shot_valid = false;
                            ctx->game.aim_preview_progress = 0.0f;
                        }
                    }
                    break;
                    
                case PHASE_SHOT_EXECUTION:
                    if (app_is_shot_settled(ctx)) {
                        ctx->game.phase = PHASE_SETTLING;
                    }
                    break;
                    
                case PHASE_SETTLING:
                    // Double-check settling with re-entry guard
                    if (app_is_shot_settled(ctx)) {
                        ctx->game.phase = PHASE_RESOLVING;
                        ShotResult result = app_collect_shot_result(ctx);
                        app_resolve_shot(ctx, &result);
                    } else {
                        // Re-entered motion (rare) — go back to SHOT_EXECUTION
                        ctx->game.phase = PHASE_SHOT_EXECUTION;
                    }
                    break;
                    
                case PHASE_RESOLVING:
                    // Handled above in app_resolve_shot which sets next phase
                    break;
                    
                default:
                    break;
            }
        }
        
        // Render (mode-specific)
        if (ctx->renderer) {
            int sw = GetScreenWidth();
            int sh = GetScreenHeight();
            Layout L;
            layout_compute(sw, sh, &L);
            
            renderer_begin(ctx->renderer);
            renderer_draw_hud_sidebar(ctx->renderer, &ctx->match, &ctx->game, ctx->playback_speed, &L);
            renderer_begin_board(ctx->renderer);
            float alpha = (float)(ctx->accumulator / PHYSICS_DT);
            if (alpha > 1.0f) alpha = 1.0f;
            renderer_draw_board(ctx->renderer, &ctx->game.board, ctx->physics, alpha, &L, ctx->game.phase, &ctx->game);
            renderer_draw_effects(ctx->renderer, &ctx->game, ctx->placement_timer, &L);
            renderer_end_board(ctx->renderer);
            renderer_draw_placement_banner(ctx->renderer, &ctx->game, ctx->placement_timer, &L);
            renderer_end(ctx->renderer);
            
            // Capture frames if in capture mode
            if (ctx->config.mode == APP_MODE_CAPTURE && ctx->capture_frame_count < ctx->config.frames) {
                renderer_capture_frame(ctx->renderer, ctx->config.capture_dir, ctx->capture_frame_count);
                ctx->capture_frame_count++;
                
                // Check if we've captured enough frames - exit simulation loop
                if (ctx->capture_frame_count >= ctx->config.frames) {
                    if (ctx->config.verbose) {
                        printf("[DEBUG] Capture complete: %lu frames captured\n", (unsigned long)ctx->capture_frame_count);
                        fflush(stdout);
                    }
                    ctx->running = false;
                }
            }
            ctx->frame_count++;
            
            // Hard wall-time budget for capture mode (belt-and-suspenders)
            if (ctx->config.mode == APP_MODE_CAPTURE) {
                double elapsed_wall = platform_time_now() - capture_start_wall;
                if (elapsed_wall > capture_max_wall) {
                    fprintf(stderr, "[ERROR] Capture wall-time budget exceeded: %.2fs > %.2fs (frames=%lu)\n",
                            elapsed_wall, capture_max_wall, (unsigned long)ctx->capture_frame_count);
                    
                    // Write .stall marker file
                    if (ctx->config.capture_dir) {
                        char stall_path[512];
                        snprintf(stall_path, sizeof(stall_path), "%s/.stall", ctx->config.capture_dir);
                        FILE* fp = fopen(stall_path, "w");
                        if (fp) {
                            fprintf(fp, "Capture stalled at frame %lu after %.2f seconds (budget %.2fs)\n",
                                    (unsigned long)ctx->capture_frame_count, elapsed_wall, capture_max_wall);
                            fclose(fp);
                        }
                    }
                    
                    return 74;  // EX_IOERR
                }
            }
        }
        
        // Frame limiting with WaitTime to cap CPU (R5)
        // Target 15 FPS = 66.67ms per frame
        double frame_time = platform_time_now() - now;
        double target_frame_time = 1.0 / 15.0;
        if (frame_time < target_frame_time) {
            platform_sleep_ms((uint32_t)((target_frame_time - frame_time) * 1000));
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
    
    // Initialize playback speed based on mode
    if (config->playback_speed > 0.0f) {
        ctx->playback_speed = config->playback_speed;
    } else {
        // Default based on mode
        switch (config->mode) {
            case APP_MODE_SOAK:
            case APP_MODE_DIAGNOSTIC:
                ctx->playback_speed = 1.0f;
                break;
            case APP_MODE_RENDERED:
            case APP_MODE_CAPTURE:
            default:
                ctx->playback_speed = 0.5f;
                break;
        }
    }
    ctx->speed_paused = false;
    ctx->placement_timer = 0.0;
    ctx->placement_phase_active = false;
    ctx->aim_preview_timer = 0.0;
    ctx->aim_preview_active = false;
    
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
    
    if (ctx->config.verbose) {
        printf("[DEBUG] app_run_capture: target frames=%u\n", ctx->config.frames);
        fflush(stdout);
    }
    
    uint32_t target_frames = ctx->config.frames;
    uint32_t total_frames = 0;
    
    // Run multiple boards until we capture enough frames
    while (total_frames < target_frames) {
        // Ensure running state for each board
        ctx->running = true;
        
        app_init_match(ctx);
        match_start_board(&ctx->match, &ctx->game, &ctx->rng);
        physics_sync_from_board(ctx->physics, &ctx->game.board, ctx->game.turn_seat);
        
        // Run simulation for this board, but stop if we hit target frames
        uint32_t frames_before = ctx->frame_count;
        app_run_simulation(ctx);
        uint32_t frames_this_board = ctx->frame_count - frames_before;
        total_frames += frames_this_board;
        
        if (ctx->config.verbose) {
            printf("[DEBUG] Board complete: captured %u frames this board, total %u/%u\n", 
                   frames_this_board, total_frames, target_frames);
            fflush(stdout);
        }
        
        // If no frames captured, break to avoid infinite loop
        if (frames_this_board == 0) {
            break;
        }
    }
    
    return 0;
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
        } else if ((val = get_arg_value(argv[i], "--playback-speed")) != 0) {
            float speed = strtof(val, NULL);
            if (speed < 0.05f) speed = 0.05f;
            if (speed > 4.0f) speed = 4.0f;
            config.playback_speed = speed;
        } else if ((val = get_arg_value(argv[i], "--ai-budget-ms")) != 0) {
            config.ai_budget_ms = (uint32_t)strtoul(val, NULL, 10);
            if (config.ai_budget_ms < 10) config.ai_budget_ms = 10;
            if (config.ai_budget_ms > 10000) config.ai_budget_ms = 10000;
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
    printf("  --mode <mode>         Mode: rendered, diagnostic, soak, capture (default: rendered)\n");
    printf("  --seed <n>            Master RNG seed (0 = random)\n");
    printf("  --boards <n>          Boards per seed (soak mode, default: 100)\n");
    printf("  --seeds <n>           Number of seeds (soak mode, default: 100)\n");
    printf("  --matches <n>         Matches per board/seed (soak mode, default: 10)\n");
    printf("  --frames <n>          Frames to capture (capture mode, default: 300)\n");
    printf("  --trace-dir <path>    Trace output directory (default: traces)\n");
    printf("  --capture-dir <path>  Capture output directory (default: captures)\n");
    printf("  --playback-speed <x>  Sim speed multiplier 0.05-4.0 (default: 0.05 rendered/capture, 1.0 soak/diagnostic)\n");
    printf("  --ai-budget-ms <n>    AI decision time budget in ms (default: 150, range: 10-10000)\n");
    printf("  --verbose             Verbose logging\n");
    printf("  --headless            Force headless mode\n");
    printf("  --width <n>           Window width (default: 1280)\n");
    printf("  --height <n>          Window height (default: 720)\n");
    printf("  --replay <file>       Replay trace file\n");
    printf("  --help, -h            Show this help\n");
    printf("  --version, -v         Show version\n");
}

void app_print_version(void) {
    printf("Carrom Arena v1.0.0 (Build: %s)\n", BUILD_ID);
}
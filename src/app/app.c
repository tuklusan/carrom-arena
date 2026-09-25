#include <math.h>
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
#include "telemetry/flight.h"
#include "board_view.h"
#include "piece_draw.h"
#include "render/renderer.h"
#include "render/effects.h"
#include "audio/audio.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <assert.h>

/* -----------------------------------------------------------------------------
 * Application Context
 * --------------------------------------------------------------------------- */struct AppContext {
    AppConfig config;
    RNGContext rng;
    MatchState match;
    GameState game;
    PhysicsWorld* physics;
    TraceWriter* trace;
    struct { double at; float speed; } bounce[4];   // striker floor bounces still to be heard
    int bounce_count;
    AudioPolicy audio_policy;      // which sound / how loud / rate limiting
    bool prev_muted;
    FlightRecorder* flight;        // binary flight recorder (per-frame state + events)
    double flight_t0;
    int flight_prev_phase;
    int flight_prev_seat;
    float flight_prev_speed;
    bool flight_prev_paused;
    float flight_prev_layout[3];
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
    bool striker_fall_registered;    // striker pocket animation already started for this shot
    int pockets_registered;          // Pockets of the running shot already registered in game state
    float next_progress_time;        // Sim time of the next SHOT_PROGRESS trace record
};

/* -----------------------------------------------------------------------------
 * Helpers
 * --------------------------------------------------------------------------- */
static void app_init_controllers(AppContext* ctx) {
    for (int i = 0; i < 4; i++) {
        const StrategyProfile* profile = strategy_by_index(seat_to_strategy((Seat)i));
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
        char flight_path[512];
        snprintf(flight_path, sizeof(flight_path), "%s/flight_%llu.bin",
                 ctx->config.trace_dir, (unsigned long long)ctx->rng.master_seed);
        ctx->flight = flight_open(flight_path, ctx->rng.master_seed);
        ctx->flight_t0 = platform_time_now();
        ctx->flight_prev_phase = -1;
        ctx->flight_prev_seat = -1;
        ctx->flight_prev_speed = -1.0f;
        if (ctx->flight) {
            flight_write_event(ctx->flight, 0.0, 0.0f, FLIGHT_EV_START, (float)(ctx->rng.master_seed & 0xFFFFFFu),
                               (float)ctx->config.window_width, (float)ctx->config.window_height, 0.0f);
        }
    }
}

static void app_setup_renderer(AppContext* ctx) {
    if (ctx->config.mode == APP_MODE_RENDERED) {
        if (!ctx->config.headless) {
            ctx->renderer = renderer_create(ctx->config.window_width, ctx->config.window_height, 
                                             "Carrom Arena", false, false, ctx->config.debug_phase, ctx->playback_speed);
            audio_init();   /* silent no-op when there is no audio device */
        }
    } else if (ctx->config.mode == APP_MODE_CAPTURE) {
        // Capture mode always needs a renderer (windowed or hidden)
        ctx->renderer = renderer_create(ctx->config.window_width, ctx->config.window_height, 
                                         "Carrom Arena", true, ctx->config.headless, ctx->config.debug_phase, ctx->playback_speed);
    }
}

/* -----------------------------------------------------------------------------
 * Core Simulation Step (Fixed Timestep)
 * --------------------------------------------------------------------------- */
// Use physics.h definitions: PHYSICS_HZ, PHYSICS_DT, MAX_SUBSTEPS

#define AIM_PREVIEW_SECONDS 2.0   /* the launch line + arrow are shown for 2 s before the shot */

/* The configured game speed (default 1x) applies only from striker LAUNCH until the board
 * SETTLES. Thinking, placement and aim preview always run at full (1x) speed. */
static double app_phase_speed(const AppContext* ctx) {
    if (ctx->game.phase == PHASE_SHOT_EXECUTION || ctx->game.phase == PHASE_SETTLING) {
        return (double)ctx->playback_speed;
    }
    return 1.0;
}

static void app_simulation_step(AppContext* ctx, double dt) {
    // Apply playback speed: at 0.5x, 1 wall-clock second advances 0.5s of simulation
    // Space key pause sets speed_paused=true, which forces dt=0
    if (ctx->speed_paused) {
        dt = 0.0;
    }
    ctx->accumulator += dt * app_phase_speed(ctx);

    // Cap accumulated time to prevent spiral of death and lag above 0.5x playback
    // Max substeps (4) at 1/120s is ~0.033s. Capping at 0.25s drops excess.
    if (ctx->accumulator > 0.25) {
        ctx->accumulator = 0.25;
    }
    
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

#define FLIGHT_EVENT(ctx, kind, a, b, c, d) \
    do { if ((ctx)->flight) flight_write_event((ctx)->flight, platform_time_now() - (ctx)->flight_t0, \
         (ctx)->physics ? physics_get_sim_time((ctx)->physics) : 0.0f, (kind), (float)(a), (float)(b), (float)(c), (float)(d)); } while (0)

/* One FRAME record per rendered frame: physics state of everything, and what the renderer actually drew. */
static void app_flight_frame(AppContext* ctx, float alpha, double frame_dt) {
    if (!ctx->flight) return;
    double wall = platform_time_now() - ctx->flight_t0;

    /* change events */
    if ((int)ctx->game.phase != ctx->flight_prev_phase) {
        FLIGHT_EVENT(ctx, FLIGHT_EV_PHASE, ctx->flight_prev_phase, ctx->game.phase, ctx->game.turn_seat, 0);
        ctx->flight_prev_phase = (int)ctx->game.phase;
    }
    if ((int)ctx->game.turn_seat != ctx->flight_prev_seat) {
        FLIGHT_EVENT(ctx, FLIGHT_EV_TURN, ctx->game.turn_seat, ctx->game.active_player.team, 0, 0);
        ctx->flight_prev_seat = (int)ctx->game.turn_seat;
    }
    if (fabsf(ctx->playback_speed - ctx->flight_prev_speed) > 1e-6f) {
        FLIGHT_EVENT(ctx, FLIGHT_EV_SPEED, ctx->playback_speed, 0, 0, 0);
        ctx->flight_prev_speed = ctx->playback_speed;
    }
    if (ctx->paused != ctx->flight_prev_paused) {
        FLIGHT_EVENT(ctx, FLIGHT_EV_PAUSE, ctx->paused ? 1 : 0, 0, 0, 0);
        ctx->flight_prev_paused = ctx->paused;
    }
    Layout L = renderer_get_layout(ctx->renderer);
    if (fabsf((float)L.board_x - ctx->flight_prev_layout[0]) > 0.5f || fabsf((float)L.board_y - ctx->flight_prev_layout[1]) > 0.5f ||
        fabsf((float)L.board_size - ctx->flight_prev_layout[2]) > 0.5f) {
        FLIGHT_EVENT(ctx, FLIGHT_EV_LAYOUT, L.board_x, L.board_y, L.board_size, L.sw);
        ctx->flight_prev_layout[0] = (float)L.board_x;
        ctx->flight_prev_layout[1] = (float)L.board_y;
        ctx->flight_prev_layout[2] = (float)L.board_size;
    }

    FlightFrame f;
    memset(&f, 0, sizeof(f));
    f.wall = wall;
    f.frame = ctx->frame_count;
    f.sim_time = physics_get_sim_time(ctx->physics);
    f.playback_speed = ctx->playback_speed;
    f.alpha = alpha;
    f.frame_dt = (float)frame_dt;
    f.placement_timer = (float)ctx->placement_timer;
    f.thinking_timer = (float)ctx->thinking_timer;
    f.aim_timer = (float)ctx->aim_preview_timer;
    f.phase = (uint8_t)ctx->game.phase;
    f.turn_seat = (uint8_t)ctx->game.turn_seat;

    BoardViewDebug dbg;
    board_view_get_debug(&dbg);
    Vec2 sp, sv;
    physics_get_striker_position(ctx->physics, &sp);
    physics_get_striker_velocity(ctx->physics, &sv);
    f.striker_pos[0] = sp.x; f.striker_pos[1] = sp.y;
    f.striker_vel[0] = sv.x; f.striker_vel[1] = sv.y;
    f.striker_vis[0] = dbg.striker_vis.x; f.striker_vis[1] = dbg.striker_vis.y;
    for (int i = 0; i < 4; i++) f.figures[i] = dbg.figures[i];
    f.aim_angle = ctx->game.computed_shot_plan.aim_angle;
    f.aim_power = ctx->game.computed_shot_plan.power;
    if (dbg.aim_drawn) {
        f.aim_line[0] = dbg.aim_start.x; f.aim_line[1] = dbg.aim_start.y;
        f.aim_line[2] = dbg.aim_end.x;   f.aim_line[3] = dbg.aim_end.y;
    }
    f.layout[0] = (float)L.board_x; f.layout[1] = (float)L.board_y; f.layout[2] = (float)L.board_size;
    f.layout[3] = (float)L.sw;      f.layout[4] = (float)L.sh;
    f.score_white = (uint16_t)ctx->match.games_won_white;
    f.score_black = (uint16_t)ctx->match.games_won_black;

    unsigned int falling = effects_falling_mask();
    Vec2 pos[MAX_PIECES];
    physics_get_positions(ctx->physics, pos);
    _Static_assert(FLIGHT_PIECES == MAX_PIECES, "the flight recorder records every piece");
    for (int i = 0; i < FLIGHT_PIECES; i++) {
        FlightPiece* p = &f.piece[i];
        Vec2 v;
        bool alive = physics_get_piece_velocity(ctx->physics, i, &v);
        p->x = alive ? pos[i].x : ctx->game.board.pieces[i].position.x;
        p->y = alive ? pos[i].y : ctx->game.board.pieces[i].position.y;
        p->vx = v.x; p->vy = v.y;
        p->flags = (uint8_t)((ctx->game.board.pieces[i].on_board ? 1 : 0) | (ctx->game.board.pieces[i].pocketed ? 2 : 0) |
                             (alive ? 4 : 0) | (((falling >> i) & 1u) ? 8 : 0) | (((dbg.drawn_from_physics_mask >> i) & 1u) ? 16 : 0));
    }
    f.flags = (uint8_t)((ctx->paused ? 1 : 0) | (dbg.aim_drawn ? 2 : 0) | (physics_is_striker_pocketed(ctx->physics) ? 4 : 0) |
                        (ctx->game.board.striker.on_baseline ? 8 : 0) | (dbg.striker_valid ? 16 : 0));
    for (int i = 0; i <= MAX_PIECES; i++) if ((falling >> i) & 1u) f.n_falling++;
    flight_write_frame(ctx->flight, &f);
}

/* Play a cue through the audio policy (loudness, variant rotation, rate limit) and log it in the flight recorder. */
static void app_cue(AppContext* ctx, AudioCue cue, float speed) {
    float volume;
    int variant;
    double now = platform_time_now();
    if (!audio_policy_admit(&ctx->audio_policy, cue, speed, now, audio_variant_count(cue), &volume, &variant)) return;
    audio_play(cue, volume, variant);
    FLIGHT_EVENT(ctx, FLIGHT_EV_SOUND, cue, speed, volume, variant);
    if (cue == CUE_STRIKER_POCKET && ctx->bounce_count == 0) {
        /* the striker dropped through the pocket to the floor: two bounces, each quieter */
        ctx->bounce[0].at = now + 0.32; ctx->bounce[0].speed = 1.5f;
        ctx->bounce[1].at = now + 0.58; ctx->bounce[1].speed = 0.7f;
        ctx->bounce_count = 2;
    }
}

/* Drain the sounds physics recorded since the last frame and play them. */
static void app_play_sounds(AppContext* ctx) {
    SoundEvent ev[64];
    int n;
    for (int i = 0; i < ctx->bounce_count; ) {
        if (platform_time_now() >= ctx->bounce[i].at) {
            app_cue(ctx, CUE_STRIKER_BOUNCE, ctx->bounce[i].speed);
            ctx->bounce[i] = ctx->bounce[--ctx->bounce_count];
        } else {
            i++;
        }
    }
    while ((n = physics_drain_sound_events(ctx->physics, ev, 64)) > 0) {
        for (int i = 0; i < n; i++) {
            app_cue(ctx, audio_cue_for_sound_kind(ev[i].kind), ev[i].speed);
            if (ev[i].kind == SOUND_COIN_POCKET && ev[i].piece_id == QUEEN_ID) app_cue(ctx, CUE_QUEEN, 1.0f);
        }
    }
    if (audio_is_muted() != ctx->prev_muted) {
        ctx->prev_muted = audio_is_muted();
        FLIGHT_EVENT(ctx, FLIGHT_EV_MUTE, ctx->prev_muted ? 1 : 0, 0, 0, 0);
    }
}

/* Place a freshly pocketed piece in its 3x3 corner slot outside the board (game state only). */
static void app_register_pocket(AppContext* ctx, uint8_t piece_id, uint8_t pocket_idx) {
    if (piece_id >= MAX_PIECES || pocket_idx > 3) return;
    BoardState* board = &ctx->game.board;
    board->pieces[piece_id].on_board = false;
    board->pieces[piece_id].pocketed = true;

    /* Stash: a 3x3 lineup of non-overlapping coins in the outside corner next to the pocket, growing away from
     * the board. Pockets: 0=top-left (NW), 1=top-right (NE), 2=bottom-left (SW), 3=bottom-right (SE). */
    int in_this_pocket = 0;
    for (int q = 0; q < board->pocketed_count; q++) {
        if (board->pocketed_pieces[q].pocket_index == pocket_idx) in_this_pocket++;
    }
    int slot = board->pocketed_count;
    if (slot >= MAX_PIECES) return;
    board->pieces[piece_id].pocketed_position = pocket_stash_position(pocket_idx, in_this_pocket);
    board->pieces[piece_id].pocket_index = pocket_idx;
    board->pocketed_pieces[slot] = board->pieces[piece_id];
    board->pocketed_count++;
}

static void app_snapshot_trace(AppContext* ctx, bool interrupted) {
    if (!ctx->trace) return;
    Vec2 pos[MAX_PIECES], vel[MAX_PIECES], spos, svel;
    bool alive[MAX_PIECES];
    physics_get_positions(ctx->physics, pos);
    for (int i = 0; i < MAX_PIECES; i++) alive[i] = physics_get_piece_velocity(ctx->physics, i, &vel[i]);
    physics_get_striker_position(ctx->physics, &spos);
    physics_get_striker_velocity(ctx->physics, &svel);
    ShotResult r = {0};
    physics_collect_pocketed(ctx->physics, &r);
    const char* phase = ctx->game.phase == PHASE_SETTLING ? "SETTLING" : "SHOT_EXECUTION";
    uint64_t shot = ctx->shot_count ? ctx->shot_count - 1 : 0;
    trace_write_shot_snapshot(ctx->trace, interrupted, shot, physics_get_sim_time(ctx->physics), phase,
                              &spos, &svel, pos, vel, alive, r.pocketed_ids, r.pocketed_count);
}

/* Called every frame while a shot runs: register new pockets at once, trace progress. */
static void app_shot_progress(AppContext* ctx) {
    ShotResult r = {0};
    physics_collect_pocketed(ctx->physics, &r);
    float t = physics_get_sim_time(ctx->physics);
    uint64_t shot = ctx->shot_count ? ctx->shot_count - 1 : 0;
    for (int i = ctx->pockets_registered; i < r.pocketed_count; i++) {
        app_register_pocket(ctx, r.pocketed_ids[i], r.pocketed_pocket_indices[i]);
        {
            Vec2 lp, lv;
            physics_get_pocketed_last(ctx->physics, i, &lp, &lv);
            FLIGHT_EVENT(ctx, FLIGHT_EV_POCKET, r.pocketed_ids[i], r.pocketed_pocket_indices[i], sqrtf(lv.x * lv.x + lv.y * lv.y), t);
            FLIGHT_EVENT(ctx, FLIGHT_EV_STASH, r.pocketed_ids[i], r.pocketed_pocket_indices[i],
                         ctx->game.board.pieces[r.pocketed_ids[i]].pocketed_position.x,
                         ctx->game.board.pieces[r.pocketed_ids[i]].pocketed_position.y);
        }
        if (ctx->renderer) {
            Vec2 lp, lv;
            physics_get_pocketed_last(ctx->physics, i, &lp, &lv);
            effects_trigger_pocket_fall(r.pocketed_ids[i], (PieceColor)r.pocketed_colors[i], lp, lv, r.pocketed_pocket_indices[i]);
            effects_trigger_pocket_fade(r.pocketed_pocket_indices[i]);
        }
        if (ctx->config.verbose) {
            printf("[POCKET] id=%d pocket=%d\n", (int)r.pocketed_ids[i], (int)r.pocketed_pocket_indices[i]);
            fflush(stdout);
        }
        if (ctx->trace) {
            trace_write_pocket(ctx->trace, shot, r.pocketed_ids[i], r.pocketed_colors[i],
                               r.pocketed_pocket_indices[i], t);
        }
    }
    ctx->pockets_registered = r.pocketed_count;
    if (ctx->renderer && !ctx->striker_fall_registered) {
        Vec2 sp, sv;
        int spocket;
        if (physics_get_striker_pocket_info(ctx->physics, &sp, &sv, &spocket)) {
            ctx->striker_fall_registered = true;
            FLIGHT_EVENT(ctx, FLIGHT_EV_STRIKER_POCKET, spocket, sqrtf(sv.x * sv.x + sv.y * sv.y), 0, 0);
            effects_trigger_pocket_fall(EFFECTS_STRIKER_ID, PIECE_STRIKER, sp, sv, spocket);
            if (ctx->config.verbose) { printf("[POCKET] striker pocket=%d\n", spocket); fflush(stdout); }
            effects_trigger_pocket_fade_long(spocket, 0.9f);
        }
    }
    if (t >= ctx->next_progress_time) {
        app_snapshot_trace(ctx, false);
        ctx->next_progress_time = t + 2.0f;
    }
}

static void app_resolve_shot(AppContext* ctx, const ShotResult* result) {
    // Extract facts for rules engine
    ShotFacts facts;
    match_extract_facts(&ctx->game, result, &facts);
    
    // Resolve through rules engine
    RulesOutcome outcome = rules_resolve(&ctx->match, &ctx->game, &facts);
    
    // Apply outcome to match and game states
    ctx->game = outcome.next_game_state;
    ctx->match = outcome.next_match_state;
    // The coins have moved: the game state (what the AI plans from) must show where they really are now
    board_apply_final_positions(&ctx->game.board, result->final_positions);

    FLIGHT_EVENT(ctx, FLIGHT_EV_SHOT_END, (int)outcome.turn_decision, result->pocketed_count, result->striker_pocketed ? 1 : 0, result->sim_time);

    if (facts.fouls != FOUL_NONE) app_cue(ctx, CUE_FOUL, 1.0f);
    if (outcome.turn_decision == TURN_BOARD_OVER) app_cue(ctx, CUE_BOARD_WON, 1.0f);

    // Register any pockets not already registered mid-shot (idempotent)
    for (int i = ctx->pockets_registered; i < result->pocketed_count; i++) {
        app_register_pocket(ctx, result->pocketed_ids[i], result->pocketed_pocket_indices[i]);
    }
    ctx->pockets_registered = 0;
    ctx->striker_fall_registered = false;
    ctx->next_progress_time = 0.0f;

    // Fresh striker for the next turn (a pocketed striker is a foul, but the next player still gets one)
    striker_state_init(&ctx->game.board.striker, ctx->game.turn_seat);
    board_place_striker_on_baseline(&ctx->game.board.striker, ctx->game.turn_seat);
    // The rules may have put the queen back on the board (she was not covered): put her back in the physics world too
    if (ctx->game.board.pieces[QUEEN_ID].on_board && physics_is_piece_pocketed(ctx->physics, QUEEN_ID)) {
        physics_sync_from_board(ctx->physics, &ctx->game.board, ctx->game.turn_seat);
    }

    // Set phase for next turn based on turn decision
    switch (outcome.turn_decision) {
        case TURN_CONTINUE:
        case TURN_ADVANCE:
            // Reset physics turn timer so settle detection starts fresh for next turn
            physics_reset_turn_timer(ctx->physics);
            ctx->game.phase = PHASE_THINKING;
            ctx->thinking_phase_active = true;
            ctx->thinking_timer = 0.0;
            ctx->candidates_evaluated = 0;
            ctx->pending_shot_valid = false;
            // Minimum thinking time for visualization (2s for verification on subsequent turns)
            double budget_sec = (ctx->config.ai_budget_ms > 0) ? (ctx->config.ai_budget_ms / 1000.0) : 0.15;
            ctx->thinking_min_wall = budget_sec * 0.2;
            if (ctx->thinking_min_wall < 2.0) ctx->thinking_min_wall = 2.0;  // At least 2 seconds for verification
            break;
        case TURN_BOARD_OVER:
            // Set to THINKING for the new board immediately to avoid a frame hole
            ctx->game.phase = PHASE_THINKING;
            ctx->thinking_phase_active = true;
            ctx->thinking_timer = 0.0;
            ctx->candidates_evaluated = 0;
            ctx->pending_shot_valid = false;
            
            // Set minimum thinking time for new board visualization
            double budget_sec_new = (ctx->config.ai_budget_ms > 0) ? (ctx->config.ai_budget_ms / 1000.0) : 0.15;
            ctx->thinking_min_wall = budget_sec_new * 0.2;
            if (ctx->thinking_min_wall < 0.5) ctx->thinking_min_wall = 0.5; 
            // Thinking min wall is now treated as a duration in seconds, not an absolute timestamp
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
    
    // Start next board if needed
    if (outcome.turn_decision == TURN_BOARD_OVER) {
        if (!match_is_over(&ctx->match)) {
            match_start_board(&ctx->match, &ctx->game, &ctx->rng);
            physics_sync_from_board(ctx->physics, &ctx->game.board, ctx->game.turn_seat);
            // match_start_board leaves the phase at PLACEMENT and the old plan in place: without this the new board skipped
            // THINKING and its first player aimed with the previous board's last plan (a line pointing off the board).
            physics_reset_turn_timer(ctx->physics);
            ctx->game.phase = PHASE_THINKING;
            ctx->game.computed_shot_valid = false;
            ctx->game.aim_preview_progress = 0.0f;
            ctx->thinking_phase_active = true;
            ctx->thinking_timer = 0.0;
            ctx->pending_shot_valid = false;
            ctx->aim_preview_active = false;
            ctx->placement_phase_active = false;
            ctx->pockets_registered = 0;
            ctx->striker_fall_registered = false;
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
    // First turn: 0.5s for figure flash sync; subsequent turns use 2.0s for verification
    double budget_sec = (ctx->config.ai_budget_ms > 0) ? (ctx->config.ai_budget_ms / 1000.0) : 0.15;
    ctx->thinking_min_wall = budget_sec * 0.2;
    if (ctx->thinking_min_wall < 0.5) ctx->thinking_min_wall = 0.5;  // First turn: at least 0.5s for figure flash sync
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
        }

        if (ctx->renderer) {
            float r_speed = renderer_get_playback_speed(ctx->renderer);
            if (fabsf(r_speed - ctx->playback_speed) > 1e-6f) {
                ctx->playback_speed = r_speed;
            }
        }
        ctx->speed_paused = ctx->paused;
        
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
            if (ctx->renderer) effects_update((float)(dt * app_phase_speed(ctx)));
            app_play_sounds(ctx);
            if (ctx->game.phase == PHASE_SHOT_EXECUTION || ctx->game.phase == PHASE_SETTLING) {
                app_shot_progress(ctx);
            }
            
            // Diagnostic: trace physics state during shot execution and settling
            if (ctx->trace && ctx->config.verbose && 
                (ctx->game.phase == PHASE_SHOT_EXECUTION || ctx->game.phase == PHASE_SETTLING)) {
                Vec2 striker_vel, striker_pos;
                physics_get_striker_velocity(ctx->physics, &striker_vel);
                physics_get_striker_position(ctx->physics, &striker_pos);
                trace_write_physics_state(ctx->trace, ctx->frame_count, ctx->shot_count,
                                          physics_get_sim_time(ctx->physics), &striker_vel, &striker_pos,
                                          ctx->game.phase == PHASE_SHOT_EXECUTION ? "SHOT_EXECUTION" : "SETTLING");
            }
            
            // State machine
            // Phase invariants (debug assertions)
            assert(!(ctx->game.phase == PHASE_AIM_PREVIEW && ctx->game.computed_shot_valid && 
                     (ctx->game.board.striker.velocity.x != 0.0f || ctx->game.board.striker.velocity.y != 0.0f)));
            assert(!(ctx->game.phase == PHASE_SHOT_EXECUTION && ctx->game.computed_shot_valid));
            assert(!(ctx->game.phase == PHASE_PLACEMENT && !ctx->game.board.striker.on_baseline));
            assert(!(ctx->game.phase == PHASE_THINKING && ctx->pending_shot_valid && ctx->aim_preview_active));
            
            switch (ctx->game.phase) {
                case PHASE_IDLE:
                    break;
                    
                case PHASE_THINKING:
                    // AI thinking phase: compute shot plan
                    if (ctx->thinking_phase_active) {
                        ctx->thinking_timer += dt * app_phase_speed(ctx);
                        
                        // Check for transition: using scaled thinking_timer instead of wall time
                        if (ctx->pending_shot_valid && ctx->thinking_timer >= 2.0) {
                            ctx->thinking_phase_active = false;
                            // Put the striker (physics + game state) exactly where the plan places it, so the
                            // drawn striker, the aim line and the launch all start from the same spot.
                            physics_place_striker(ctx->physics, ctx->game.turn_seat, ctx->pending_shot_plan.placement);
                            ctx->game.board.striker.position = ctx->pending_shot_plan.placement;
                            ctx->game.board.striker.velocity = (Vec2){0.0f, 0.0f};
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
                            FLIGHT_EVENT(ctx, FLIGHT_EV_PLAN, plan.aim_angle, plan.power, plan.placement.x, plan.placement.y);
                            if (ctx->flight) {
                                char note[160];
                                snprintf(note, sizeof(note), "plan seat=%d tactic=%d aim=%.4f power=%.3f placement=(%.4f,%.4f)",
                                         (int)seat, (int)plan.tactic, plan.aim_angle, plan.power, plan.placement.x, plan.placement.y);
                                flight_write_text(ctx->flight, platform_time_now() - ctx->flight_t0, note);
                            }
                            
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
                        ctx->placement_timer = 1.0;
                        ctx->placement_phase_active = true;
                        if (ctx->config.verbose) {
                            printf("[DEBUG] Frame %llu: Placement phase started for seat %d, timer=%.2fs\n", 
                                   (unsigned long long)ctx->frame_count, ctx->game.turn_seat, ctx->placement_timer);
                            fflush(stdout);
                        }
                    }
                    
                    // Count down timer (only if not paused)
                    if (!ctx->paused && !ctx->speed_paused) {
                        ctx->placement_timer -= dt * app_phase_speed(ctx);
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
                        // computed_shot_valid will be set to true when AIM_PREVIEW phase starts
                        
                        // Start AIM_PREVIEW phase (5 seconds scaled time)
                        ctx->aim_preview_timer = AIM_PREVIEW_SECONDS;
                        ctx->aim_preview_active = true;
                        ctx->game.phase = PHASE_AIM_PREVIEW;
                        ctx->pending_shot_valid = false;
                    }
                    break;
                    
                case PHASE_AIM_PREVIEW:
                    // AIM_PREVIEW phase: timer scaled by playback_speed
                    if (ctx->aim_preview_active) {
                        // Use playback_speed for timing
                        ctx->aim_preview_timer -= dt * app_phase_speed(ctx);
                        
                        // Set computed_shot_valid true on first frame of AIM_PREVIEW
                        if (!ctx->game.computed_shot_valid) {
                            ctx->game.computed_shot_valid = true;
                        }
                        
                        // Progress for renderer (0.0 to 1.0 over 0.3s)
                        // Based on inverse of timer: timer starts at 5.0, ends at 0.0
                        //’s progress = (5.0 - timer) / 0.3
                        double elapsed_scaled = AIM_PREVIEW_SECONDS - ctx->aim_preview_timer;
                        ctx->game.aim_preview_progress = (float)(elapsed_scaled / 0.3);
                        if (ctx->game.aim_preview_progress > 1.0f) ctx->game.aim_preview_progress = 1.0f;
                        if (ctx->game.aim_preview_progress < 0.0f) ctx->game.aim_preview_progress = 0.0f;
                        
                        if (ctx->aim_preview_timer <= 0.0) {
                            // 5 seconds elapsed (scaled) - execute the shot
                            ctx->aim_preview_active = false;
                            if (ctx->config.verbose) {
                                printf("[DEBUG] Frame %llu: AIM_PREVIEW complete, executing shot for seat %d\n", 
                                       (unsigned long long)ctx->frame_count, ctx->game.turn_seat);
                                fflush(stdout);
                            }
                            
                            // FIRST: Invalidate computed shot so aim line clears BEFORE striker gains velocity
                            ctx->game.computed_shot_valid = false;
                            ctx->game.aim_preview_progress = 0.0f;
                            
                            // THEN: Execute the pre-computed shot plan
                            Seat seat = ctx->game.turn_seat;
                            physics_place_striker(ctx->physics, seat, ctx->game.computed_shot_plan.placement);
                            physics_apply_shot(ctx->physics, ctx->game.computed_shot_plan.aim_angle, ctx->game.computed_shot_plan.power);
                            
                            // Fix launch speed burst: reset accumulator to 0.
                            // This ensures the first rendered frame of the shot is precisely the 
                            // start of the motion, without jumping ahead by several physics steps.
                            ctx->accumulator = 0.0;
                            
                            ctx->game.phase = PHASE_SHOT_EXECUTION;
                            
                            // Log shot plan
                            if (ctx->trace) {
                                trace_write_shot_start(ctx->trace, &ctx->match, &ctx->game, 
                                                       ctx->shot_count, seat, &ctx->game.computed_shot_plan);
                            }
                            FLIGHT_EVENT(ctx, FLIGHT_EV_SHOT_START, ctx->shot_count, seat, ctx->game.computed_shot_plan.aim_angle, ctx->game.computed_shot_plan.power);
                            ctx->shot_count++;
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
                        // Consume pocketed pieces so physics doesn't accumulate them across shots
                        physics_consume_pocketed(ctx->physics);
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
            renderer_set_turn_team(ctx->renderer, ctx->game.active_player.team);
            renderer_begin(ctx->renderer);
            renderer_begin_board(ctx->renderer);
            float alpha = (float)(ctx->accumulator / PHYSICS_DT);
            if (alpha > 1.0f) alpha = 1.0f;
            renderer_draw_board(ctx->renderer, &ctx->game.board, ctx->physics, alpha, ctx->game.phase, &ctx->game, ctx->placement_timer);
            renderer_draw_effects(ctx->renderer, &ctx->game, ctx->placement_timer);
            renderer_end_board(ctx->renderer);
            renderer_end(ctx->renderer);
            app_flight_frame(ctx, alpha, dt);

            // Capture frames if in capture mode
            if (ctx->config.mode == APP_MODE_CAPTURE && ctx->capture_frame_count < ctx->config.frames) {
                renderer_capture_frame(ctx->renderer, ctx->config.capture_dir, ctx->capture_frame_count,
                                       ctx->game.phase, ctx->placement_timer, ctx->playback_speed, &ctx->game.board);
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
                        FILE* fp = platform_fopen_private(stall_path, "w");
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
        // Target 60 FPS
        double frame_time = platform_time_now() - now;
        double target_frame_time = 1.0 / 60.0;
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
    
    if (config->playback_speed > 0.0f) {
        ctx->playback_speed = config->playback_speed;
    } else {
        // Default to real time (1x)
        ctx->playback_speed = 1.0f;
    }
    printf("[DEBUG] App created with playback_speed=%.2fx\n", ctx->playback_speed);
    fflush(stdout);
    ctx->speed_paused = false;
    audio_policy_init(&ctx->audio_policy);
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
    
    if (ctx->flight) {
        FLIGHT_EVENT(ctx, FLIGHT_EV_CLOSE, ctx->game.phase, 0, 0, 0);
        flight_close(ctx->flight);
        ctx->flight = NULL;
    }
    if (ctx->trace) {
        if (ctx->game.phase == PHASE_SHOT_EXECUTION || ctx->game.phase == PHASE_SETTLING) {
            app_snapshot_trace(ctx, true);
        }
        trace_close(ctx->trace);
    }
    
    audio_shutdown();
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
        } else if (strcmp(argv[i], "--debug-phase") == 0) {
            config.debug_phase = true;
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
    printf("  --playback-speed <x>  Sim speed multiplier 0.05-4.0 (default: 1.0)\n");
    printf("  --ai-budget-ms <n>    AI decision time budget in ms (default: 150, range: 10-10000)\n");
    printf("  --verbose             Verbose logging\n");
    printf("  --headless            Force headless mode\n");
    printf("  --debug-phase         Enable per-frame phase debug logging in capture mode\n");
    printf("  --width <n>           Window width (default: 800)\n");
    printf("  --height <n>          Window height (default: 560)\n");
    printf("  --help, -h            Show this help\n");
    printf("  --version, -v         Show version\n");
}

void app_print_version(void) {
    printf("Carrom Arena v1.0.0 (Build: %s)\n", BUILD_ID);
    printf("Based on original work by Supratim Sanyal of SANYALnet Labs.\n");
    printf("SANYALnet Labs Non-Commercial License; see the LICENSE file.\n");
}
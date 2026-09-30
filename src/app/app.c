#include <math.h>
#include "app.h"
#include "game/striker_path.h"
#include "common/types.h"
#include "common/rng.h"
#include "common/strategy_profiles.h"
#include "platform/platform.h"
#include "audio/radio.h"
#include "game/match.h"
#include "game/board.h"
#include "game/scoring.h"
#include "game/rules.h"
#include "physics/physics.h"
#include "ai/controller.h"
#include "telemetry/trace.h"
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
    int score_base[2];       // red (N/S) and blue (E/W) points of the finished boards THIS GAME; the live board's coins are added on top
    int pair_boards_won[2];  // red, blue: boards won THIS GAME, resolved per board (ICF 43: colour is per-board, so a colour-keyed
                             // count would misattribute a board once the breaker, and so the colour, rotates to the other pair)
    int total_games[2];    // games won
    struct { double at; float speed; } bounce[4];   // striker floor bounces still to be heard
    int bounce_count;
    AudioPolicy audio_policy;      // which sound / how loud / rate limiting
    bool prev_muted;
    /* App-level event tracking (2026-09-28): these replace the separate binary flight recorder's own
     * previous-value tracking, now folded into the shared trace file as APP_EVENT records - see
     * trace_write_app_event() in telemetry/trace.c and the APP_EVENT() macro below. */
    int app_ev_prev_phase;
    int app_ev_prev_seat;
    float app_ev_prev_speed;
    bool app_ev_prev_paused;
    float app_ev_prev_layout[3];
    Renderer* renderer;
    Controller* controllers[4];  // One per seat
    uint64_t frame_count;
    uint64_t shot_count;
    bool running;
    bool paused;
    double last_frame_time;
    double accumulator;
    float playback_speed;  // Current playback speed multiplier
    bool speed_paused;     // Temporary pause from space key (speed = 0)
    double placement_timer;  // Timer for striker placement phase (seconds)
    bool placement_phase_active;  // Whether we're in the placement hold phase
    double arrange_wait;          // Wall seconds left while the coins slide into their starting places (nothing else plays)
    
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
    int striker_fall_pocket;         // the pocket the striker fell into (where it slides back from)
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
    match_randomize_first_breaker(&ctx->match, &ctx->rng);   /* arena kickoff: a real coin toss for who breaks first */
    game_state_init(&ctx->game, ctx->rng.master_seed);
    ctx->game.turn_seat = SEAT_NORTH;
    ctx->frame_count = 0;
    ctx->shot_count = 0;
    
    // Set max candidates based on mode (R5)
    // For rendered mode, scale candidates with ai_budget_ms to make THINKING phase visible
    if (ctx->config.mode == APP_MODE_RENDERED) {
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

/* App-level event kinds (2026-09-28): what used to be the separate binary flight recorder's own APP_EV_* enum,
 * now written as APP_EVENT JSONL records into the shared trace file instead (trace_write_app_event()). Kept as the
 * same small integers for continuity; app_ev_kind_name() below is what makes each record self-describing. */
enum {
    APP_EV_START = 1, APP_EV_PHASE = 2, APP_EV_PLAN = 3, APP_EV_SHOT_START = 4, APP_EV_POCKET = 5,
    APP_EV_STRIKER_POCKET = 6, APP_EV_SHOT_END = 7, APP_EV_SPEED = 8, APP_EV_PAUSE = 9, APP_EV_STASH = 10,
    APP_EV_TURN = 11, APP_EV_CLOSE = 12, APP_EV_LAYOUT = 13, APP_EV_SOUND = 14, APP_EV_MUTE = 15
};

static const char* app_ev_kind_name(int kind) {
    switch (kind) {
        case APP_EV_START:          return "START";
        case APP_EV_PHASE:          return "PHASE";
        case APP_EV_PLAN:           return "PLAN";
        case APP_EV_SHOT_START:     return "SHOT_START";
        case APP_EV_POCKET:         return "POCKET";
        case APP_EV_STRIKER_POCKET: return "STRIKER_POCKET";
        case APP_EV_SHOT_END:       return "SHOT_END";
        case APP_EV_SPEED:          return "SPEED";
        case APP_EV_PAUSE:          return "PAUSE";
        case APP_EV_STASH:          return "STASH";
        case APP_EV_TURN:           return "TURN";
        case APP_EV_CLOSE:          return "CLOSE";
        case APP_EV_LAYOUT:         return "LAYOUT";
        case APP_EV_SOUND:          return "SOUND";
        case APP_EV_MUTE:           return "MUTE";
        default:                    return "UNKNOWN";
    }
}

#define APP_EVENT(ctx, kind, a, b, c, d) \
    do { if ((ctx)->trace) trace_write_app_event((ctx)->trace, (kind), app_ev_kind_name(kind), \
         (ctx)->physics ? physics_get_sim_time((ctx)->physics) : 0.0f, \
         (float)(a), (float)(b), (float)(c), (float)(d)); } while (0)

static void app_setup_trace(AppContext* ctx) {
    if (ctx->config.trace_dir) {
        platform_mkdir(ctx->config.trace_dir);

        /* ONE FILE, REUSED FOR EVERY RUN, AND NO OTHER FILES (2026-09-28, the operator: "we must have EXACTLY ONE
         * TRACE FILE that captures everything you need to debug... NO ADDITIONAL FILES"). Three separate files
         * used to exist alongside this one - a binary flight recorder (flight_<seed>.bin, needing its own
         * flight_dump tool to read), a plain-text debug log (debug_<seed>.log), and trace.c's own human-readable
         * mirror (seed_<seed>.log) - all now folded into this single JSONL ring: platform_diag_set_sink() below
         * routes every platform_diag_logf() call (raylib's own log, per-frame phase notes) here as LOG records,
         * and APP_EVENT() (this file, further down) replaces the flight recorder's discrete EVENT records. A
         * fixed, obvious name - not seed-suffixed - so every run, whatever its seed, opens the exact same path;
         * trace_open() creates it if it does not exist and otherwise reopens and continues it, and writes a
         * RUN_START marker on every open so a reader can find exactly where the latest run's own data begins in
         * a file many runs' records now share. Built with the same snprintf + '/' join every path in this
         * function already uses, which this codebase already ships working identically on Windows, Linux and
         * macOS. */
        char trace_path[512];
        snprintf(trace_path, sizeof(trace_path), "%s/trace.jsonl", ctx->config.trace_dir);
        ctx->trace = trace_open(trace_path, ctx->rng.master_seed);
        if (ctx->trace) {
            platform_diag_set_sink(trace_diag_sink, ctx->trace);
        }
        platform_diag_logf("Carrom Arena %s seed=%llu speed=%.2fx window=%dx%d\n", BUILD_ID,
                           (unsigned long long)ctx->rng.master_seed, ctx->playback_speed, ctx->config.window_width, ctx->config.window_height);

        ctx->app_ev_prev_phase = -1;
        ctx->app_ev_prev_seat = -1;
        ctx->app_ev_prev_speed = -1.0f;
        APP_EVENT(ctx, APP_EV_START, (float)(ctx->rng.master_seed & 0xFFFFFFu),
                  (float)ctx->config.window_width, (float)ctx->config.window_height, 0.0f);
    }
}

static void app_setup_renderer(AppContext* ctx) {
    if (ctx->config.mode == APP_MODE_RENDERED) {
        ctx->renderer = renderer_create(ctx->config.window_width, ctx->config.window_height,
                                         "Carrom Arena", ctx->config.debug_phase, ctx->playback_speed);
        audio_init();   /* silent no-op when there is no audio device */
        if (!ctx->config.no_radio) radio_init();   /* AH.FM, playing by default */
    }
}

/* -----------------------------------------------------------------------------
 * Core Simulation Step (Fixed Timestep)
 * --------------------------------------------------------------------------- */
// Use physics.h definitions: PHYSICS_HZ, PHYSICS_DT, MAX_SUBSTEPS

#define AIM_PREVIEW_SECONDS 2.0   /* the launch line + arrow are shown for 2 s before the shot; they grow over the first 85% of it */

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


/* Discrete state-change events, checked once a frame (2026-09-28): this used to also assemble a full per-piece,
 * per-frame FlightFrame binary record here - dropped along with the rest of the separate flight recorder (see the
 * operator's "exactly one trace file" note in app_setup_trace()). That full-board-every-frame fidelity is not
 * replicated: trace's own PHYSICS_STATE (striker, roughly every frame during a shot) and SHOT_PROGRESS/
 * SHOT_INTERRUPTED (every moving or all-pieces-on-interrupt) already cover physics state at a size the shared
 * 8 MiB budget can actually sustain across a real session, where a full 19-piece JSON snapshot every rendered
 * frame would fill the ring in seconds. What is kept is exactly the discrete, cheap, high-debug-value part: phase/
 * turn/speed/pause/layout actually CHANGING. */
static void app_track_state_changes(AppContext* ctx) {
    if (!ctx->trace) return;

    if ((int)ctx->game.phase != ctx->app_ev_prev_phase) {
        APP_EVENT(ctx, APP_EV_PHASE, ctx->app_ev_prev_phase, ctx->game.phase, ctx->game.turn_seat, 0);
        ctx->app_ev_prev_phase = (int)ctx->game.phase;
    }
    if ((int)ctx->game.turn_seat != ctx->app_ev_prev_seat) {
        APP_EVENT(ctx, APP_EV_TURN, ctx->game.turn_seat, ctx->game.active_player.team, 0, 0);
        ctx->app_ev_prev_seat = (int)ctx->game.turn_seat;
    }
    if (fabsf(ctx->playback_speed - ctx->app_ev_prev_speed) > 1e-6f) {
        APP_EVENT(ctx, APP_EV_SPEED, ctx->playback_speed, 0, 0, 0);
        ctx->app_ev_prev_speed = ctx->playback_speed;
    }
    if (ctx->paused != ctx->app_ev_prev_paused) {
        APP_EVENT(ctx, APP_EV_PAUSE, ctx->paused ? 1 : 0, 0, 0, 0);
        ctx->app_ev_prev_paused = ctx->paused;
    }
    Layout L = renderer_get_layout(ctx->renderer);
    if (fabsf((float)L.board_x - ctx->app_ev_prev_layout[0]) > 0.5f || fabsf((float)L.board_y - ctx->app_ev_prev_layout[1]) > 0.5f ||
        fabsf((float)L.board_size - ctx->app_ev_prev_layout[2]) > 0.5f) {
        APP_EVENT(ctx, APP_EV_LAYOUT, L.board_x, L.board_y, L.board_size, L.sw);
        ctx->app_ev_prev_layout[0] = (float)L.board_x;
        ctx->app_ev_prev_layout[1] = (float)L.board_y;
        ctx->app_ev_prev_layout[2] = (float)L.board_size;
    }
}

/* Play a cue through the audio policy (loudness, variant rotation, rate limit) and log it in the trace. */
static void app_cue(AppContext* ctx, AudioCue cue, float speed) {
    float volume;
    int variant;
    double now = platform_time_now();
    if (!audio_policy_admit(&ctx->audio_policy, cue, speed, now, audio_variant_count(cue), &volume, &variant)) return;
    audio_play(cue, volume, variant);
    APP_EVENT(ctx, APP_EV_SOUND, cue, speed, volume, variant);
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
        APP_EVENT(ctx, APP_EV_MUTE, ctx->prev_muted ? 1 : 0, 0, 0, 0);
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
            APP_EVENT(ctx, APP_EV_POCKET, r.pocketed_ids[i], r.pocketed_pocket_indices[i], sqrtf(lv.x * lv.x + lv.y * lv.y), t);
            APP_EVENT(ctx, APP_EV_STASH, r.pocketed_ids[i], r.pocketed_pocket_indices[i],
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
            platform_diag_logf("[POCKET] id=%d pocket=%d\n", (int)r.pocketed_ids[i], (int)r.pocketed_pocket_indices[i]);
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
            ctx->striker_fall_pocket = spocket;
            APP_EVENT(ctx, APP_EV_STRIKER_POCKET, spocket, sqrtf(sv.x * sv.x + sv.y * sv.y), 0, 0);
            effects_trigger_pocket_fall(EFFECTS_STRIKER_ID, PIECE_STRIKER, sp, sv, spocket);
            if (ctx->config.verbose) { platform_diag_logf("[POCKET] striker pocket=%d\n", spocket); }
            effects_trigger_pocket_fade_long(spocket, 0.9f);
        }
    }
    if (t >= ctx->next_progress_time) {
        app_snapshot_trace(ctx, false);
        ctx->next_progress_time = t + 2.0f;
    }
}

/* The scoreboard shows scoring_live_board_points (the coins pocketed on the current board, the queen once covered) on top of the
 * finished boards THIS GAME (score_base, reset to 0 when a new game starts): it moves the moment a coin drops or comes back.
 * Past 999 it starts again from 0. */
static int app_live_points(AppContext* ctx, int pair) {
    int live = scoring_live_board_points(&ctx->game.board, pair);
    if (ctx->score_base[pair] + live > 999) ctx->score_base[pair] = -live;               /* past 999: start again from 0 */
    if (ctx->score_base[pair] + live < 0) ctx->score_base[pair] = -live;
    return ctx->score_base[pair] + live;
}

/* -----------------------------------------------------------------------------
 * Arranging the coins: at the start of every board (and of the very first one, from a random scatter) the coins glide into the
 * starting formation, the queen first, then the inner ring, then the outer ring, each along its own curve. The game waits
 * for it, which is also the pause between boards and games.
 * --------------------------------------------------------------------------- */
#define ARRANGE_STAGGER 0.08f     /* seconds between one coin setting off and the next */
#define ARRANGE_SLIDE 1.0f        /* seconds one coin takes */
#define ARRANGE_PAUSE_BOARD 0.8f  /* the final position stays visible this long after a board */
#define ARRANGE_PAUSE_GAME 2.5f   /* ... after a game */
#define ARRANGE_PAUSE_MATCH 3.5f  /* ... after a match */
#define ARRANGE_PAUSE_FIRST 0.7f  /* the scatter at the very start */

/* Where every coin is now: on the board, or in its slot beside the pocket it fell into */
static void app_capture_positions(const BoardState* b, Vec2 out[MAX_PIECES]) {
    for (int i = 0; i < MAX_PIECES; i++) {
        out[i] = b->pieces[i].position;
        if (b->pieces[i].on_board) continue;
        for (int k = 0; k < MAX_PIECES; k++) {
            if (b->pocketed_pieces[k].pocketed && b->pocketed_pieces[k].id == i) { out[i] = b->pocketed_pieces[k].pocketed_position; break; }
        }
    }
}

/* Visual-only randomness (never the game's streams: a rendered game must play exactly like a headless one of the same seed) */
static uint64_t app_visual_rand(uint64_t* s) {
    *s ^= *s << 13; *s ^= *s >> 7; *s ^= *s << 17;
    return *s;
}

static void app_scatter_positions(uint64_t seed, Vec2 out[MAX_PIECES]) {
    uint64_t s = seed * 0x9E3779B97F4A7C15ULL + 0x5DEECE66DULL;
    if (s == 0) s = 1;
    const float area = 0.36f, min_d = 2.2f * PIECE_RADIUS_NORM;
    for (int i = 0; i < MAX_PIECES; i++) {
        Vec2 p = { 0.0f, 0.0f };
        for (int tries = 0; tries < 200; tries++) {
            p.x = ((float)(app_visual_rand(&s) >> 40) / 16777216.0f * 2.0f - 1.0f) * area;
            p.y = ((float)(app_visual_rand(&s) >> 40) / 16777216.0f * 2.0f - 1.0f) * area;
            bool clear = true;
            for (int j = 0; j < i && clear; j++) {
                float dx = p.x - out[j].x, dy = p.y - out[j].y;
                if (dx * dx + dy * dy < min_d * min_d) clear = false;
            }
            if (clear) break;
        }
        out[i] = p;
    }
}

static void app_begin_arrange(AppContext* ctx, const Vec2 from[MAX_PIECES], float pause) {
    if (!ctx->renderer) return;
    int order[MAX_PIECES];
    float dist[MAX_PIECES];
    for (int i = 0; i < MAX_PIECES; i++) {
        order[i] = i;
        Vec2 to = ctx->game.board.pieces[i].position;
        dist[i] = to.x * to.x + to.y * to.y;
    }
    for (int i = 1; i < MAX_PIECES; i++) {          /* insertion sort: the coins nearest the centre first */
        int id = order[i], j = i - 1;
        while (j >= 0 && dist[order[j]] > dist[id]) { order[j + 1] = order[j]; j--; }
        order[j + 1] = id;
    }
    for (int k = 0; k < MAX_PIECES; k++) {
        int id = order[k];
        effects_trigger_slide(id, ctx->game.board.pieces[id].color, from[id], ctx->game.board.pieces[id].position,
                              pause + (float)k * ARRANGE_STAGGER, ARRANGE_SLIDE, (k & 1) ? 0.22f : -0.22f);
    }
    ctx->arrange_wait = (double)(pause + (float)(MAX_PIECES - 1) * ARRANGE_STAGGER + ARRANGE_SLIDE + 0.3f);
}

static void app_resolve_shot(AppContext* ctx, const ShotResult* result) {
    // Extract facts for rules engine
    ShotFacts facts;
    match_extract_facts(&ctx->game, result, &facts);
    
    // The coins have moved: the game state (what the AI plans from, and where the rules place a coin paid back) must show
    // where they really are now
    board_apply_shot_positions(&ctx->game.board, result);

    // Resolve through rules engine
    RulesOutcome outcome = rules_resolve(&ctx->match, &ctx->game, &facts);
    
    // The scoreboard keeps a running tally across boards and games
    ctx->total_games[0] += (int)outcome.next_match_state.games_won_white - (int)ctx->match.games_won_white;
    ctx->total_games[1] += (int)outcome.next_match_state.games_won_black - (int)ctx->match.games_won_black;
    for (int i = 0; i < 2; i++) if (ctx->total_games[i] > 99) ctx->total_games[i] = 0;   /* the board shows 00-99 games */

    // Boards won this game, by PHYSICAL pair. boards_won_white/black are already keyed by physical pair (0 = north/
    // south, 1 = east/west) at the source in rules.c - winner_pair there comes from pair_of_seat(seat), which never
    // changes with a colour swap - so no seats_swapped translation belongs here; that used to double-flip it.
    {
        int dwhite = (int)outcome.next_match_state.boards_won_white - (int)ctx->match.boards_won_white;
        int dblack = (int)outcome.next_match_state.boards_won_black - (int)ctx->match.boards_won_black;
        ctx->pair_boards_won[0] += dwhite;   /* red = N/S */
        ctx->pair_boards_won[1] += dblack;   /* blue = E/W */
    }

    // Apply outcome to match and game states
    ctx->game = outcome.next_game_state;
    ctx->match = outcome.next_match_state;

    // The rendered arena never stops: after a game (or a whole match) the next one begins, and the scoreboard carries on
    TurnDecision decision = outcome.turn_decision;
    bool starting_new_game = (decision == TURN_GAME_OVER || decision == TURN_MATCH_OVER);
    if (ctx->config.mode == APP_MODE_RENDERED && starting_new_game) {
        ctx->match.boards_won_white = 0;
        ctx->match.boards_won_black = 0;
        ctx->pair_boards_won[0] = 0;
        ctx->pair_boards_won[1] = 0;
        if (decision == TURN_MATCH_OVER) {
            ctx->match.games_won_white = 0;
            ctx->match.games_won_black = 0;
            match_randomize_first_breaker(&ctx->match, &ctx->rng);   /* a new match: toss again for who breaks first */
        }
        ctx->game.scores.white = 0;
        ctx->game.scores.black = 0;
        decision = TURN_BOARD_OVER;
    }

    APP_EVENT(ctx, APP_EV_SHOT_END, (int)outcome.turn_decision, result->pocketed_count, result->striker_pocketed ? 1 : 0, result->sim_time);

    if (outcome.foul) app_cue(ctx, CUE_FOUL, 1.0f);
    if (outcome.returned_count > 0) {
        /* ICF 65-66, 72-75, 93: the queen and the coins paid back return to the board: each slides in from the pocket it fell into */
        bool queen_back = false;
        for (int i = 0; i < outcome.returned_count; i++) {
            const ReturnedCoin* rc = &outcome.returned[i];
            if (rc->id == QUEEN_ID) queen_back = true;
            if (ctx->renderer) effects_trigger_return(rc->id, ctx->game.board.pieces[rc->id].color, rc->from_pocket, rc->pos);
            APP_EVENT(ctx, APP_EV_STASH, rc->id, rc->from_pocket, rc->pos.x, rc->pos.y);
        }
        if (queen_back) app_cue(ctx, CUE_QUEEN_BACK, 1.0f);
    }
    if (outcome.turn_decision == TURN_BOARD_OVER) app_cue(ctx, CUE_BOARD_WON, 1.0f);

    // Register any pockets not already registered mid-shot (idempotent)
    for (int i = ctx->pockets_registered; i < result->pocketed_count; i++) {
        if (result->pocketed_ids[i] < MAX_PIECES && ctx->game.board.pieces[result->pocketed_ids[i]].on_board) continue;   /* paid back */
        app_register_pocket(ctx, result->pocketed_ids[i], result->pocketed_pocket_indices[i]);
    }
    ctx->pockets_registered = 0;
    ctx->striker_fall_registered = false;
    ctx->next_progress_time = 0.0f;

    // Fresh striker for the next turn (a pocketed striker is a foul, but the next player still gets one)
    striker_state_init(&ctx->game.board.striker, ctx->game.turn_seat);
    board_place_striker_on_baseline(&ctx->game.board.striker, ctx->game.turn_seat);
    // The rules may have put coins back on the board (the queen, not covered; a coin paid back for a pocketed striker):
    // put them back in the physics world too
    for (int i = 0; i < MAX_PIECES; i++) {
        if (ctx->game.board.pieces[i].on_board && physics_is_piece_pocketed(ctx->physics, i)) {
            physics_sync_from_board(ctx->physics, &ctx->game.board, ctx->game.turn_seat);
            break;
        }
    }

    /* Nothing else happens until the coins the rules paid back (the queen included) have slid in, and then until a pocketed striker
     * has slid back to the player whose turn it is, around the coins; meanwhile no coin moves. (Rendered mode only.) */
    if (ctx->renderer) {
        float wait = 0.0f;
        if (outcome.returned_count > 0) wait = EFFECTS_RETURN_SLIDE_TIME + 0.15f;
        if (result->striker_pocketed && decision != TURN_BOARD_OVER) {
            Vec2 coins[MAX_PIECES];
            int nc = 0;
            for (int i = 0; i < MAX_PIECES; i++) if (ctx->game.board.pieces[i].on_board) coins[nc++] = ctx->game.board.pieces[i].position;
            Vec2 from = POCKET_CENTERS[(ctx->striker_fall_pocket >= 0 && ctx->striker_fall_pocket <= 3) ? ctx->striker_fall_pocket : 0];
            Vec2 to = ctx->game.board.striker.position;
            /* a coin sitting right on the spot: the nearest free place along the baseline instead */
            bool horizontal = (ctx->game.turn_seat == SEAT_NORTH || ctx->game.turn_seat == SEAT_SOUTH);
            for (int k = 0; k <= 24; k++) {
                int sign = (k & 1) ? -1 : 1;
                float off = (float)((k + 1) / 2) * 0.03f * (float)sign;
                Vec2 cand = horizontal ? (Vec2){ to.x + off, to.y } : (Vec2){ to.x, to.y + off };
                bool free_spot = true;
                for (int c = 0; c < nc && free_spot; c++) {
                    float dx = cand.x - coins[c].x, dy = cand.y - coins[c].y;
                    if (sqrtf(dx * dx + dy * dy) < STRIKER_PATH_CLEAR) free_spot = false;
                }
                if (free_spot) { to = cand; break; }
            }
            Vec2 path[40];
            int np = striker_path_plan(from, to, coins, nc, path, 40);
            float delay = wait + 0.25f;
            float dur = effects_trigger_striker_slide(path, np, delay, 0.9f);
            wait = delay + dur + 0.2f;
        }
        if ((double)wait > ctx->arrange_wait) ctx->arrange_wait = (double)wait;
    }

    // Set phase for next turn based on turn decision
    switch (decision) {
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
    if (decision == TURN_BOARD_OVER) {
        if (!match_is_over(&ctx->match)) {
            Vec2 prev_positions[MAX_PIECES];
            app_capture_positions(&ctx->game.board, prev_positions);
            for (int pr = 0; pr < 2; pr++) {
                /* a new game starts P fresh at 0; otherwise the finished board's coins are banked into the running total */
                ctx->score_base[pr] = starting_new_game ? 0 : app_live_points(ctx, pr);
            }
            float pause = (outcome.turn_decision == TURN_MATCH_OVER) ? ARRANGE_PAUSE_MATCH
                        : (outcome.turn_decision == TURN_GAME_OVER) ? ARRANGE_PAUSE_GAME : ARRANGE_PAUSE_BOARD;
            match_start_board(&ctx->match, &ctx->game, &ctx->rng);
            physics_sync_from_board(ctx->physics, &ctx->game.board, ctx->game.turn_seat);
            app_begin_arrange(ctx, prev_positions, pause);
            // match_start_board leaves the phase at PLACEMENT and the old plan in place: without this the new board skipped
            // THINKING and its first player aimed with the previous board's last plan (a line pointing off the board).
            physics_reset_turn_timer(ctx->physics);
            ctx->game.phase = PHASE_THINKING;
            ctx->game.computed_shot_valid = false;
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
    
    // Initialize first board - start with THINKING for first turn
    match_start_board(&ctx->match, &ctx->game, &ctx->rng);
    physics_sync_from_board(ctx->physics, &ctx->game.board, ctx->game.turn_seat);
    ctx->arrange_wait = 0.0;
    if (ctx->renderer) {      /* the very first scene: coins strewn about, then arranged */
        Vec2 scattered[MAX_PIECES];
        app_scatter_positions(ctx->rng.master_seed, scattered);
        app_begin_arrange(ctx, scattered, ARRANGE_PAUSE_FIRST);
    }
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
        platform_diag_logf("[DEBUG] Starting simulation, seed=%llu\n", (unsigned long long)ctx->rng.master_seed);
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
        radio_update();   /* keeps the radio fed whatever the game is doing (also while paused) */
        
        if (!ctx->paused) {
            // Periodic debug output
            if (ctx->config.verbose && (ctx->frame_count % 1000 == 0)) {
                platform_diag_logf("[DEBUG] Frame %llu: phase=%d, sim_time=%.2f, shots=%llu\n", 
                       (unsigned long long)ctx->frame_count, ctx->game.phase, 
                       physics_get_sim_time(ctx->physics), (unsigned long long)ctx->shot_count);
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
            
            /* while the coins are being arranged nothing else happens (the phase counts as idle) */
            switch (ctx->arrange_wait > 0.0 ? (ctx->arrange_wait -= dt, PHASE_IDLE) : ctx->game.phase) {
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
                            
                            PhysicsSnapshot* psnap = physics_snapshot(ctx->physics);
                            DecisionSnapshot snap = {
                                .match = &ctx->match,
                                .game = &ctx->game,
                                .board = &ctx->game.board,
                                .physics = psnap,
                                .active_seat = seat,
                                .ai_budget_ms = ctx->config.ai_budget_ms,
                                .max_candidates = ctx->max_candidates
                            };
                            
                            // AI decides shot plan
                            ShotPlan plan;
                            if (psnap) plan = controller_decide(controller, &snap, &ctx->rng.streams[seat]);
                            else plan = controller_fallback_shot(controller, &snap, &ctx->rng.streams[seat]);
                            physics_snapshot_destroy(psnap);   /* one snapshot per decision: it used to leak */
                            
                            // Validate shot plan
                            if (!match_validate_shot(&ctx->game, &plan)) {
                                plan = controller_fallback_shot(controller, &snap, &ctx->rng.streams[seat]);
                            }
                            
                            ctx->pending_shot_plan = plan;
                            ctx->pending_shot_valid = true;
                            APP_EVENT(ctx, APP_EV_PLAN, plan.aim_angle, plan.power, plan.placement.x, plan.placement.y);
                            
                            if (ctx->config.verbose) {
                                platform_diag_logf("[DEBUG] Frame %llu: THINKING complete for seat %d, tactic=%d\n", 
                                       (unsigned long long)ctx->frame_count, seat, plan.tactic);
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
                            platform_diag_logf("[DEBUG] Frame %llu: Placement phase started for seat %d, timer=%.2fs\n", 
                                   (unsigned long long)ctx->frame_count, ctx->game.turn_seat, ctx->placement_timer);
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
                            platform_diag_logf("[DEBUG] Frame %llu: Placement complete, entering AIM_PREVIEW for seat %d\n", 
                                   (unsigned long long)ctx->frame_count, ctx->game.turn_seat);
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
                        ctx->game.aim_line_progress = (float)(elapsed_scaled / AIM_PREVIEW_SECONDS);
                        if (ctx->game.aim_line_progress > 1.0f) ctx->game.aim_line_progress = 1.0f;
                        if (ctx->game.aim_line_progress < 0.0f) ctx->game.aim_line_progress = 0.0f;
                        
                        if (ctx->aim_preview_timer <= 0.0) {
                            // 5 seconds elapsed (scaled) - execute the shot
                            ctx->aim_preview_active = false;
                            if (ctx->config.verbose) {
                                platform_diag_logf("[DEBUG] Frame %llu: AIM_PREVIEW complete, executing shot for seat %d\n", 
                                       (unsigned long long)ctx->frame_count, ctx->game.turn_seat);
                            }
                            
                            // FIRST: Invalidate computed shot so aim line clears BEFORE striker gains velocity
                            ctx->game.computed_shot_valid = false;
                                            ctx->game.aim_line_progress = 0.0f;
                            
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
                            APP_EVENT(ctx, APP_EV_SHOT_START, ctx->shot_count, seat, ctx->game.computed_shot_plan.aim_angle, ctx->game.computed_shot_plan.power);
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
            if (renderer_radio_clicked(ctx->renderer)) radio_toggle();
            renderer_set_radio(ctx->renderer, !ctx->config.no_radio && audio_ready(), radio_is_playing());   /* the button (and the radio itself) needs a real audio device: no device, no button */
            renderer_set_scoreboard(ctx->renderer,
                                     ctx->total_games[0], ctx->pair_boards_won[0], app_live_points(ctx, 0),
                                     ctx->total_games[1], ctx->pair_boards_won[1], app_live_points(ctx, 1));
            renderer_set_turn_team(ctx->renderer, ctx->game.active_player.team);
            renderer_begin(ctx->renderer);
            renderer_begin_board(ctx->renderer);
            float alpha = (float)(ctx->accumulator / PHYSICS_DT);
            if (alpha > 1.0f) alpha = 1.0f;
            renderer_draw_board(ctx->renderer, &ctx->game.board, ctx->physics, alpha, ctx->game.phase, &ctx->game, ctx->placement_timer);
            renderer_draw_effects(ctx->renderer, &ctx->game, ctx->placement_timer);
            renderer_end_board(ctx->renderer);
            renderer_end(ctx->renderer);
            app_track_state_changes(ctx);
            ctx->frame_count++;
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
ctx->speed_paused = false;
    audio_policy_init(&ctx->audio_policy);
    ctx->placement_timer = 0.0;
    ctx->placement_phase_active = false;
    ctx->aim_preview_timer = 0.0;
    ctx->aim_preview_active = false;
    
    // Initialize RNG
    uint64_t seed = config->seed;
if (seed == 0) {
        seed = platform_time_us();
    }
    rng_context_init(&ctx->rng, seed);
    
    // Initialize physics
    ctx->physics = physics_create();
    if (!ctx->physics) {
        platform_fatal("Could not create the physics world.");
        free(ctx);
        return NULL;
    }

    // Setup subsystems
    app_setup_trace(ctx);
    app_setup_renderer(ctx);
    if (config->mode == APP_MODE_RENDERED && !ctx->renderer) {
        platform_fatal("Could not open the game window (graphics initialisation failed).");
        app_destroy(ctx);
        return NULL;
    }
    app_init_controllers(ctx);
    app_init_match(ctx);
    
    return ctx;
}

void app_destroy(AppContext* ctx) {
    if (!ctx) return;
    
    app_cleanup_controllers(ctx);
    
    APP_EVENT(ctx, APP_EV_CLOSE, ctx->game.phase, 0, 0, 0);
    if (ctx->trace) {
        if (ctx->game.phase == PHASE_SHOT_EXECUTION || ctx->game.phase == PHASE_SETTLING) {
            app_snapshot_trace(ctx, true);
        }
        trace_close(ctx->trace);
        /* Detach the sink right here, not after: everything below (radio/audio/renderer/physics shutdown) can
         * still call platform_diag_logf on an error path, and the trace writer it would be handed to is gone. */
        platform_diag_set_sink(NULL, NULL);
    }
    
    radio_shutdown();
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


/* -----------------------------------------------------------------------------
 * CLI Parsing
 * --------------------------------------------------------------------------- */
/* "--name=value" or "--name value" (the usage text shows the second form). Advances *i past a separate value. */
static const char* get_arg_value(int argc, char* argv[], int* i, const char* name) {
    const char* arg = argv[*i];
    size_t len = strlen(name);
    if (strncmp(arg, name, len) != 0) return NULL;
    if (arg[len] == '=') return arg + len + 1;
    if (arg[len] == '\0' && *i + 1 < argc) return argv[++*i];
    return NULL;
}

AppConfig app_parse_args(int argc, char* argv[]) {
    AppConfig config = app_config_default();
    
    for (int i = 1; i < argc; i++) {
        const char* val;
        if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
            app_print_usage(argv[0]);
            exit(0);
        } else if (strcmp(argv[i], "--version") == 0 || strcmp(argv[i], "-v") == 0) {
            app_print_version();
            exit(0);
        } else if ((val = get_arg_value(argc, argv, &i, "--mode")) != 0) {
            if (strcmp(val, "rendered") == 0) config.mode = APP_MODE_RENDERED;
            else if (strcmp(val, "diagnostic") == 0) config.mode = APP_MODE_DIAGNOSTIC;
            else if (strcmp(val, "soak") == 0) config.mode = APP_MODE_SOAK;
        } else if ((val = get_arg_value(argc, argv, &i, "--seed")) != 0) {
            config.seed = strtoull(val, NULL, 10);
        } else if ((val = get_arg_value(argc, argv, &i, "--boards")) != 0) {
            config.boards = (uint32_t)strtoul(val, NULL, 10);
        } else if ((val = get_arg_value(argc, argv, &i, "--seeds")) != 0) {
            config.seeds = (uint32_t)strtoul(val, NULL, 10);
        } else if ((val = get_arg_value(argc, argv, &i, "--matches")) != 0) {
            config.matches = (uint32_t)strtoul(val, NULL, 10);
        } else if ((val = get_arg_value(argc, argv, &i, "--trace-dir")) != 0) {
            config.trace_dir = val;
        } else if ((val = get_arg_value(argc, argv, &i, "--width")) != 0) {
            config.window_width = atoi(val);
        } else if ((val = get_arg_value(argc, argv, &i, "--height")) != 0) {
            config.window_height = atoi(val);
        } else if ((val = get_arg_value(argc, argv, &i, "--playback-speed")) != 0) {
            float speed = strtof(val, NULL);
            if (speed < 0.05f) speed = 0.05f;
            if (speed > 4.0f) speed = 4.0f;
            config.playback_speed = speed;
        } else if ((val = get_arg_value(argc, argv, &i, "--ai-budget-ms")) != 0) {
            config.ai_budget_ms = (uint32_t)strtoul(val, NULL, 10);
            if (config.ai_budget_ms < 10) config.ai_budget_ms = 10;
            if (config.ai_budget_ms > 10000) config.ai_budget_ms = 10000;
        } else if (strcmp(argv[i], "--verbose") == 0) {
            config.verbose = true;
        } else if (strcmp(argv[i], "--debug-phase") == 0) {
            config.debug_phase = true;
        } else if (strcmp(argv[i], "--no-radio") == 0) {
            config.no_radio = true;
        }
    }
    
    /* the window size is locked: the layout is final, and the window cannot be resized */
    if (config.mode == APP_MODE_RENDERED) { config.window_width = 560; config.window_height = 560; }
    if (config.window_width < 400) config.window_width = 400;     /* a zero or absurd size would fail InitWindow */
    if (config.window_width > 4096) config.window_width = 4096;
    if (config.window_height < 400) config.window_height = 400;
    if (config.window_height > 4096) config.window_height = 4096;
    return config;
}

void app_print_usage(const char* prog_name) {
    printf("Carrom Arena - Autonomous Four-Player Carrom Simulation\n\n");
    printf("Usage: %s [options]\n\n", prog_name);
    printf("Options:\n");
    printf("  --mode <mode>         Mode: rendered, diagnostic, soak (default: rendered)\n");
    printf("  --seed <n>            Master RNG seed (0 = random)\n");
    printf("  --boards <n>          Boards per seed (soak mode, default: 100)\n");
    printf("  --seeds <n>           Number of seeds (soak mode, default: 100)\n");
    printf("  --matches <n>         Matches per board/seed (soak mode, default: 10)\n");
    printf("  --trace-dir <path>    Trace output directory (default: traces)\n");
    printf("  --playback-speed <x>  Sim speed multiplier 0.05-4.0 (default: 1.0)\n");
    printf("  --ai-budget-ms <n>    AI decision time budget in ms (default: 150, range: 10-10000)\n");
    printf("  --verbose             Verbose logging\n");
    printf("  --no-radio            Do not start the AH.FM internet radio\n");
    printf("  --debug-phase         Enable per-frame phase debug logging\n");
    printf("  --width <n>           Window width (default: 560)\n");
    printf("  --height <n>          Window height (default: 560)\n");
    printf("  --help, -h            Show this help\n");
    printf("  --version, -v         Show version\n");
}

void app_print_version(void) {
    printf("Carrom Arena v1.0.0 (Build: %s)\n", BUILD_ID);
    printf("Based on original work by Supratim Sanyal of SANYALnet Labs.\n");
    printf("SANYALnet Labs Non-Commercial License; see the LICENSE file.\n");
}
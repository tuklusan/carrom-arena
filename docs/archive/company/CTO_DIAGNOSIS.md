# CTO DIAGNOSIS: Carrom Arena Jerky/Non-Game-Like Behavior

**Date:** 2026-09-03  
**Author:** CTO, SANYALnet Labs  
**Status:** Production Blocking — Must Fix Before Demo

---

## Root Cause Summary

| # | Symptom | Root Cause | File:Line |
|---|---------|------------|-----------|
| 1 | Jerky/teleporting pieces | 120 Hz physics → 60 FPS render **with zero interpolation**; renderer reads raw `physics_get_positions` each frame | `board_view.c:89-93` |
| 2 | Pieces never settle (30 s timeout hits) | Coulomb=0.12, Viscous=0.85 **10× too weak** for carrom; decel ≈ 0.12 + 0.85·v vs required ~2–3 m/s² | `physics.h:23-24`, `physics.c:246` |
| 3 | Window ignores `--width/--height`, overflows 1080p | `renderer_create` discards CLI args; fixed `GAME_SURFACE_SIZE=600` + title/copyright bars push height > 1080 on small displays | `renderer.c:53-78` |
| 4 | HUD unreadable on small windows | `hud_draw` uses absolute pixels (20, 20, 50…) and fixed font sizes; no viewport-relative layout | `hud.c:10-40` |
| 5 | Game loop stalls in `PHASE_SETTLING` | #2 causes `physics_is_settled` to return false → 30 s timeout → forced settle → wrong scores | `app.c:297-304`, `physics.c:491` |

---

## Minimal Fix Blueprint (≤ 1 Page)

### 1. Physics Constants — `src/physics/physics.h`

```c
// OLD (lines 23-27)
#define BOARD_COULOMB     0.12f
#define BOARD_VISCOUS     0.85f
#define SETTLE_SPEED_EPS  1e-4f
#define SETTLE_ACCEL_EPS  1e-4f
#define SETTLE_TIMEOUT_SECONDS 30.0f

// NEW — tuned for 30 mm pieces on 74 cm board, ~2.5 m/s² total decel
#define BOARD_COULOMB     0.35f    // ~0.35 m/s² constant friction
#define BOARD_VISCOUS     2.20f    // ~2.2·v  (dominates at speed)
#define SETTLE_SPEED_EPS  5e-3f    // 5 mm/s — visible stop
#define SETTLE_ACCEL_EPS  5e-3f
#define SETTLE_TIMEOUT_SECONDS 8.0f   // 8 s hard cap (was 30 s)
```

**Rationale:** Carrom pieces (≈15 g, 30 mm) on polished plywood exhibit μ≈0.15 Coulomb + quadratic air/viscous drag. At 1 m/s, total decel ≈ 0.35 + 2.2 ≈ 2.55 m/s² → stops in ~0.4 s from 1 m/s. 8 s timeout covers worst-case multi-collision sequences.

---

### 2. Renderer Interpolation — `src/render/board_view.c` + `src/physics/physics.h`

**Add to `PhysicsWorld` (physics.h:31-35):**
```c
// Previous-frame positions for interpolation
Vec2 prev_piece_positions[MAX_PIECES];
Vec2 prev_striker_position;
```

**In `physics_step` (physics.c:210-222) — snapshot BEFORE stepping:**
```c
// Store previous positions for renderer interpolation
for (int i = 0; i < MAX_PIECES; i++) {
    if (!pw->piece_pocketed[i] && b2Body_IsValid(pw->piece_bodies[i])) {
        b2Vec2 p = b2Body_GetPosition(pw->piece_bodies[i]);
        pw->prev_piece_positions[i] = (Vec2){ p.x, p.y };
    }
}
if (!pw->striker_pocketed && b2Body_IsValid(pw->striker_body)) {
    b2Vec2 p = b2Body_GetPosition(pw->striker_body);
    pw->prev_striker_position = (Vec2){ p.x, p.y };
}
```

**New accessor (physics.h:65-67):**
```c
void physics_get_prev_positions(const PhysicsWorld* pw, Vec2* positions);
void physics_get_prev_striker_position(const PhysicsWorld* pw, Vec2* pos);
```

**Interpolation in `board_view_draw` (board_view.c:84-114):**
```c
// Compute alpha = accumulator / PHYSICS_DT (0…1)
float alpha = 0.0f;
if (physics) {
    // Need access to accumulator — add to PhysicsWorld or pass from app
    extern float physics_get_accumulator(const PhysicsWorld*); // new helper
    alpha = physics_get_accumulator(physics) / PHYSICS_DT;
    if (alpha > 1.0f) alpha = 1.0f;
}

Vec2 pos = vec2_lerp(prev_pos, curr_pos, alpha);  // lerp helper in math.h
```

**math.h addition:**
```c
static inline Vec2 vec2_lerp(Vec2 a, Vec2 b, float t) {
    return (Vec2){ a.x + t * (b.x - a.x), a.y + t * (b.y - a.y) };
}
```

---

### 3. Window Clamp & CLI Respect — `src/render/renderer.c`

**Replace `renderer_create` (lines 53-105) with:**
```c
Renderer* renderer_create(int width, int height, const char* title, bool capture_mode) {
    Renderer* r = calloc(1, sizeof(Renderer));
    if (!r) return NULL;
    r->capture_mode = capture_mode;

    // Measure title/copyright
    r->title_width = measure_text_width(TITLE_TEXT, TITLE_FONT_SIZE);
    r->copyright_width = measure_text_width(COPYRIGHT_TEXT, COPYRIGHT_FONT_SIZE);

    int title_h = TITLE_FONT_SIZE + TITLE_PADDING * 2;
    int copyright_h = COPYRIGHT_FONT_SIZE + COPYRIGHT_PADDING * 2;

    // Clamp requested size to monitor bounds
    int monitor = GetCurrentMonitor();
    int max_w = GetMonitorWidth(monitor);
    int max_h = GetMonitorHeight(monitor);

    int req_w = (width > 0) ? width : max_w * 4 / 5;
    int req_h = (height > 0) ? height : max_h * 4 / 5;
    if (req_w > max_w) req_w = max_w;
    if (req_h > max_h) req_h = max_h;

    // Game surface = min(req_w, req_h - title_h - copyright_h), min 400, max 800
    int game_surface = req_h - title_h - copyright_h;
    if (game_surface > req_w) game_surface = req_w;
    game_surface = CLAMP(game_surface, 400, 800);

    r->width  = game_surface + 40;                 // 20 px side padding
    r->height = title_h + game_surface + copyright_h;

    r->game_surface_x = (r->width - game_surface) / 2;
    r->game_surface_y = title_h;

    r->viewport = math_viewport_create(game_surface, game_surface);

    SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_VSYNC_HINT);
    InitWindow(r->width, r->height, title);
    SetTargetFPS(60);
    center_window_on_monitor(r->width, r->height);
    ...
}
```

---

### 4. HUD Scaling — `src/render/hud.c`

**Replace `hud_draw` with viewport-relative layout:**
```c
void hud_draw(Viewport vp, const MatchState* match, const GameState* game) {
    // Convert viewport to screen coords for text
    Vec2 tl = math_world_to_screen(vp, (Vec2){ -0.5f, 0.5f });   // top-left of board
    float scale = vp.screen_w / 600.0f;                          // base 600 px
    int base_x = (int)tl.x + 10 * scale;
    int line_h = (int)(22 * scale);
    int fs_title = (int)(24 * scale);
    int fs_body  = (int)(18 * scale);
    int y = (int)tl.y + 10 * scale;

    #define HUD_LINE(fmt, ...) \
        DrawText(TextFormat(fmt, __VA_ARGS__), base_x, y, fs_body, WHITE); \
        y += line_h

    DrawText(TextFormat("WHITE: %d", match->games_won_white), base_x, y, fs_title, WHITE); y += line_h;
    DrawText(TextFormat("BLACK: %d", match->games_won_black), base_x, y, fs_title, WHITE); y += line_h;
    HUD_LINE("Board: W %d - B %d", game->scores.white, game->scores.black);
    HUD_LINE("Turn: %s (%s)", seat_names[game->turn_seat], team_names[game->active_player.team]);
    HUD_LINE("Phase: %s", phase_names[game->phase]);
    HUD_LINE("Boards: %d/%d", match->boards_won_white + match->boards_won_black, match->target_boards_per_game);
    HUD_LINE("Games: %d/%d", match->games_won_white + match->games_won_black, match->target_games_per_match);
    HUD_LINE("Queen: %s", queen_states[game->board.queen_state]);
    HUD_LINE("Dues: W=%d B=%d Q=%d", game->board.white_dues, game->board.black_dues, game->board.queen_dues);
    HUD_LINE("Pieces: W=%d B=%d Q=%s", game->board.white_on_board, game->board.black_on_board,
             game->board.queen_on_board ? "YES" : "NO");
}
```

---

### 5. Settling Flow Guard — `src/app/app.c`

**In `app_simulation_step` (line 95-104) — expose accumulator:**
```c
// Add to PhysicsWorld: float last_accumulator;  // updated each step
// In physics_step: pw->last_accumulator = pw->accumulator; (before substeps)

// In app.c:274-304 — tighten settling transition
case PHASE_SHOT_EXECUTION:
    if (app_is_shot_settled(ctx)) {
        ctx->game.phase = PHASE_SETTLING;
        // Immediately resolve on next frame if still settled (avoids extra frame)
    }
    break;

case PHASE_SETTLING:
    if (app_is_shot_settled(ctx)) {
        ctx->game.phase = PHASE_RESOLVING;
        ShotResult result = app_collect_shot_result(ctx);
        app_resolve_shot(ctx, &result);
    } else {
        // Re-entered motion (rare) — go back to SHOT_EXECUTION
        ctx->game.phase = PHASE_SHOT_EXECUTION;
    }
    break;
```

---

## Verification Checklist

| Test | Command | Pass Criteria |
|------|---------|---------------|
| Physics settle time | `./carrom_arena --mode diagnostic --seed 42 --verbose` | All shots settle < 3 s sim time, no 8 s timeout |
| Visual smoothness | `./carrom_arena --mode rendered --seed 123` | No teleport artifacts at 60 FPS; pieces glide |
| Window clamp | `./carrom_arena --mode rendered --width 2000 --height 2000` | Window ≤ monitor bounds, game surface 400–800 px |
| HUD scaling | Resize window to 800×600 | All HUD text visible, no overlap, scales with window |
| Full match | `./carrom_arena --mode soak --seeds 5 --boards 10 --matches 2` | Completes without hangs, correct scores |

---

## Files to Change (Minimal Set)

1. `src/physics/physics.h` — constants + prev-position fields + accessor prototypes
2. `src/physics/physics.c` — snapshot prev positions in `physics_step`, implement accessors
3. `src/common/math.h` — add `vec2_lerp`
4. `src/render/board_view.c` — interpolate using `alpha = accumulator / PHYSICS_DT`
5. `src/render/renderer.c` — clamp window, respect `--width/--height`
6. `src/render/hud.c` — viewport-relative layout with `scale`
7. `src/app/app.c` — tighter settling state transitions

**Estimated diff:** ~120 lines changed across 7 files. Zero new dependencies.
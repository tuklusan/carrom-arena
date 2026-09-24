# Rendering System Implementation Plan

## Overview
Three focused improvements to the rendering pipeline:
1. **Physics interpolation** in `board_view.c` for smooth 60fps+ rendering
2. **Window sizing/clamping** in `renderer.c` respecting CLI args and monitor constraints
3. **HUD legibility** in `hud.c` with responsive scaling and positioning

---

## 1. Physics Interpolation in board_view.c

### Problem
The renderer reads physics positions directly without interpolation. Physics runs at 120Hz fixed timestep, but rendering can run at any frame rate. This causes stutter when render frames fall between physics ticks.

### Solution
Store previous physics positions and lerp between previous/current based on `alpha = accumulator / PHYSICS_DT`.

### Changes

#### board_view.h
```c
// Add previous positions to BoardState or create new struct
// Option A: Add to BoardState (minimal API change)
typedef struct {
    PieceState pieces[MAX_PIECES];
    StrikerState striker;
    // ... existing fields ...
    Vec2 prev_piece_positions[MAX_PIECES];
    Vec2 prev_striker_position;
} BoardState;
```

#### board_view.c - `board_view_draw` signature
```c
// OLD:
void board_view_draw(Viewport vp, const BoardState* board, const PhysicsWorld* physics);

// NEW:
void board_view_draw(Viewport vp, const BoardState* board, const PhysicsWorld* physics, float interpolation_alpha);
```

#### board_view.c - Implementation
```c
void board_view_draw(Viewport vp, const BoardState* board, const PhysicsWorld* physics, float alpha) {
    // Get CURRENT physics positions
    Vec2 curr_positions[MAX_PIECES];
    Vec2 curr_striker_pos;
    physics_get_positions(physics, curr_positions);
    physics_get_striker_position(physics, &curr_striker_pos);
    
    // Interpolate: prev + alpha * (curr - prev)
    for (int i = 0; i < MAX_PIECES; i++) {
        if (!board->pieces[i].on_board) continue;
        
        Vec2 interp_pos = {
            board->prev_piece_positions[i].x + alpha * (curr_positions[i].x - board->prev_piece_positions[i].x),
            board->prev_piece_positions[i].y + alpha * (curr_positions[i].y - board->prev_piece_positions[i].y)
        };
        // ... draw at interp_pos ...
    }
    
    // Striker interpolation
    Vec2 interp_striker = {
        board->prev_striker_position.x + alpha * (curr_striker_pos.x - board->prev_striker_position.x),
        board->prev_striker_position.y + alpha * (curr_striker_pos.y - board->prev_striker_position.y)
    };
}
```

#### app.c - Update previous positions after physics step
In `app_simulation_step`, AFTER `physics_step`:
```c
static void app_simulation_step(AppContext* ctx, double dt) {
    ctx->accumulator += dt;
    
    int substeps = 0;
    while (ctx->accumulator >= PHYSICS_DT && substeps < MAX_SUBSTEPS) {
        // Store PREVIOUS positions BEFORE stepping
        physics_get_positions(ctx->physics, ctx->game.board.prev_piece_positions);
        physics_get_striker_position(ctx->physics, &ctx->game.board.prev_striker_position);
        
        physics_step(ctx->physics, PHYSICS_DT);
        ctx->accumulator -= PHYSICS_DT;
        substeps++;
    }
}
```

#### app.c - Pass alpha to renderer
```c
// In render section:
float alpha = (float)(ctx->accumulator / PHYSICS_DT);
renderer_draw_board(ctx->renderer, &ctx->game.board, ctx->physics, alpha);
```

---

## 2. Window Sizing/Clamping in renderer.c

### Problem
- `--width`/`--height` CLI args are passed but ignored (line 54: `(void)width; (void)height;`)
- No clamping to monitor size (1920x1080) with margins
- Layout assumes fixed 600x600 game surface but doesn't validate window fits

### Solution
Respect CLI dimensions, clamp to monitor with margins, ensure 600x600 + title + copyright + margins fit.

### Changes

#### renderer.c - `renderer_create` function

```c
Renderer* renderer_create(int width, int height, const char* title, bool capture_mode) {
    Renderer* r = calloc(1, sizeof(Renderer));
    if (!r) return NULL;
    
    r->capture_mode = capture_mode;
    r->paused = false;
    
    /* Calculate layout dimensions */
    r->title_width = measure_text_width(TITLE_TEXT, TITLE_FONT_SIZE);
    r->copyright_width = measure_text_width(COPYRIGHT_TEXT, COPYRIGHT_FONT_SIZE);
    
    int content_width = GAME_SURFACE_SIZE;
    int title_area_height = TITLE_FONT_SIZE + TITLE_PADDING * 2;
    int copyright_area_height = COPYRIGHT_FONT_SIZE + COPYRIGHT_PADDING * 2;
    int total_height = title_area_height + GAME_SURFACE_SIZE + copyright_area_height;
    
    /* Window dimensions - respect CLI args, clamp to monitor */
    int monitor = GetCurrentMonitor();
    int monitor_width = GetMonitorWidth(monitor);
    int monitor_height = GetMonitorHeight(monitor);
    
    // Margins: 40px horizontal, 80px vertical (taskbar + titlebar)
    int max_width = monitor_width - 40;
    int max_height = monitor_height - 80;
    
    // Minimum required: 600 game surface + 40px horizontal padding = 640
    // Minimum height: title(24+32) + 600 + copyright(16+24) = ~696
    int min_width = 640;
    int min_height = 696;
    
    // Start with CLI values, fallback to calculated
    int window_width = (width > 0) ? width : content_width + 40;
    int window_height = (height > 0) ? height : total_height + 40;
    
    // Clamp to [min, max]
    window_width = math_clamp(window_width, min_width, max_width);
    window_height = math_clamp(window_height, min_height, max_height);
    
    // Ensure text fits
    if (r->title_width + 40 > window_width) window_width = r->title_width + 40;
    if (r->copyright_width + 40 > window_width) window_width = r->copyright_width + 40;
    
    r->width = window_width;
    r->height = window_height;
    
    /* Game surface position within window - centered with margins */
    r->game_surface_x = (r->width - GAME_SURFACE_SIZE) / 2;
    r->game_surface_y = title_area_height;
    
    /* Create viewport for the fixed 600x600 game surface */
    r->viewport = math_viewport_create(GAME_SURFACE_SIZE, GAME_SURFACE_SIZE);
    
    SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_VSYNC_HINT);
    InitWindow(r->width, r->height, "SANYALnet Labs Carrom Arena");
    SetTargetFPS(60);
    
    /* Center window on monitor */
    center_window_on_monitor(r->width, r->height);
    
    // ... rest unchanged ...
}
```

---

## 3. HUD Legibility in hud.c

### Problems
- Fixed pixel positions (20, 20), (20, 50), etc. - don't scale with window
- Fixed font sizes (24, 20, 18) - too small on large windows, too large on small
- HUD overlaps game surface on small windows
- Current player not visually highlighted

### Solution
- Scale font sizes based on game surface size (viewport.screen_width)
- Position HUD relative to game surface corners with margins
- Use viewport to convert world→screen for positioning
- Highlight current player turn with distinct color/style

### Changes

#### hud.h - Add viewport parameter (already has it, good)

#### hud.c - Complete rewrite
```c
void hud_draw(Viewport vp, const MatchState* match, const GameState* game) {
    // Base font size scales with game surface (600px = baseline)
    float scale = vp.board_size_px / 600.0f;
    int base_font = (int)(18 * scale);
    int large_font = (int)(24 * scale);
    int small_font = (int)(14 * scale);
    int line_height = (int)(28 * scale);
    
    // Game surface screen bounds
    float gs_left = vp.board_center_px.x - vp.board_size_px * 0.5f;
    float gs_top = vp.board_center_px.y - vp.board_size_px * 0.5f;
    float gs_right = vp.board_center_px.x + vp.board_size_px * 0.5f;
    float gs_bottom = vp.board_center_px.y + vp.board_size_px * 0.5f;
    
    // Margin from game surface
    float margin = 16 * scale;
    
    // HUD positioned at TOP-LEFT of game surface (outside board)
    float x = gs_left - margin;
    float y = gs_top - margin;
    
    // But if that's off-screen, anchor to window left with margin
    if (x < margin) x = margin;
    
    // Draw score panel - TOP LEFT
    Color white_color = WHITE;
    Color black_color = WHITE;
    
    // Highlight current player's team
    if (game->active_player.team == TEAM_WHITE) white_color = YELLOW;
    else black_color = YELLOW;
    
    DrawText(TextFormat("WHITE: %d", match->games_won_white), (int)x, (int)y, large_font, white_color);
    y += line_height;
    DrawText(TextFormat("BLACK: %d", match->games_won_black), (int)x, (int)y, large_font, black_color);
    y += line_height * 1.2f;
    
    // Board score
    DrawText(TextFormat("Board: W %d - B %d", game->scores.white, game->scores.black), 
             (int)x, (int)y, base_font, WHITE);
    y += line_height;
    
    // Turn indicator - HIGHLIGHT current player
    const char* seat_names[4] = { "NORTH", "EAST", "SOUTH", "WEST" };
    const char* team_names[2] = { "WHITE", "BLACK" };
    Color turn_color = (game->active_player.team == TEAM_WHITE) ? YELLOW : ORANGE;
    DrawText(TextFormat("Turn: %s (%s)", seat_names[game->turn_seat], team_names[game->active_player.team]), 
             (int)x, (int)y, base_font, turn_color);
    y += line_height;
    
    // Phase
    const char* phase_names[] = {
        "IDLE", "PLACEMENT", "AIMING", "SHOT", "SETTLING", "RESOLVING",
        "BOARD_OVER", "GAME_OVER", "MATCH_OVER"
    };
    DrawText(TextFormat("Phase: %s", phase_names[game->phase]), (int)x, (int)y, small_font, GREEN);
    y += line_height;
    
    // Progress
    DrawText(TextFormat("Boards: %d/%d", match->boards_won_white + match->boards_won_black, match->target_boards_per_game), 
             (int)x, (int)y, small_font, LIGHTGRAY);
    y += line_height * 0.8f;
    DrawText(TextFormat("Games: %d/%d", match->games_won_white + match->games_won_black, match->target_games_per_match), 
             (int)x, (int)y, small_font, LIGHTGRAY);
    y += line_height;
    
    // Queen state
    const char* queen_states[] = { "ON_BOARD", "POCKETED_NO_COVER", "COVERED", "DUE" };
    Color queen_color = (game->board.queen_state == QUEEN_STATE_COVERED) ? GOLD : WHITE;
    DrawText(TextFormat("Queen: %s", queen_states[game->board.queen_state]), (int)x, (int)y, small_font, queen_color);
    y += line_height * 0.8f;
    
    // Dues
    DrawText(TextFormat("Dues: W=%d B=%d Q=%d", game->board.white_dues, game->board.black_dues, game->board.queen_dues), 
             (int)x, (int)y, small_font, LIGHTGRAY);
    y += line_height * 0.8f;
    
    // Piece counts
    DrawText(TextFormat("Pieces: W=%d B=%d Q=%s", game->board.white_on_board, game->board.black_on_board, 
             game->board.queen_on_board ? "YES" : "NO"), (int)x, (int)y, small_font, LIGHTGRAY);
}
```

---

## Implementation Order

1. **First**: Add `prev_piece_positions` and `prev_striker_position` to `BoardState` in `types.h`
2. **Second**: Update `app_simulation_step` in `app.c` to capture previous positions before physics step
3. **Third**: Modify `board_view_draw` signature and implementation for interpolation
4. **Fourth**: Update `renderer_draw_board` call in `app.c` to pass alpha
5. **Fifth**: Fix `renderer_create` window sizing/clamping logic
6. **Sixth**: Rewrite `hud_draw` with responsive scaling and positioning

---

## Verification Checklist

- [ ] Physics interpolation: pieces move smoothly at 60fps, 120fps, 144fps
- [ ] Window respects `--width 800 --height 600` and clamps to monitor
- [ ] 600x600 game surface + title + copyright always visible with margins
- [ ] HUD scales correctly at different window sizes
- [ ] Current player highlighted in turn indicator
- [ ] No text overlap at any supported resolution
- [ ] Capture mode still works correctly
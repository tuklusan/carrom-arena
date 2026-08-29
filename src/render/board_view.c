#include "board_view.h"
#include "common/types.h"
#include "common/math.h"
#include "physics/physics.h"
#include <raylib.h>
#include <math.h>

/* Colors */
#define COLOR_BOARD (Color){ 139, 105, 70, 255 }      // Wood brown
#define COLOR_CUSHION (Color){ 100, 70, 40, 255 }     // Darker brown
#define COLOR_POCKET (Color){ 0, 0, 0, 255 }          // Black
#define COLOR_WHITE_PIECE (Color){ 240, 240, 240, 255 }
#define COLOR_BLACK_PIECE (Color){ 30, 30, 30, 255 }
#define COLOR_QUEEN (Color){ 220, 30, 30, 255 }       // Red
#define COLOR_STRIKER (Color){ 255, 215, 0, 255 }     // Gold
#define COLOR_LINE (Color){ 255, 255, 255, 100 }      // White translucent
#define COLOR_BASELINE (Color){ 100, 255, 100, 150 }  // Green translucent

void board_view_draw(Viewport vp, const BoardState* board, const PhysicsWorld* physics) {
    // Use physics positions for smooth animation, fall back to board state
    Vec2 physics_positions[MAX_PIECES];
    Vec2 striker_pos;
    bool use_physics = false;
    
    if (physics) {
        physics_get_positions(physics, physics_positions);
        physics_get_striker_position(physics, &striker_pos);
        // Check if physics has valid positions (not all zero)
        for (int i = 0; i < MAX_PIECES; i++) {
            if (physics_positions[i].x != 0 || physics_positions[i].y != 0) {
                use_physics = true;
                break;
            }
        }
    }
    
    // Board surface
    float board_half = BOARD_SIDE_NORM * 0.5f;
    Vec2 bl = math_world_to_screen(vp, (Vec2){ -board_half, -board_half });
    Vec2 tr = math_world_to_screen(vp, (Vec2){ board_half, board_half });
    float board_w = tr.x - bl.x;
    float board_h = tr.y - bl.y;
    DrawRectangle((int)bl.x, (int)bl.y, (int)board_w, (int)board_h, COLOR_BOARD);
    
    // Cushions
    float cushion_t = math_world_to_screen_dist(vp, CUSHION_THICKNESS);
    
    // Top cushion
    DrawRectangle((int)bl.x, (int)bl.y, (int)board_w, (int)cushion_t, COLOR_CUSHION);
    // Bottom cushion
    DrawRectangle((int)bl.x, (int)(tr.y - cushion_t), (int)board_w, (int)cushion_t, COLOR_CUSHION);
    // Left cushion
    DrawRectangle((int)bl.x, (int)bl.y, (int)cushion_t, (int)board_h, COLOR_CUSHION);
    // Right cushion
    DrawRectangle((int)(tr.x - cushion_t), (int)bl.y, (int)cushion_t, (int)board_h, COLOR_CUSHION);
    
    // Center circle
    Vec2 center = math_world_to_screen(vp, (Vec2){0, 0});
    float circle_r = math_world_to_screen_dist(vp, 0.08f);
    DrawCircleLines((int)center.x, (int)center.y, circle_r, COLOR_LINE);
    
    // Baselines
    float baseline_y_n = math_world_to_screen(vp, (Vec2){0, BASELINE_Y_NORTH}).y;
    float baseline_y_s = math_world_to_screen(vp, (Vec2){0, BASELINE_Y_SOUTH}).y;
    float baseline_x_e = math_world_to_screen(vp, (Vec2){BASELINE_X_EAST, 0}).x;
    float baseline_x_w = math_world_to_screen(vp, (Vec2){BASELINE_X_WEST, 0}).x;
    
    float baseline_len = math_world_to_screen_dist(vp, BASELINE_MAX_OFFSET * 2);
    float baseline_x_start = center.x - baseline_len * 0.5f;
    float baseline_y_start = center.y - baseline_len * 0.5f;
    
    DrawLine((int)baseline_x_start, (int)baseline_y_n, (int)(baseline_x_start + baseline_len), (int)baseline_y_n, COLOR_BASELINE);
    DrawLine((int)baseline_x_start, (int)baseline_y_s, (int)(baseline_x_start + baseline_len), (int)baseline_y_s, COLOR_BASELINE);
    DrawLine((int)baseline_x_e, (int)baseline_y_start, (int)baseline_x_e, (int)(baseline_y_start + baseline_len), COLOR_BASELINE);
    DrawLine((int)baseline_x_w, (int)baseline_y_start, (int)baseline_x_w, (int)(baseline_y_start + baseline_len), COLOR_BASELINE);
    
    // Pockets
    float pocket_r = math_world_to_screen_dist(vp, POCKET_RADIUS_NORM);
    for (int i = 0; i < 4; i++) {
        Vec2 p = math_world_to_screen(vp, POCKET_CENTERS[i]);
        DrawCircle((int)p.x, (int)p.y, pocket_r, COLOR_POCKET);
    }
    
    // Pieces (from physics for smooth animation, fall back to board state)
    for (int i = 0; i < MAX_PIECES; i++) {
        if (!board->pieces[i].on_board) continue;
        
        Vec2 pos;
        if (use_physics && (physics_positions[i].x != 0 || physics_positions[i].y != 0)) {
            pos = physics_positions[i];
        } else {
            pos = board->pieces[i].position;
        }
        
        Vec2 screen = math_world_to_screen(vp, pos);
        float piece_r = math_world_to_screen_dist(vp, PIECE_RADIUS_NORM);
        
        Color c;
        if (board->pieces[i].color == PIECE_WHITE) c = COLOR_WHITE_PIECE;
        else if (board->pieces[i].color == PIECE_BLACK) c = COLOR_BLACK_PIECE;
        else c = COLOR_QUEEN;
        
        DrawCircle((int)screen.x, (int)screen.y, piece_r, c);
        DrawCircleLines((int)screen.x, (int)screen.y, piece_r, COLOR_LINE);
    }
    
    // Striker
    if (board->striker.on_baseline && !board->striker.pocketed) {
        Vec2 pos = use_physics ? striker_pos : board->striker.position;
        Vec2 screen = math_world_to_screen(vp, pos);
        float striker_r = math_world_to_screen_dist(vp, STRIKER_RADIUS_NORM);
        DrawCircle((int)screen.x, (int)screen.y, striker_r, COLOR_STRIKER);
        DrawCircleLines((int)screen.x, (int)screen.y, striker_r, COLOR_LINE);
    }
}
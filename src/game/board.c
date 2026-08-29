#include "board.h"
#include "types.h"
#include "math.h"
#include <stdlib.h>
#include <string.h>

/* -----------------------------------------------------------------------------
 * Board State Initialization
 * --------------------------------------------------------------------------- */
void board_state_init(BoardState* board) {
    memset(board, 0, sizeof(BoardState));
    for (int i = 0; i < MAX_PIECES; i++) {
        board->pieces[i].id = (uint8_t)i;
        board->pieces[i].color = (i < 9) ? PIECE_WHITE : (i == QUEEN_ID ? PIECE_QUEEN : PIECE_BLACK);
        board->pieces[i].position = (Vec2){0, 0};
        board->pieces[i].velocity = (Vec2){0, 0};
        board->pieces[i].on_board = false;
        board->pieces[i].pocketed = false;
    }
    board->striker.position = (Vec2){0, 0};
    board->striker.velocity = (Vec2){0, 0};
    board->striker.on_baseline = false;
    board->striker.pocketed = false;
    board->striker.owner_seat = SEAT_NORTH;
    board->queen_state = QUEEN_STATE_ON_BOARD;
    board->white_on_board = 0;
    board->black_on_board = 0;
    board->queen_on_board = false;
    board->white_dues = 0;
    board->black_dues = 0;
    board->queen_dues = 0;
}

void striker_state_init(StrikerState* striker, Seat seat) {
    striker->position = (Vec2){0, 0};
    striker->velocity = (Vec2){0, 0};
    striker->on_baseline = true;
    striker->pocketed = false;
    striker->owner_seat = seat;
}

/* -----------------------------------------------------------------------------
 * Initial Piece Formation (Center Circle)
 * --------------------------------------------------------------------------- */
#define INITIAL_RADIUS 0.08f  // Radius of center circle formation
#define QUEEN_CENTER_OFFSET 0.0f  // Queen at exact center

static const Vec2 INITIAL_POSITIONS_WHITE[9] = {
    { 0.00f,  0.00f },  // Will be replaced by queen
    {  INITIAL_RADIUS,  0.00f },
    { -INITIAL_RADIUS,  0.00f },
    {  0.00f,  INITIAL_RADIUS },
    {  0.00f, -INITIAL_RADIUS },
    {  INITIAL_RADIUS * 0.707f,  INITIAL_RADIUS * 0.707f },
    { -INITIAL_RADIUS * 0.707f,  INITIAL_RADIUS * 0.707f },
    {  INITIAL_RADIUS * 0.707f, -INITIAL_RADIUS * 0.707f },
    { -INITIAL_RADIUS * 0.707f, -INITIAL_RADIUS * 0.707f }
};

static const Vec2 INITIAL_POSITIONS_BLACK[9] = {
    {  INITIAL_RADIUS * 1.5f,  0.00f },
    { -INITIAL_RADIUS * 1.5f,  0.00f },
    {  0.00f,  INITIAL_RADIUS * 1.5f },
    {  0.00f, -INITIAL_RADIUS * 1.5f },
    {  INITIAL_RADIUS * 1.06f,  INITIAL_RADIUS * 1.06f },
    { -INITIAL_RADIUS * 1.06f,  INITIAL_RADIUS * 1.06f },
    {  INITIAL_RADIUS * 1.06f, -INITIAL_RADIUS * 1.06f },
    { -INITIAL_RADIUS * 1.06f, -INITIAL_RADIUS * 1.06f },
    {  0.00f,  0.00f }  // Placeholder
};

void board_setup_initial_formation(BoardState* board, RNGContext* rng) {
    // Place queen at center
    board->pieces[QUEEN_ID].position = (Vec2){0.0f, 0.0f};
    board->pieces[QUEEN_ID].velocity = (Vec2){0.0f, 0.0f};
    board->pieces[QUEEN_ID].on_board = true;
    board->pieces[QUEEN_ID].pocketed = false;
    board->queen_on_board = true;
    
    // Place white pieces (IDs 0-8)
    for (int i = 0; i < 9; i++) {
        board->pieces[i].position = INITIAL_POSITIONS_WHITE[i];
        board->pieces[i].velocity = (Vec2){0.0f, 0.0f};
        board->pieces[i].on_board = true;
        board->pieces[i].pocketed = false;
    }
    // Fix: white piece 0 should not be at center (queen is there)
    // Shift it slightly
    board->pieces[0].position = (Vec2){ INITIAL_RADIUS * 0.5f, INITIAL_RADIUS * 0.5f };
    
    // Place black pieces (IDs 9-17)
    for (int i = 9; i < 18; i++) {
        int idx = i - 9;
        board->pieces[i].position = INITIAL_POSITIONS_BLACK[idx];
        board->pieces[i].velocity = (Vec2){0.0f, 0.0f};
        board->pieces[i].on_board = true;
        board->pieces[i].pocketed = false;
    }
    
    board->white_on_board = 9;
    board->black_on_board = 9;
    board->queen_state = QUEEN_STATE_ON_BOARD;
}

void board_place_striker_on_baseline(StrikerState* striker, Seat seat) {
    // Default to center of baseline
    Vec2 pos = {0, 0};
    
    switch (seat) {
        case SEAT_NORTH:
            pos.x = 0.0f;
            pos.y = BASELINE_Y_NORTH;
            break;
        case SEAT_SOUTH:
            pos.x = 0.0f;
            pos.y = BASELINE_Y_SOUTH;
            break;
        case SEAT_EAST:
            pos.x = BASELINE_X_EAST;
            pos.y = 0.0f;
            break;
        case SEAT_WEST:
            pos.x = BASELINE_X_WEST;
            pos.y = 0.0f;
            break;
    }
    
    striker->position = pos;
    striker->velocity = (Vec2){0, 0};
    striker->on_baseline = true;
    striker->pocketed = false;
}

int board_get_legal_placements(Seat seat, Vec2* out_placements, int max_placements) {
    int count = 0;
    float baseline_coord, min_offset, max_offset;
    bool horizontal;
    
    switch (seat) {
        case SEAT_NORTH:
        case SEAT_SOUTH:
            horizontal = true;
            baseline_coord = (seat == SEAT_NORTH) ? BASELINE_Y_NORTH : BASELINE_Y_SOUTH;
            min_offset = -BASELINE_MAX_OFFSET;
            max_offset = BASELINE_MAX_OFFSET;
            break;
        case SEAT_EAST:
        case SEAT_WEST:
            horizontal = false;
            baseline_coord = (seat == SEAT_EAST) ? BASELINE_X_EAST : BASELINE_X_WEST;
            min_offset = -BASELINE_MAX_OFFSET;
            max_offset = BASELINE_MAX_OFFSET;
            break;
    }
    
    // Generate PLACEMENTS_PER_SEAT positions along baseline
    int num_placements = (max_placements < 8) ? max_placements : 8;
    for (int i = 0; i < num_placements; i++) {
        float t = (float)i / (float)(num_placements - 1);
        float offset = math_lerp(min_offset, max_offset, t);
        
        Vec2 pos;
        if (horizontal) {
            pos = (Vec2){ offset, baseline_coord };
        } else {
            pos = (Vec2){ baseline_coord, offset };
        }
        
        if (board_is_legal_placement(seat, pos)) {
            out_placements[count++] = pos;
        }
    }
    
    return count;
}

bool board_is_legal_placement(Seat seat, Vec2 pos) {
    // Check distance from pockets (must not overlap pocket sensor radius)
    float pocket_sensor_radius = POCKET_RADIUS_NORM + STRIKER_RADIUS_NORM;
    
    for (int i = 0; i < 4; i++) {
        float dx = pos.x - POCKET_CENTERS[i].x;
        float dy = pos.y - POCKET_CENTERS[i].y;
        if (dx*dx + dy*dy < pocket_sensor_radius * pocket_sensor_radius) {
            return false;  // Too close to pocket
        }
    }
    
    // Check baseline bounds
    switch (seat) {
        case SEAT_NORTH:
            if (math_fabsf(pos.y - BASELINE_Y_NORTH) > 0.01f) return false;
            if (pos.x < -BASELINE_MAX_OFFSET || pos.x > BASELINE_MAX_OFFSET) return false;
            break;
        case SEAT_SOUTH:
            if (math_fabsf(pos.y - BASELINE_Y_SOUTH) > 0.01f) return false;
            if (pos.x < -BASELINE_MAX_OFFSET || pos.x > BASELINE_MAX_OFFSET) return false;
            break;
        case SEAT_EAST:
            if (math_fabsf(pos.x - BASELINE_X_EAST) > 0.01f) return false;
            if (pos.y < -BASELINE_MAX_OFFSET || pos.y > BASELINE_MAX_OFFSET) return false;
            break;
        case SEAT_WEST:
            if (math_fabsf(pos.x - BASELINE_X_WEST) > 0.01f) return false;
            if (pos.y < -BASELINE_MAX_OFFSET || pos.y > BASELINE_MAX_OFFSET) return false;
            break;
    }
    
    return true;
}

const PieceState* board_get_piece(const BoardState* board, uint8_t id) {
    if (id < MAX_PIECES) {
        return &board->pieces[id];
    }
    return NULL;
}

int board_count_on_board(const BoardState* board, PieceColor color) {
    int count = 0;
    for (int i = 0; i < MAX_PIECES; i++) {
        if (board->pieces[i].on_board && board->pieces[i].color == color) {
            count++;
        }
    }
    return count;
}

void board_sync_from_physics(BoardState* board, const PhysicsSnapshot* phys) {
    // Stub - will be implemented when physics_snapshot is ready
    (void)board;
    (void)phys;
}
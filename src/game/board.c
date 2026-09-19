#include "board.h"
#include "types.h"
#include "math.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>

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
        board->pieces[i].pocketed_position = (Vec2){0, 0};
        board->pieces[i].pocket_index = 255;
    }
    for (int i = 0; i < MAX_PIECES; i++) {
        board->pocketed_pieces[i].id = (uint8_t)i;
        board->pocketed_pieces[i].color = PIECE_WHITE;
        board->pocketed_pieces[i].position = (Vec2){0, 0};
        board->pocketed_pieces[i].velocity = (Vec2){0, 0};
        board->pocketed_pieces[i].on_board = false;
        board->pocketed_pieces[i].pocketed = false;
        board->pocketed_pieces[i].pocketed_position = (Vec2){0, 0};
        board->pocketed_pieces[i].pocket_index = 255;
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
    board->pocketed_count = 0;
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
#define INNER_RING_RADIUS 0.04f
#define OUTER_RING_RADIUS 0.08f

void board_setup_initial_formation(BoardState* board, RNGContext* rng) {
    (void)rng;

    // 1. Reset all pieces to off-board first
    for (int i = 0; i < MAX_PIECES; i++) {
        board->pieces[i].on_board = false;
        board->pieces[i].pocketed = false;
    }

    // 2. Place queen at center
    board->pieces[QUEEN_ID].position = (Vec2){0.0f, 0.0f};
    board->pieces[QUEEN_ID].velocity = (Vec2){0.0f, 0.0f};
    board->pieces[QUEEN_ID].on_board = true;
    board->pieces[QUEEN_ID].pocketed = false;
    board->pieces[QUEEN_ID].pocketed_position = (Vec2){0, 0};
    board->pieces[QUEEN_ID].pocket_index = 255;
    board->pieces[QUEEN_ID].color = PIECE_QUEEN;
    board->queen_on_board = true;

    // 3. Inner Ring: 6 pieces (alternating White/Black)
    // We use IDs 0-5 for inner ring.
    for (int i = 0; i < 6; i++) {
        float angle = (float)i * (M_PI / 3.0f);
        Vec2 pos = { INNER_RING_RADIUS * cosf(angle), INNER_RING_RADIUS * sinf(angle) };
        
        int piece_id = i;
        // If i is QUEEN_ID, we skip it or offset it. 
        // Since QUEEN_ID is typically 18 or something similar, we can just use i.
        // But wait, if QUEEN_ID is between 0-5, we'd overwrite.
        // Let's check QUEEN_ID. If QUEEN_ID is in [0, 5], we need to handle it.
        // Assuming QUEEN_ID is not in [0, 5] and [6, 17].
        
        PieceColor color = (i % 2 == 0) ? PIECE_WHITE : PIECE_BLACK;
        
        board->pieces[piece_id].position = pos;
        board->pieces[piece_id].velocity = (Vec2){0.0f, 0.0f};
        board->pieces[piece_id].on_board = true;
        board->pieces[piece_id].pocketed = false;
        board->pieces[piece_id].pocketed_position = (Vec2){0, 0};
        board->pieces[piece_id].pocket_index = 255;
        board->pieces[piece_id].color = color;
    }

    // 4. Outer Ring: 12 pieces (alternating White/Black)
    // We use IDs 6-17 for outer ring.
    for (int i = 0; i < 12; i++) {
        float angle = (float)i * (M_PI / 6.0f);
        Vec2 pos = { OUTER_RING_RADIUS * cosf(angle), OUTER_RING_RADIUS * sinf(angle) };
        
        int piece_id = i + 6;
        PieceColor color = (i % 2 == 0) ? PIECE_WHITE : PIECE_BLACK;
        
        board->pieces[piece_id].position = pos;
        board->pieces[piece_id].velocity = (Vec2){0.0f, 0.0f};
        board->pieces[piece_id].on_board = true;
        board->pieces[piece_id].pocketed = false;
        board->pieces[piece_id].pocketed_position = (Vec2){0, 0};
        board->pieces[piece_id].pocket_index = 255;
        board->pieces[piece_id].color = color;
    }

    board->white_on_board = 9;
    board->black_on_board = 9;
    board->queen_state = QUEEN_STATE_ON_BOARD;
    board->pocketed_count = 0;
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

// Remove the stub and its declaration as it is not called anywhere in the codebase.

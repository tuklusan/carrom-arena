#include <math.h>
#include "board.h"
#include "types.h"
#include "common/vecmath.h"
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

    // 3. Generate ICF hexagonal positions
    Vec2 positions[18];
    float r = PIECE_RADIUS_NORM;
    int pos_idx = 0;

    // Inner ring: R = 2r, 6 pieces, 60deg increments
    for (int i = 0; i < 6; i++) {
        float angle = (float)i * (M_PI / 3.0f);
        positions[pos_idx++] = math_vec2_from_angle(angle);
        positions[pos_idx-1].x *= 2.0f * r;
        positions[pos_idx-1].y *= 2.0f * r;
    }

    // Outer ring Tips: R = 4r, 6 pieces, 60deg increments
    for (int i = 0; i < 6; i++) {
        float angle = (float)i * (M_PI / 3.0f);
        positions[pos_idx++] = math_vec2_from_angle(angle);
        positions[pos_idx-1].x *= 4.0f * r;
        positions[pos_idx-1].y *= 4.0f * r;
    }

    // Outer ring Notches: R = 2*sqrt(3)*r, 6 pieces, 30deg, 90deg...
    for (int i = 0; i < 6; i++) {
        float angle = (float)i * (M_PI / 3.0f) + (M_PI / 6.0f);
        positions[pos_idx++] = math_vec2_from_angle(angle);
        float scale = 2.0f * math_sqrtf(3.0f) * r;
        positions[pos_idx-1].x *= scale;
        positions[pos_idx-1].y *= scale;
    }

    // 4. Assign IDs by color: 0-8 White, 9-17 Black
    // To meet ICF spec, colors must alternate within rings.
    
    int white_count = 0;
    int black_count = 0;
    
    for (int i = 0; i < 18; i++) {
        PieceColor color;
        if (i < 6) {
            // Inner ring: alternate W, B, W, B, W, B
            color = (i % 2 == 0) ? PIECE_WHITE : PIECE_BLACK;
        } else if (i < 12) {
            // Outer ring tips: alternate W, B, W, B, W, B
            color = ((i - 6) % 2 == 0) ? PIECE_WHITE : PIECE_BLACK;
        } else {
            // Outer ring notches: alternate W, B, W, B, W, B
            color = ((i - 12) % 2 == 0) ? PIECE_WHITE : PIECE_BLACK;
        }

        int piece_id;
        if (color == PIECE_WHITE) {
            piece_id = white_count++;
        } else {
            piece_id = 9 + black_count++;
        }
        
        board->pieces[piece_id].position = positions[i];
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
    float baseline_coord = 0.0f, min_offset, max_offset;
    bool horizontal = true;
    
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
    if (num_placements < 2) return 0;   /* the spacing below divides by (num_placements - 1) */
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
    /* The pocket sensor (POCKET_CAPTURE_RADIUS_NORM = pocket + coin radius, see physics.h) captures anything it
     * overlaps, so a striker centre must stay outside sensor radius + striker radius, or it is "pocketed" the
     * moment it is placed. (The old bound, pocket + striker radius, let positions up to 2 cm too deep through.) */
    float pocket_sensor_radius = POCKET_RADIUS_NORM + PIECE_RADIUS_NORM + STRIKER_RADIUS_NORM + 0.003f;
    
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

void board_apply_final_positions(BoardState* board, const Vec2* positions) {
    for (int i = 0; i < MAX_PIECES; i++) {
        if (!board->pieces[i].on_board || board->pieces[i].pocketed) continue;
        board->pieces[i].position = positions[i];
        board->pieces[i].velocity = (Vec2){ 0.0f, 0.0f };
    }
}

void board_remove_from_stash(BoardState* board, int piece_id) {
    int at = -1;
    for (int q = 0; q < board->pocketed_count; q++) {
        if (board->pocketed_pieces[q].id == piece_id) { at = q; break; }
    }
    if (at < 0) return;
    for (int q = at; q + 1 < board->pocketed_count; q++) board->pocketed_pieces[q] = board->pocketed_pieces[q + 1];
    board->pocketed_count--;
    /* re-line every coin up in its pocket's corner, in the order it fell in */
    int nth[4] = {0, 0, 0, 0};
    for (int q = 0; q < board->pocketed_count; q++) {
        int pk = board->pocketed_pieces[q].pocket_index;
        if (pk > 3) continue;
        Vec2 pos = board_stash_position(pk, nth[pk]++);
        board->pocketed_pieces[q].pocketed_position = pos;
        int id = board->pocketed_pieces[q].id;
        if (id < MAX_PIECES) board->pieces[id].pocketed_position = pos;
    }
}

Vec2 board_find_free_spot(const BoardState* board, Vec2 target) {
    const float gap = 2.0f * PIECE_RADIUS_NORM - 0.0005f;    /* touching coins are fine: the rack itself packs them edge to edge */
    const float step = 2.0f * PIECE_RADIUS_NORM + 0.002f;
    const float limit = 0.5f - CUSHION_THICKNESS - PIECE_RADIUS_NORM - 0.01f;
    Vec2 best = target;
    float best_d = 1e9f;
    for (int ring = 0; ring <= 8; ring++) {
        int samples = (ring == 0) ? 1 : 12 * ring;
        for (int k = 0; k < samples; k++) {
            float ang = 2.0f * (float)M_PI * (float)k / (float)samples;
            Vec2 c = { target.x + (float)ring * step * cosf(ang), target.y + (float)ring * step * sinf(ang) };
            if (fabsf(c.x) > limit || fabsf(c.y) > limit) continue;
            bool free_spot = true;
            for (int i = 0; i < MAX_PIECES && free_spot; i++) {
                if (!board->pieces[i].on_board || board->pieces[i].pocketed) continue;
                float dx = board->pieces[i].position.x - c.x, dy = board->pieces[i].position.y - c.y;
                if (dx * dx + dy * dy < gap * gap) free_spot = false;
            }
            if (free_spot) {
                float d = (c.x - target.x) * (c.x - target.x) + (c.y - target.y) * (c.y - target.y);
                if (d < best_d) { best_d = d; best = c; }
            }
        }
        if (best_d < 1e8f) break;   /* the nearest ring that has a free spot wins */
    }
    return best;
}

void board_apply_shot_positions(BoardState* board, const ShotResult* result) {
    for (int i = 0; i < MAX_PIECES; i++) {
        if (!board->pieces[i].on_board || board->pieces[i].pocketed) continue;
        bool pocketed_now = false;
        for (int k = 0; k < result->pocketed_count; k++) if (result->pocketed_ids[k] == i) pocketed_now = true;
        if (pocketed_now) continue;
        board->pieces[i].position = result->final_positions[i];
        board->pieces[i].velocity = (Vec2){ 0.0f, 0.0f };
    }
}

bool board_find_due_spot(const BoardState* board, Vec2* out) {
    const float gap = 2.0f * PIECE_RADIUS_NORM + 0.002f;
    const float r_min = CENTRE_CIRCLE_RADIUS + PIECE_RADIUS_NORM + 0.002f;
    const float r_max = OUTER_CIRCLE_RADIUS - PIECE_RADIUS_NORM;
    bool found = false;
    float best = -1.0f;
    for (float r = r_min; r <= r_max; r += 0.008f) {
        for (int k = 0; k < 36; k++) {
            float ang = 2.0f * (float)M_PI * (float)k / 36.0f;
            Vec2 c = { r * cosf(ang), r * sinf(ang) };
            bool free_spot = true;
            for (int i = 0; i < MAX_PIECES && free_spot; i++) {
                if (!board->pieces[i].on_board || board->pieces[i].pocketed) continue;
                float dx = board->pieces[i].position.x - c.x, dy = board->pieces[i].position.y - c.y;
                if (dx * dx + dy * dy < gap * gap) free_spot = false;
            }
            if (!free_spot) continue;
            float near_pocket = 1e9f;
            for (int p = 0; p < 4; p++) {
                float dx = POCKET_CENTERS[p].x - c.x, dy = POCKET_CENTERS[p].y - c.y;
                float d = dx * dx + dy * dy;
                if (d < near_pocket) near_pocket = d;
            }
            if (near_pocket > best) { best = near_pocket; *out = c; found = true; }
        }
    }
    return found;
}

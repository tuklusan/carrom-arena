#ifndef CARROM_BOARD_H
#define CARROM_BOARD_H

#include "types.h"

#ifdef __cplusplus
extern "C" {
#endif

/* -----------------------------------------------------------------------------
 * Board Geometry & Piece Inventory
 * --------------------------------------------------------------------------- */

/* Board state initialization */
void board_state_init(BoardState* board);
void board_setup_initial_formation(BoardState* board, RNGContext* rng);
void board_place_striker_on_baseline(StrikerState* striker, Seat seat);
void striker_state_init(StrikerState* striker, Seat seat);

/* After a shot settles: record where the coins really are (positions[i] per piece id), so the game state, the AI and
 * the shot planner see the current board and not the initial rack. Coins off the board are left alone. */
void board_apply_final_positions(BoardState* board, const Vec2* positions);

/* Where the nth coin (0-based) pocketed in pocket `pocket_index` is stashed: a 3x3 lineup in the outside corner beside
 * the pocket, coins touching but never overlapping, growing away from the board. */
static inline Vec2 board_stash_position(int pocket_index, int nth) {
    const float first = 0.5f + PIECE_RADIUS_NORM + 0.012f;
    const float step = 2.0f * PIECE_RADIUS_NORM + 0.004f;
    float sx = (POCKET_CENTERS[pocket_index].x < 0.0f) ? -1.0f : 1.0f;
    float sy = (POCKET_CENTERS[pocket_index].y > 0.0f) ? 1.0f : -1.0f;
    return (Vec2){ sx * (first + (float)(nth % 3) * step), sy * (first + (float)((nth / 3) % 3) * step) };
}

/* Take a coin out of the stash (it goes back on the board) and close the gap in its lineup */
void board_remove_from_stash(BoardState* board, int piece_id);

/* Get legal baseline placement positions for a seat */
int board_get_legal_placements(Seat seat, Vec2* out_placements, int max_placements);

/* Check if position is valid baseline placement */
bool board_is_legal_placement(Seat seat, Vec2 pos);

/* Piece queries */

/* Game/Match state initialization */
void game_state_init(GameState* game, uint64_t seed);
void match_state_init(MatchState* match);
void rules_outcome_init(RulesOutcome* outcome);
void shot_plan_init(ShotPlan* plan);
void shot_result_init(ShotResult* result);

#ifdef __cplusplus
}
#endif

#endif // CARROM_BOARD_H
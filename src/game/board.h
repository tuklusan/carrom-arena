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

/* Get legal baseline placement positions for a seat */
int board_get_legal_placements(Seat seat, Vec2* out_placements, int max_placements);

/* Check if position is valid baseline placement */
bool board_is_legal_placement(Seat seat, Vec2 pos);

/* Piece queries */
const PieceState* board_get_piece(const BoardState* board, uint8_t id);
int board_count_on_board(const BoardState* board, PieceColor color);

/* Update piece positions from physics */
void board_sync_from_physics(BoardState* board, const PhysicsSnapshot* phys);

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
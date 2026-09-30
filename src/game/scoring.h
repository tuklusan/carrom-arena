/*
 * Copyright (c) 2026 Supratim Sanyal of SANYALnet Labs.
 * Licensed under the SANYALnet Labs Non-Commercial License; see LICENSE.
 */

#ifndef CARROM_SCORING_H
#define CARROM_SCORING_H

#include "types.h"

#ifdef __cplusplus
extern "C" {
#endif

/* -----------------------------------------------------------------------------
 * Scoring Rules
 * --------------------------------------------------------------------------- */


int scoring_queen_points(bool covered);

/* The live score shown on the scoreboard for one pair (0 = red, the N/S pair; 1 = blue, E/W) on the CURRENT board: the coins of the
 * pair's colour that are in a pocket (a coin the rules put back stops counting at once) plus 3 once the pair has covered the queen.
 * The white coins are the red pair's unless the board is swapped (E/W broke). */
int scoring_live_board_points(const BoardState* b, int pair);


#ifdef __cplusplus
}
#endif

#endif // CARROM_SCORING_H
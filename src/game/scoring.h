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


#ifdef __cplusplus
}
#endif

#endif // CARROM_SCORING_H
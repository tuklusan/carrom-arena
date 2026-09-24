#include "scoring.h"
#include "common/types.h"

int scoring_queen_points(bool covered) {
    return covered ? 3 : 0;  // 3 points for covered queen, 0 if not covered (goes to due)
}
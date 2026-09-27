#include "scoring.h"
#include "common/types.h"

int scoring_queen_points(bool covered) {
    return covered ? 3 : 0;  // 3 points for covered queen, 0 if not covered (goes to due)
}
int scoring_live_board_points(const BoardState* b, int pair) {
    PieceColor mine = ((pair == 0) != b->seats_swapped) ? PIECE_WHITE : PIECE_BLACK;
    int pts = 0;
    for (int i = 0; i < QUEEN_ID; i++) {
        if (b->pieces[i].color == mine && !b->pieces[i].on_board) pts++;
    }
    if (b->queen_state == QUEEN_STATE_COVERED && b->queen_covered_team == (mine == PIECE_WHITE ? 1 : 2)) pts += scoring_queen_points(true);
    return pts;
}

#include "scoring.h"
#include "common/types.h"

void scoring_calculate(const ShotFacts* facts, const GameState* game, TeamScores* delta) {
    delta->white = 0;
    delta->black = 0;
    
    Team active_team = (facts->active_seat == SEAT_NORTH || facts->active_seat == SEAT_SOUTH) ? TEAM_WHITE : TEAM_BLACK;
    
    // Count pocketed pieces by color
    int white_pocketed = 0;
    int black_pocketed = 0;
    
    for (int i = 0; i < facts->pocketed_count; i++) {
        if (facts->pocketed_colors[i] == PIECE_WHITE) white_pocketed++;
        else if (facts->pocketed_colors[i] == PIECE_BLACK) black_pocketed++;
    }
    
    // Active team scores for their own pieces
    if (active_team == TEAM_WHITE) {
        delta->white += white_pocketed;
    } else {
        delta->black += black_pocketed;
    }
    
    // Queen scoring
    if (facts->queen_pocketed) {
        bool covered = scoring_is_queen_covered(facts, active_team);
        int queen_pts = scoring_queen_points(covered);
        
        if (active_team == TEAM_WHITE) {
            delta->white += queen_pts;
        } else {
            delta->black += queen_pts;
        }
    }
}

bool scoring_is_queen_covered(const ShotFacts* facts, Team active_team) {
    // Queen is covered if active team pocketed at least one own piece in same shot
    if (active_team == TEAM_WHITE) {
        for (int i = 0; i < facts->pocketed_count; i++) {
            if (facts->pocketed_colors[i] == PIECE_WHITE) return true;
        }
    } else {
        for (int i = 0; i < facts->pocketed_count; i++) {
            if (facts->pocketed_colors[i] == PIECE_BLACK) return true;
        }
    }
    return false;
}

int scoring_queen_points(bool covered) {
    return covered ? 3 : 0;  // 3 points for covered queen, 0 if not covered (goes to due)
}

void scoring_apply_dues(BoardState* board, Team team, int count) {
    if (team == TEAM_WHITE) {
        board->white_dues += (uint8_t)count;
    } else {
        board->black_dues += (uint8_t)count;
    }
}
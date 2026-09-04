#define __USE_MINGW_ANSI_STDIO 1
#include <math.h>
#ifdef __MINGW32__
extern float sqrtf(float);
#endif
#include "shot_evaluator.h"
#include "types.h"
#include "math.h"
#include "rng.h"
#include "physics/physics.h"
#include "physics/physics_snapshot.h"
#include "board.h"
#include "rules.h"
#include <stdlib.h>

/* -----------------------------------------------------------------------------
 * Scratch Simulation
 * --------------------------------------------------------------------------- */
void shot_evaluator_evaluate(ShotCandidate* candidate, const DecisionSnapshot* snap, PCG32* rng) {
    (void)rng;
    // Create physics world from snapshot
    PhysicsWorld* sim_world = physics_world_from_snapshot(snap->physics);
    if (!sim_world) {
        candidate->sim_valid = false;
        return;
    }
    
    // Execute shot in simulation
    physics_place_striker(sim_world, snap->active_seat, candidate->plan.placement);
    physics_apply_shot(sim_world, candidate->plan.aim_angle, candidate->plan.power);
    
    // Simulate until settled or timeout
    float sim_time = 0.0f;
#ifndef MAX_SIM_TIME
    const float MAX_SIM_TIME = 4.0f;
#else
    const float MAX_SIM_TIME = MAX_SIM_TIME_VAL;
#endif
    const float SIM_DT = PHYSICS_DT;
    
    while (sim_time < MAX_SIM_TIME) {
        physics_step(sim_world, SIM_DT);
        sim_time += SIM_DT;
        
        if (physics_is_settled(sim_world)) {
            break;
        }
    }
    
    // Collect result
    physics_collect_pocketed(sim_world, &candidate->sim_result);
    physics_get_final_positions(sim_world, candidate->sim_result.final_positions);
    candidate->sim_result.sim_time = sim_time;
    candidate->sim_valid = true;
    
    // Cleanup
    physics_destroy(sim_world);
}

/* -----------------------------------------------------------------------------
 * Scoring Components
 * --------------------------------------------------------------------------- */
float score_pocket_value(const ShotResult* result, Team team, const StrategyProfile* profile) {
    float score = 0.0f;
    
    for (int i = 0; i < result->pocketed_count; i++) {
        PieceColor color = result->pocketed_colors[i];
        bool is_own = (team == TEAM_WHITE && color == PIECE_WHITE) || 
                      (team == TEAM_BLACK && color == PIECE_BLACK);
        
        if (is_own) {
            score += profile->weight_pocket;
        } else if (color == PIECE_QUEEN) {
            // Queen handled separately
        } else {
            // Opponent piece pocketed - slight negative (helps opponent)
            score -= profile->weight_pocket * 0.5f;
        }
    }
    
    return score;
}

float score_queen_value(const ShotResult* result, const BoardState* board, Team team, const StrategyProfile* profile) {
    if (!result->queen_pocketed) return 0.0f;
    
    // Check if queen covered in this shot
    bool covered = false;
    for (int i = 0; i < result->pocketed_count; i++) {
        bool is_own = (team == TEAM_WHITE && result->pocketed_colors[i] == PIECE_WHITE) ||
                      (team == TEAM_BLACK && result->pocketed_colors[i] == PIECE_BLACK);
        if (is_own) {
            covered = true;
            break;
        }
    }
    
    if (covered) {
        return profile->weight_queen + profile->weight_cover;
    } else {
        // Queen pocketed but not covered - risky, goes to due
        return profile->weight_queen * 0.3f;  // Reduced value
    }
}

float score_cover_bonus(const ShotResult* result, const BoardState* board, Team team, const StrategyProfile* profile) {
    if (board->queen_state == QUEEN_STATE_POCKETED_NO_COVER) {
        // Need to cover queen
        for (int i = 0; i < result->pocketed_count; i++) {
            bool is_own = (team == TEAM_WHITE && result->pocketed_colors[i] == PIECE_WHITE) ||
                          (team == TEAM_BLACK && result->pocketed_colors[i] == PIECE_BLACK);
            if (is_own) {
                return profile->weight_cover;
            }
        }
    }
    return 0.0f;
}

float score_striker_risk(const ShotResult* result, const StrategyProfile* profile) {
    if (result->striker_pocketed) {
        return profile->weight_striker_risk;  // Negative value
    }
    return 0.0f;
}

float score_opponent_leave(const ShotResult* result, const BoardState* board, Team team, const StrategyProfile* profile) {
    // Penalize leaving easy shots for opponent
    // Simple heuristic: count opponent pieces near pockets
    float penalty = 0.0f;
    Team opponent = (team == TEAM_WHITE) ? TEAM_BLACK : TEAM_WHITE;
    PieceColor opp_color = (opponent == TEAM_WHITE) ? PIECE_WHITE : PIECE_BLACK;
    
    for (int i = 0; i < MAX_PIECES; i++) {
        if (board->pieces[i].on_board && board->pieces[i].color == opp_color) {
            Vec2 pos = result->final_positions[i];
            // Check distance to pockets
            for (int p = 0; p < 4; p++) {
                float dx = pos.x - POCKET_CENTERS[p].x;
                float dy = pos.y - POCKET_CENTERS[p].y;
                float dist = sqrtf(dx*dx + dy*dy);
                if (dist < 0.15f) {  // Near pocket
                    penalty += 1.0f;
                }
            }
        }
    }
    
    return penalty * profile->weight_opponent_leave;  // Negative weight
}

float score_positional(const ShotResult* result, const BoardState* board, Team team, const StrategyProfile* profile) {
    // Reward good positional play: own pieces centralized, opponent pieces scattered
    float score = 0.0f;
    PieceColor own_color = (team == TEAM_WHITE) ? PIECE_WHITE : PIECE_BLACK;
    PieceColor opp_color = (team == TEAM_WHITE) ? PIECE_BLACK : PIECE_WHITE;
    
    for (int i = 0; i < MAX_PIECES; i++) {
        if (board->pieces[i].on_board) {
            Vec2 pos = result->final_positions[i];
            float dist_from_center = sqrtf(pos.x * pos.x + pos.y * pos.y);
            
            if (board->pieces[i].color == own_color) {
                // Reward own pieces near center (control)
                score += (0.3f - dist_from_center) * 2.0f;
            } else if (board->pieces[i].color == opp_color) {
                // Reward opponent pieces near edges/cushions
                score += (dist_from_center - 0.2f) * 1.0f;
            }
        }
    }
    
    return score * profile->weight_positional;
}

/* -----------------------------------------------------------------------------
 * Total Score
 * --------------------------------------------------------------------------- */
void shot_evaluator_score_candidates(ShotCandidate* candidates, int count, const DecisionSnapshot* snap, const StrategyProfile* profile) {
    Team active_team = (snap->active_seat == SEAT_NORTH || snap->active_seat == SEAT_SOUTH) ? TEAM_WHITE : TEAM_BLACK;
    
    for (int i = 0; i < count; i++) {
        if (!candidates[i].sim_valid) {
            candidates[i].score = -1e9f;
            continue;
        }
        
        float score = 0.0f;
        score += score_pocket_value(&candidates[i].sim_result, active_team, profile);
        score += score_queen_value(&candidates[i].sim_result, snap->board, active_team, profile);
        score += score_cover_bonus(&candidates[i].sim_result, snap->board, active_team, profile);
        score += score_striker_risk(&candidates[i].sim_result, profile);
        score += score_opponent_leave(&candidates[i].sim_result, snap->board, active_team, profile);
        score += score_positional(&candidates[i].sim_result, snap->board, active_team, profile);
        
        candidates[i].score = score;
    }
}
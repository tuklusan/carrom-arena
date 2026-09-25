#define __USE_MINGW_ANSI_STDIO 1
#include <math.h>
#ifdef __MINGW32__
extern float sqrtf(float);
#endif
#include "shot_evaluator.h"
#include "types.h"
#include "vecmath.h"
#include "rng.h"
#include "physics/physics.h"
#include "physics/physics_snapshot.h"
#include "board.h"
#include "rules.h"
#include <stdlib.h>
#include "platform/platform.h"

/* -----------------------------------------------------------------------------
 * Scratch Simulation with Time Budget
 * --------------------------------------------------------------------------- */
void shot_evaluator_evaluate(ShotCandidate* candidate, const DecisionSnapshot* snap, PCG32* rng) {
    (void)rng;
    
    // Record start time for budget enforcement
    double start_time = platform_time_now();
    // Default budget: 250ms, can be overridden via environment or passed in
    // For now, use a reasonable default
    const double budget_seconds = 0.25;  // 250ms
    
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
    int sim_iter = 0;
    
    while (sim_time < MAX_SIM_TIME) {
        // Check wall-time budget
        double elapsed = platform_time_now() - start_time;
        if (elapsed > budget_seconds) {
            break;  // Time budget exceeded, return best-so-far
        }
        
        physics_step(sim_world, SIM_DT);
        sim_time += SIM_DT;
        sim_iter++;
        
        if (physics_is_settled(sim_world)) {
            break;
        }
        
        // Yield to OS every 8 simulation steps to keep host responsive (R5)
        if (sim_iter % 8 == 0) {
            platform_yield();
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
/* final_positions holds (0,0) for a coin that was pocketed in the simulation: such a coin must not be scored as if it lay
 * in the middle of the board */
static bool pocketed_in_result(const ShotResult* r, int id) {
    for (int k = 0; k < r->pocketed_count; k++) if (r->pocketed_ids[k] == id) return true;
    return false;
}

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
        if (board->pieces[i].on_board && board->pieces[i].color == opp_color && !pocketed_in_result(result, i)) {
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
        if (board->pieces[i].on_board && !pocketed_in_result(result, i)) {
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
    Team active_team = board_team_of_seat(snap->board, snap->active_seat);
    
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

        /* Progress: a shot that touches nothing achieves nothing (and, being deterministic, would be repeated by every
         * player forever), so it is penalised; moving own coins closer to a pocket is a small plus. */
        {
            PieceColor own = (active_team == TEAM_WHITE) ? PIECE_WHITE : PIECE_BLACK;
            const ShotResult* sr = &candidates[i].sim_result;
            float moved = 0.0f, progress = 0.0f;
            for (int p = 0; p < MAX_PIECES; p++) {
                const PieceState* pc = &snap->board->pieces[p];
                if (!pc->on_board || pc->pocketed) continue;
                bool pocketed_in_sim = false;
                for (int k = 0; k < sr->pocketed_count; k++) if (sr->pocketed_ids[k] == p) pocketed_in_sim = true;
                if (pocketed_in_sim) { moved += 1.0f; continue; }
                Vec2 a = sr->final_positions[p];
                moved += fabsf(a.x - pc->position.x) + fabsf(a.y - pc->position.y);
                if (pc->color == own) {
                    float before = 9.0f, after = 9.0f;
                    for (int q = 0; q < 4; q++) {
                        float db = hypotf(pc->position.x - POCKET_CENTERS[q].x, pc->position.y - POCKET_CENTERS[q].y);
                        float da = hypotf(a.x - POCKET_CENTERS[q].x, a.y - POCKET_CENTERS[q].y);
                        if (db < before) before = db;
                        if (da < after) after = da;
                    }
                    progress += before - after;
                }
            }
            if (moved < 0.002f) score -= 1.5f;
            else score += (progress > 0.4f ? 0.4f : (progress < -0.4f ? -0.4f : progress)) * 0.8f;
        }
        
        candidates[i].score = score;
    }
}
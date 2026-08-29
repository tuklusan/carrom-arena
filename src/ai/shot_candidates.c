#include "shot_candidates.h"
#include "types.h"
#include "math.h"
#include "board.h"
#include <math.h>
#include <stdlib.h>

/* -----------------------------------------------------------------------------
 * Legal Placements
 * --------------------------------------------------------------------------- */
int shot_candidates_placements(Seat seat, Vec2* out_placements, int max_placements) {
    return board_get_legal_placements(seat, out_placements, max_placements);
}

/* -----------------------------------------------------------------------------
 * Tactical Candidate Generation
 * --------------------------------------------------------------------------- */
static float angle_to_piece(Vec2 from, Vec2 to) {
    return atan2f(to.y - from.y, to.x - from.x);
}

static float angle_to_pocket(Vec2 from, int pocket_idx) {
    return angle_to_piece(from, POCKET_CENTERS[pocket_idx]);
}

int shot_candidates_tactical(const DecisionSnapshot* snap, Vec2 placement, ShotCandidate* out_candidates, int max_candidates, PCG32* rng) {
    (void)rng;
    int count = 0;
    const BoardState* board = snap->board;
    Seat seat = snap->active_seat;
    Team team = (seat == SEAT_NORTH || seat == SEAT_SOUTH) ? TEAM_WHITE : TEAM_BLACK;
    
    // Find target pieces (own color)
    Vec2 targets[19];
    int target_count = 0;
    for (int i = 0; i < MAX_PIECES; i++) {
        if (board->pieces[i].on_board && board->pieces[i].color == (team == TEAM_WHITE ? PIECE_WHITE : PIECE_BLACK)) {
            targets[target_count++] = board->pieces[i].position;
        }
    }
    
    // Queen target
    Vec2 queen_pos = {0, 0};
    bool queen_on_board = board->queen_on_board;
    if (queen_on_board) {
        queen_pos = board->pieces[QUEEN_ID].position;
    }
    
    // TACTIC_DIRECT: Aim at each own piece toward any valid pocket
    for (int i = 0; i < target_count && count < max_candidates; i++) {
        Vec2 target = targets[i];
        Vec2 to_target = vec2_sub(target, placement);
        
        // Check all pockets for valid alignment
        for (int p = 0; p < 4 && count < max_candidates; p++) {
            Vec2 to_pocket = vec2_sub(POCKET_CENTERS[p], target);
            
            // Check if alignment is reasonable (dot product > 0)
            if (vec2_dot(to_target, to_pocket) > 0) {
                float aim = atan2f(to_target.y, to_target.x);
                out_candidates[count].plan = (ShotPlan){
                    .placement = placement,
                    .aim_angle = aim,
                    .power = 0.5f,
                    .tactic = TACTIC_DIRECT,
                    .rng_draw = 0
                };
                count++;
            }
        }
    }
    
    // TACTIC_QUEEN: Aim at queen if on board
    if (queen_on_board && count < max_candidates) {
        Vec2 to_queen = vec2_sub(queen_pos, placement);
        // Check if queen can be pocketed
        for (int p = 0; p < 4; p++) {
            Vec2 to_pocket = vec2_sub(POCKET_CENTERS[p], queen_pos);
            if (vec2_dot(to_queen, to_pocket) > 0) {
                float aim = atan2f(to_queen.y, to_queen.x);
                out_candidates[count].plan = (ShotPlan){
                    .placement = placement,
                    .aim_angle = aim,
                    .power = 0.55f,
                    .tactic = TACTIC_QUEEN,
                    .rng_draw = 0
                };
                count++;
                break;
            }
        }
    }
    
    // TACTIC_COVER: If queen was pocketed, aim at own piece to cover
    if (board->queen_state == QUEEN_STATE_POCKETED_NO_COVER && count < max_candidates) {
        for (int i = 0; i < target_count && count < max_candidates; i++) {
            Vec2 to_target = vec2_sub(targets[i], placement);
            float aim = atan2f(to_target.y, to_target.x);
            out_candidates[count].plan = (ShotPlan){
                .placement = placement,
                .aim_angle = aim,
                .power = 0.5f,
                .tactic = TACTIC_COVER,
                .rng_draw = 0
            };
            count++;
        }
    }
    
    // TACTIC_BANK: Bank shots off cushions
    if (count < max_candidates) {
        // Bank shots: reflect target across cushions to aim at virtual image
        float cushion_y_top = 0.5f - CUSHION_THICKNESS;
        float cushion_y_bottom = -0.5f + CUSHION_THICKNESS;
        float cushion_x_left = -0.5f + CUSHION_THICKNESS;
        float cushion_x_right = 0.5f - CUSHION_THICKNESS;
        
        for (int i = 0; i < target_count && count < max_candidates; i += 2) {  // Subsample
            Vec2 target = targets[i];
            
            // Reflect target across each cushion
            // Top cushion
            Vec2 reflected = target;
            reflected.y = 2.0f * cushion_y_top - target.y;
            Vec2 to_reflected = vec2_sub(reflected, placement);
            float aim = atan2f(to_reflected.y, to_reflected.x);
            out_candidates[count].plan = (ShotPlan){
                .placement = placement,
                .aim_angle = aim,
                .power = 0.6f,
                .tactic = TACTIC_BANK,
                .rng_draw = 0
            };
            count++;
            if (count >= max_candidates) break;
            
            // Bottom cushion
            reflected = target;
            reflected.y = 2.0f * cushion_y_bottom - target.y;
            to_reflected = vec2_sub(reflected, placement);
            aim = atan2f(to_reflected.y, to_reflected.x);
            out_candidates[count].plan = (ShotPlan){
                .placement = placement,
                .aim_angle = aim,
                .power = 0.6f,
                .tactic = TACTIC_BANK,
                .rng_draw = 0
            };
            count++;
            if (count >= max_candidates) break;
            
            // Left cushion
            reflected = target;
            reflected.x = 2.0f * cushion_x_left - target.x;
            to_reflected = vec2_sub(reflected, placement);
            aim = atan2f(to_reflected.y, to_reflected.x);
            out_candidates[count].plan = (ShotPlan){
                .placement = placement,
                .aim_angle = aim,
                .power = 0.6f,
                .tactic = TACTIC_BANK,
                .rng_draw = 0
            };
            count++;
            if (count >= max_candidates) break;
            
            // Right cushion
            reflected = target;
            reflected.x = 2.0f * cushion_x_right - target.x;
            to_reflected = vec2_sub(reflected, placement);
            aim = atan2f(to_reflected.y, to_reflected.x);
            out_candidates[count].plan = (ShotPlan){
                .placement = placement,
                .aim_angle = aim,
                .power = 0.6f,
                .tactic = TACTIC_BANK,
                .rng_draw = 0
            };
            count++;
            if (count >= max_candidates) break;
        }
    }
    
    // TACTIC_CUT: Cut shots (thin hits)
    if (count < max_candidates) {
        for (int i = 0; i < target_count && count < max_candidates; i++) {
            Vec2 to_target = vec2_sub(targets[i], placement);
            float base_aim = atan2f(to_target.y, to_target.x);
            
            // Add slight offset for cut
            float offset = 0.2f;  // ~11 degrees
            out_candidates[count].plan = (ShotPlan){
                .placement = placement,
                .aim_angle = math_wrap_angle(base_aim + offset),
                .power = 0.5f,
                .tactic = TACTIC_CUT,
                .rng_draw = 0
            };
            count++;
            if (count >= max_candidates) break;
            
            out_candidates[count].plan = (ShotPlan){
                .placement = placement,
                .aim_angle = math_wrap_angle(base_aim - offset),
                .power = 0.5f,
                .tactic = TACTIC_CUT,
                .rng_draw = 0
            };
            count++;
        }
    }
    
    // TACTIC_DEFENSIVE: Safety shots - aim to leave difficult position for opponent
    if (count < max_candidates) {
        // Aim to push pieces toward cushions or cluster
        Vec2 defensive_target = {0, 0};
        if (target_count > 0) {
            defensive_target = targets[0];
        }
        Vec2 to_def = vec2_sub(defensive_target, placement);
        float aim = atan2f(to_def.y, to_def.x);
        out_candidates[count].plan = (ShotPlan){
            .placement = placement,
            .aim_angle = aim,
            .power = 0.35f,  // Lower power
            .tactic = TACTIC_DEFENSIVE,
            .rng_draw = 0
        };
        count++;
    }
    
    // TACTIC_BREAK: Break shot (first shot of board)
    if (snap->game->consecutive_turns == 0 && count < max_candidates) {
        // Aim at center of pack
        Vec2 to_center = vec2_sub((Vec2){0, 0}, placement);
        float aim = atan2f(to_center.y, to_center.x);
        out_candidates[count].plan = (ShotPlan){
            .placement = placement,
            .aim_angle = aim,
            .power = 0.8f,  // High power for break
            .tactic = TACTIC_BREAK,
            .rng_draw = 0
        };
        count++;
    }
    
    // TACTIC_FALLBACK: Always available
    if (count < max_candidates) {
        float aim = 0;
        switch (seat) {
            case SEAT_NORTH: aim = -M_PI / 2.0f; break;
            case SEAT_SOUTH: aim = M_PI / 2.0f; break;
            case SEAT_EAST:  aim = M_PI; break;
            case SEAT_WEST:  aim = 0; break;
        }
        out_candidates[count].plan = (ShotPlan){
            .placement = placement,
            .aim_angle = aim,
            .power = 0.4f,
            .tactic = TACTIC_FALLBACK,
            .rng_draw = 0
        };
        count++;
    }
    
    return count;
}

/* -----------------------------------------------------------------------------
 * Aim/Power Variants
 * --------------------------------------------------------------------------- */
int shot_candidates_variants(const DecisionSnapshot* snap, ShotCandidate* base, ShotCandidate* out_variants, int max_variants, PCG32* rng) {
    (void)snap;
    (void)rng;
    int count = 0;
    const ShotPlan* plan = &base->plan;
    
    // 5 power levels around base
    float powers[5] = {
        math_clamp(plan->power - 0.15f, 0.1f, 1.0f),
        math_clamp(plan->power - 0.075f, 0.1f, 1.0f),
        plan->power,
        math_clamp(plan->power + 0.075f, 0.1f, 1.0f),
        math_clamp(plan->power + 0.15f, 0.1f, 1.0f)
    };
    
    // 8 aim angles around base
    float aim_offsets[8] = { -0.15f, -0.1f, -0.05f, -0.02f, 0.02f, 0.05f, 0.1f, 0.15f };
    
    for (int p = 0; p < 5 && count < max_variants; p++) {
        for (int a = 0; a < 8 && count < max_variants; a++) {
            out_variants[count] = *base;
            out_variants[count].plan.power = powers[p];
            out_variants[count].plan.aim_angle = math_wrap_angle(plan->aim_angle + aim_offsets[a]);
            count++;
        }
    }
    
    return count;
}

/* -----------------------------------------------------------------------------
 * Full Candidate Generation Pipeline
 * --------------------------------------------------------------------------- */
int shot_candidates_generate(const DecisionSnapshot* snap, ShotCandidate* out_candidates, int max_candidates, PCG32* rng) {
    int total_count = 0;
    
    // Step 1: Legal placements
    Vec2 placements[8];
    int placement_count = shot_candidates_placements(snap->active_seat, placements, 8);
    
    // Step 2-4: For each placement, generate tactical candidates + variants
    for (int p = 0; p < placement_count && total_count < max_candidates; p++) {
        ShotCandidate tactical_candidates[40];
        int tactical_count = shot_candidates_tactical(snap, placements[p], tactical_candidates, 40, rng);
        
        // For each tactical candidate, generate variants
        for (int t = 0; t < tactical_count && total_count < max_candidates; t++) {
            ShotCandidate variants[40];
            int variant_count = shot_candidates_variants(snap, &tactical_candidates[t], variants, 
                                                          (max_candidates - total_count), rng);
            
            for (int v = 0; v < variant_count && total_count < max_candidates; v++) {
                out_candidates[total_count++] = variants[v];
            }
        }
    }
    
    return total_count;
}
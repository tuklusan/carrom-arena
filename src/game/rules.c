#include "rules.h"
#include "common/types.h"
#include "common/rng.h"
#include "common/math.h"
#include "board.h"
#include "scoring.h"
#include <string.h>
#include <stdlib.h>

/* -----------------------------------------------------------------------------
 * Pure Rules Engine Implementation
 * Article 16.1 - All rules bullets covered
 * --------------------------------------------------------------------------- */

void rules_outcome_init(RulesOutcome* outcome) {
    memset(outcome, 0, sizeof(RulesOutcome));
    outcome->turn_decision = TURN_ADVANCE;
    outcome->event_count = 0;
}

void game_state_init(GameState* game, uint64_t seed) {
    (void)seed;
    memset(game, 0, sizeof(GameState));
    game->phase = PHASE_IDLE;
    game->turn_seat = SEAT_NORTH;
    game->active_player.seat = SEAT_NORTH;
    game->active_player.team = TEAM_WHITE;
    game->active_player.strategy_index = 0;
    game->consecutive_turns = 0;
    game->scores.white = 0;
    game->scores.black = 0;
    board_state_init(&game->board);
}

void shot_plan_init(ShotPlan* plan) {
    memset(plan, 0, sizeof(ShotPlan));
    plan->placement = (Vec2){0, 0};
    plan->aim_angle = 0.0f;
    plan->power = 0.0f;
    plan->tactic = TACTIC_FALLBACK;
    plan->rng_draw = 0;
}

void shot_result_init(ShotResult* result) {
    memset(result, 0, sizeof(ShotResult));
    result->pocketed_count = 0;
    result->queen_pocketed = false;
    result->striker_pocketed = false;
    result->fouls = FOUL_NONE;
    result->sim_time = 0.0f;
    for (int i = 0; i < 19; i++) {
        result->final_positions[i] = (Vec2){0, 0};
    }
}

RulesOutcome rules_resolve(const MatchState* prior_match, const GameState* prior_game, const ShotFacts* facts) {
    RulesOutcome outcome;
    rules_outcome_init(&outcome);
    
    // Copy prior state
    outcome.next_game_state = *prior_game;
    outcome.next_match_state = *prior_match;
    
    GameState* game = &outcome.next_game_state;
    MatchState* match = &outcome.next_match_state;
    
    // Default turn decision
    outcome.turn_decision = TURN_ADVANCE;
    
    // Process pocketed pieces
    int white_pocketed = 0;
    int black_pocketed = 0;
    bool queen_pocketed = facts->queen_pocketed;
    
    for (int i = 0; i < facts->pocketed_count; i++) {
        if (facts->pocketed_colors[i] == PIECE_WHITE) white_pocketed++;
        else if (facts->pocketed_colors[i] == PIECE_BLACK) black_pocketed++;
    }
    
    // Determine active team
    Team active_team = (facts->active_seat == SEAT_NORTH || facts->active_seat == SEAT_SOUTH) ? TEAM_WHITE : TEAM_BLACK;
    
    // Handle fouls first (Article 16.1)
    if (facts->fouls != FOUL_NONE) {
        // Striker pocketed = foul, lose turn, due piece
        if (facts->fouls & FOUL_STRIKER_POCKETED) {
            outcome.turn_decision = TURN_ADVANCE;
            // Add due piece for active player's team
            if (active_team == TEAM_WHITE) {
                outcome.due_actions_white++;
                game->board.white_dues++;
            } else {
                outcome.due_actions_black++;
                game->board.black_dues++;
            }
            // Emit foul event
            if (outcome.event_count < 16) {
                outcome.events[outcome.event_count++] = (GameEvent){
                    .type = EVENT_FOUL,
                    .tick = game->consecutive_turns, // Use turn counter as tick
                    .seat = facts->active_seat,
                    .team = active_team,
                    .turn_decision = TURN_ADVANCE
                };
            }
        }
        
        // Wrong pocket (opponent piece in wrong pocket) - already captured in pocketed logic
        // but we emit foul event
        if (facts->fouls & FOUL_WRONG_POCKET) {
            if (outcome.event_count < 16) {
                outcome.events[outcome.event_count++] = (GameEvent){
                    .type = EVENT_FOUL,
                    .tick = game->consecutive_turns,
                    .seat = facts->active_seat,
                    .team = active_team,
                    .turn_decision = TURN_ADVANCE
                };
            }
        }
        
        // No contact foul
        if (facts->fouls & FOUL_NO_CONTACT) {
            outcome.turn_decision = TURN_ADVANCE;
            if (active_team == TEAM_WHITE) {
                outcome.due_actions_white++;
                game->board.white_dues++;
            } else {
                outcome.due_actions_black++;
                game->board.black_dues++;
            }
            if (outcome.event_count < 16) {
                outcome.events[outcome.event_count++] = (GameEvent){
                    .type = EVENT_FOUL,
                    .tick = game->consecutive_turns,
                    .seat = facts->active_seat,
                    .team = active_team,
                    .turn_decision = TURN_ADVANCE
                };
            }
        }
        
        // If any foul, turn ends (unless queen was pocketed and covered - handled below)
        // But continue processing pocketed pieces for score
    }
    
    // Score calculation
    int white_score_delta = 0;
    int black_score_delta = 0;
    
    // Active team scores for their own pieces pocketed
    if (active_team == TEAM_WHITE) {
        white_score_delta += white_pocketed;
    } else {
        black_score_delta += black_pocketed;
    }
    
    // Opponent's pieces pocketed don't score for active player
    // (they're just removed from board)
    
    // Queen handling (Article 16.1)
    if (queen_pocketed) {
        // Check if queen was covered (queen was previously pocketed without cover, 
        // and now active player pockets own piece)
        // OR queen pocketed this shot AND own piece pocketed same shot (cover)
        bool queen_covered = false;
        
        // Case 1: Queen was in POCKETED_NO_COVER state and this shot covers it
        if (facts->queen_state == QUEEN_STATE_POCKETED_NO_COVER) {
            // Active player must pocket their own piece to cover
            if ((active_team == TEAM_WHITE && white_pocketed > 0) ||
                (active_team == TEAM_BLACK && black_pocketed > 0)) {
                queen_covered = true;
            }
        }
        // Case 2: Queen pocketed this shot AND own piece pocketed same shot (instant cover)
        else {
            if ((active_team == TEAM_WHITE && white_pocketed > 0) ||
                (active_team == TEAM_BLACK && black_pocketed > 0)) {
                queen_covered = true;
            }
        }
        
        if (queen_covered) {
            // Queen covered - 3 points to active team
            if (active_team == TEAM_WHITE) {
                white_score_delta += 3;
            } else {
                black_score_delta += 3;
            }
            
            game->board.queen_state = QUEEN_STATE_COVERED;
            game->board.queen_on_board = false;
            game->board.queen_dues = 0;
            
            if (outcome.event_count < 16) {
                outcome.events[outcome.event_count++] = (GameEvent){
                    .type = EVENT_QUEEN_COVERED,
                    .tick = game->consecutive_turns,
                    .seat = facts->active_seat,
                    .team = active_team,
                    .score_delta_white = (active_team == TEAM_WHITE) ? 3 : 0,
                    .score_delta_black = (active_team == TEAM_BLACK) ? 3 : 0,
                    .turn_decision = outcome.turn_decision
                };
            }
        } else {
            // Queen pocketed but not covered - goes to due, returns to center
            game->board.queen_state = QUEEN_STATE_POCKETED_NO_COVER;
            game->board.queen_on_board = false;
            game->board.queen_dues = 1;
            
            if (outcome.event_count < 16) {
                outcome.events[outcome.event_count++] = (GameEvent){
                    .type = EVENT_QUEEN_POCKETED,
                    .tick = game->consecutive_turns,
                    .seat = facts->active_seat,
                    .team = active_team,
                    .turn_decision = outcome.turn_decision
                };
            }
        }
    }
    
    // Apply score deltas
    outcome.score_delta.white = white_score_delta;
    outcome.score_delta.black = black_score_delta;
    game->scores.white += white_score_delta;
    game->scores.black += black_score_delta;
    
    // Pocket events for regular pieces
    for (int i = 0; i < facts->pocketed_count; i++) {
        if (outcome.event_count < 16) {
            outcome.events[outcome.event_count++] = (GameEvent){
                .type = EVENT_POCKET,
                .tick = game->consecutive_turns,
                .seat = facts->active_seat,
                .team = (facts->pocketed_colors[i] == PIECE_WHITE) ? TEAM_WHITE : TEAM_BLACK,
                .piece_id = facts->pocketed_ids[i],
                .piece_color = facts->pocketed_colors[i],
                .score_delta_white = (facts->pocketed_colors[i] == PIECE_WHITE && active_team == TEAM_WHITE) ? 1 : 0,
                .score_delta_black = (facts->pocketed_colors[i] == PIECE_BLACK && active_team == TEAM_BLACK) ? 1 : 0,
                .turn_decision = outcome.turn_decision
            };
        }
    }
    
    // Update board piece counts
    uint8_t prior_white_on_board = game->board.white_on_board;
    uint8_t prior_black_on_board = game->board.black_on_board;
    
    game->board.white_on_board = (white_pocketed <= prior_white_on_board) ? 
        (uint8_t)(prior_white_on_board - white_pocketed) : 0;
    game->board.black_on_board = (black_pocketed <= prior_black_on_board) ? 
        (uint8_t)(prior_black_on_board - black_pocketed) : 0;
    
    // If queen pocketed and covered, it's no longer on board
    if (queen_pocketed && game->board.queen_state == QUEEN_STATE_COVERED) {
        game->board.queen_on_board = false;
    }
    
    // Turn continuation logic (Article 16.1)
    // Player continues if:
    // 1. They pocketed at least one own piece AND no foul
    // 2. They pocketed queen AND covered it (and no foul)
    // 3. They pocketed queen previously (POCKETED_NO_COVER) and now cover it (and no foul)
    
    bool pocketed_own = (active_team == TEAM_WHITE && white_pocketed > 0) || 
                        (active_team == TEAM_BLACK && black_pocketed > 0);
    
    bool queen_covered_this_turn = false;
    if (queen_pocketed) {
        if ((active_team == TEAM_WHITE && white_pocketed > 0) ||
            (active_team == TEAM_BLACK && black_pocketed > 0)) {
            queen_covered_this_turn = true;
        }
    } else if (facts->queen_state == QUEEN_STATE_POCKETED_NO_COVER && pocketed_own) {
        queen_covered_this_turn = true;
    }
    
    bool has_foul = (facts->fouls != FOUL_NONE);
    
    if ((pocketed_own || queen_covered_this_turn) && !has_foul) {
        outcome.turn_decision = TURN_CONTINUE;
        game->consecutive_turns++;
    } else {
        outcome.turn_decision = TURN_ADVANCE;
        game->consecutive_turns = 0;
        
        // Advance turn to next seat
        game->turn_seat = (game->turn_seat + 1) % 4;
        game->active_player.seat = game->turn_seat;
        game->active_player.team = (game->turn_seat == SEAT_NORTH || game->turn_seat == SEAT_SOUTH) ? TEAM_WHITE : TEAM_BLACK;
        
        if (outcome.event_count < 16) {
            outcome.events[outcome.event_count++] = (GameEvent){
                .type = EVENT_TURN_CHANGE,
                .tick = game->consecutive_turns,
                .seat = game->turn_seat,
                .team = game->active_player.team,
                .turn_decision = TURN_ADVANCE
            };
        }
    }
    
    // Check board over (all pieces of one color cleared + queen covered)
    // Article 16.1: "Board over when one color cleared + queen covered"
    // Only trigger if exactly one color is cleared (XOR) and queen is resolved
    bool white_cleared_now = (game->board.white_on_board == 0);
    bool black_cleared_now = (game->board.black_on_board == 0);
    bool exactly_one_cleared = (white_cleared_now != black_cleared_now);  // XOR
    bool queen_resolved = (game->board.queen_state == QUEEN_STATE_COVERED || game->board.queen_state == QUEEN_STATE_ON_BOARD);
    // Queen must be covered if it was pocketed, or still on board
    
    bool white_cleared_this_shot = (game->board.white_on_board == 0 && prior_white_on_board > 0);
    bool black_cleared_this_shot = (game->board.black_on_board == 0 && prior_black_on_board > 0);
    bool cleared_this_shot = (white_cleared_this_shot || black_cleared_this_shot);
    bool valid_over_state = (exactly_one_cleared && queen_resolved);
    
    if ((cleared_this_shot || valid_over_state) && queen_resolved) {
        outcome.turn_decision = TURN_BOARD_OVER;
        game->phase = PHASE_BOARD_OVER;
        
        // Board winner gets points for opponent's remaining pieces (ICF rules)
        if (white_cleared_now) {
            // White cleared their pieces - White wins board
            // White gets points equal to black pieces remaining + queen bonus if covered
            int bonus = game->board.black_on_board;
            if (game->board.queen_state == QUEEN_STATE_COVERED) {
                bonus += 3; // Queen points
            }
            game->scores.white += bonus;
            outcome.score_delta.white += bonus;
            match->boards_won_white++;
        } else {
            // Black cleared their pieces - Black wins board
            int bonus = game->board.white_on_board;
            if (game->board.queen_state == QUEEN_STATE_COVERED) {
                bonus += 3; // Queen points
            }
            game->scores.black += bonus;
            outcome.score_delta.black += bonus;
            match->boards_won_black++;
        }
        
        if (outcome.event_count < 16) {
            outcome.events[outcome.event_count++] = (GameEvent){
                .type = EVENT_BOARD_END,
                .tick = game->consecutive_turns,
                .team = white_cleared_now ? TEAM_WHITE : TEAM_BLACK,
                .score_delta_white = white_cleared_now ? outcome.score_delta.white : 0,
                .score_delta_black = black_cleared_now ? outcome.score_delta.black : 0,
                .turn_decision = TURN_BOARD_OVER
            };
        }
    }
    
    // Check game over (target boards reached, typically 8)
    if (match->boards_won_white >= match->target_boards_per_game || 
        match->boards_won_black >= match->target_boards_per_game) {
        outcome.turn_decision = TURN_GAME_OVER;
        game->phase = PHASE_GAME_OVER;
        
        if (match->boards_won_white >= match->target_boards_per_game) {
            match->games_won_white++;
        } else {
            match->games_won_black++;
        }
        
        if (outcome.event_count < 16) {
            outcome.events[outcome.event_count++] = (GameEvent){
                .type = EVENT_GAME_END,
                .tick = game->consecutive_turns,
                .team = (match->boards_won_white >= match->target_boards_per_game) ? TEAM_WHITE : TEAM_BLACK,
                .turn_decision = TURN_GAME_OVER
            };
        }
    }
    
    // Check match over (target games reached, typically 3)
    if (match->games_won_white >= match->target_games_per_match || 
        match->games_won_black >= match->target_games_per_match) {
        outcome.turn_decision = TURN_MATCH_OVER;
        game->phase = PHASE_MATCH_OVER;
        
        if (outcome.event_count < 16) {
            outcome.events[outcome.event_count++] = (GameEvent){
                .type = EVENT_MATCH_END,
                .tick = game->consecutive_turns,
                .team = (match->games_won_white >= match->target_games_per_match) ? TEAM_WHITE : TEAM_BLACK,
                .turn_decision = TURN_MATCH_OVER
            };
        }
    }
    
    return outcome;
}

void match_extract_facts(const GameState* game, const ShotResult* result, ShotFacts* facts) {
    memset(facts, 0, sizeof(ShotFacts));
    
    facts->active_seat = game->turn_seat;
    facts->queen_state = game->board.queen_state;
    facts->white_dues = game->board.white_dues;
    facts->black_dues = game->board.black_dues;
    facts->queen_dues = game->board.queen_dues;
    
    facts->pocketed_count = result->pocketed_count;
    facts->queen_pocketed = result->queen_pocketed;
    facts->striker_pocketed = result->striker_pocketed;
    facts->fouls = result->fouls;
    
    for (int i = 0; i < result->pocketed_count; i++) {
        facts->pocketed_ids[i] = result->pocketed_ids[i];
        facts->pocketed_colors[i] = result->pocketed_colors[i];
    }
}

bool match_validate_shot(const GameState* game, const ShotPlan* plan) {
    // Validate placement is on correct baseline for active seat
    Seat seat = game->turn_seat;
    Vec2 p = plan->placement;
    
    // Check baseline bounds
    switch (seat) {
        case SEAT_NORTH:
            if (math_fabsf(p.y - BASELINE_Y_NORTH) > 0.01f) return false;
            if (p.x < -BASELINE_MAX_OFFSET || p.x > BASELINE_MAX_OFFSET) return false;
            break;
        case SEAT_SOUTH:
            if (math_fabsf(p.y - BASELINE_Y_SOUTH) > 0.01f) return false;
            if (p.x < -BASELINE_MAX_OFFSET || p.x > BASELINE_MAX_OFFSET) return false;
            break;
        case SEAT_EAST:
            if (math_fabsf(p.x - BASELINE_X_EAST) > 0.01f) return false;
            if (p.y < -BASELINE_MAX_OFFSET || p.y > BASELINE_MAX_OFFSET) return false;
            break;
        case SEAT_WEST:
            if (math_fabsf(p.x - BASELINE_X_WEST) > 0.01f) return false;
            if (p.y < -BASELINE_MAX_OFFSET || p.y > BASELINE_MAX_OFFSET) return false;
            break;
    }
    
    // Validate power range
    if (plan->power < 0.0f || plan->power > 1.0f) return false;
    
    // Validate aim angle
    if (plan->aim_angle < -M_PI || plan->aim_angle > M_PI) return false;
    
    return true;
}

bool game_is_over(const GameState* game) {
    return game->phase == PHASE_GAME_OVER || game->phase == PHASE_MATCH_OVER;
}

bool board_is_over(const GameState* game) {
    return game->phase == PHASE_BOARD_OVER;
}
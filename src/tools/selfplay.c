/* selfplay: headless AI-vs-AI board, no timers or window, to find stalls.
 *   selfplay [--seed N] [--seeds K] [--max-turns T] [--stale] [--verbose] [--expect-finish] [--boards B]
  * Plays B boards (default 1) per seed with the arena AI and reports turns used, coins pocketed and whether the board finished.
 * A "stall" is 6 consecutive turns in which no coin moved. --stale reproduces the old bug in which the AI was shown
 * the initial rack positions forever instead of where the coins really are. */
#include "common/types.h"
#include "common/rng.h"
#include "common/strategy_profiles.h"
#include "game/match.h"
#include "game/rules.h"
#include "game/board.h"
#include "physics/physics.h"
#include "physics/physics_snapshot.h"
#include "ai/controller.h"
#include "ai/shot_candidates.h"
#include "ai/shot_evaluator.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct { int turns, pocketed, stalls, finished, max_still; } Report;

static Report play_board(uint64_t seed, int max_turns, bool stale, bool verbose, int boards) {
    Report rep = {0, 0, 0, 0, 0};
    RNGContext rng;
    rng_context_init(&rng, seed);
    MatchState match;
    match_state_init(&match);
    match.target_boards_per_game = 8;   /* boards are counted by this tool; the game never ends first */
    match.target_games_per_match = 3;
    int boards_done = 0;
    GameState game;
    game_state_init(&game, seed);
    PhysicsWorld* pw = physics_create();
    Controller* ctl[4];
    for (int i = 0; i < 4; i++)
        ctl[i] = arena_controller_create((Seat)i, strategy_by_index(seat_to_strategy((Seat)i)), &rng.streams[i]);
    match_start_board(&match, &game, &rng);
    physics_sync_from_board(pw, &game.board, game.turn_seat);

    int still = 0;
    while (rep.turns < max_turns) {
        PhysicsSnapshot* snap = physics_snapshot(pw);
        DecisionSnapshot ds = { .match = &match, .game = &game, .board = &game.board, .physics = snap,
                                .active_seat = game.turn_seat, .ai_budget_ms = 250, .max_candidates = 320 };
        ShotPlan plan = controller_decide(ctl[game.turn_seat], &ds, &rng.streams[game.turn_seat]);
        if (!match_validate_shot(&game, &plan)) plan = controller_fallback_shot(ctl[game.turn_seat], &ds, &rng.streams[game.turn_seat]);
        physics_snapshot_destroy(snap);

        Vec2 before[MAX_PIECES];
        physics_get_positions(pw, before);
        physics_place_striker(pw, game.turn_seat, plan.placement);
        physics_apply_shot(pw, plan.aim_angle, plan.power);
        for (int s = 0; s < 120 * 30; s++) {
            physics_step(pw, PHYSICS_DT);
            if (physics_is_settled(pw)) break;
        }
        ShotResult res;
        shot_result_init(&res);
        physics_collect_pocketed(pw, &res);
        physics_get_final_positions(pw, res.final_positions);
        res.sim_time = physics_get_sim_time(pw);

        float moved = 0.0f;
        for (int i = 0; i < MAX_PIECES; i++) {
            if (!game.board.pieces[i].on_board) continue;
            moved += fabsf(res.final_positions[i].x - before[i].x) + fabsf(res.final_positions[i].y - before[i].y);
        }
        still = (moved < 1e-3f) ? still + 1 : 0;
        if (still > rep.max_still) rep.max_still = still;
        if (still == 6) {
            rep.stalls++;
            printf("  STALL at turn %d: coins still on the board (id colour x y):\n", rep.turns + 1);
            for (int i = 0; i < MAX_PIECES; i++) {
                if (!game.board.pieces[i].on_board) continue;
                printf("    %2d %s (%.4f, %.4f)  physics_pocketed=%d\n", i, i < 9 ? "white" : (i < 18 ? "black" : "queen"),
                       res.final_positions[i].x, res.final_positions[i].y, physics_is_piece_pocketed(pw, i) ? 1 : 0);
            }
            printf("    last plan: seat=%d placement=(%.3f,%.3f) aim=%.3f power=%.2f tactic=%d\n", (int)game.turn_seat,
                   plan.placement.x, plan.placement.y, plan.aim_angle, plan.power, (int)plan.tactic);
            {   /* what did the AI see? evaluate every candidate without a time budget and list the best */
                PhysicsSnapshot* sn = physics_snapshot(pw);
                DecisionSnapshot d2 = { .match = &match, .game = &game, .board = &game.board, .physics = sn,
                                        .active_seat = game.turn_seat, .ai_budget_ms = 250, .max_candidates = 320 };
                ShotCandidate* cs = calloc(320, sizeof(ShotCandidate));
                int nc = shot_candidates_generate(&d2, cs, 320, &rng.streams[game.turn_seat]);
                for (int c = 0; c < nc; c++) shot_evaluator_evaluate(&cs[c], &d2, &rng.streams[game.turn_seat]);
                shot_evaluator_score_candidates(cs, nc, &d2, &ctl[game.turn_seat]->profile);
                printf("    %d candidates. best 8 by score:\n", nc);
                for (int k = 0; k < 8 && k < nc; k++) {
                    int b = 0; float bs2 = -1e30f;
                    for (int c = 0; c < nc; c++) if (cs[c].score > bs2) { bs2 = cs[c].score; b = c; }
                    printf("      score=%.3f tactic=%d geom=%.1f place=(%.2f,%.2f) aim=%.2f power=%.2f pocketed=%d striker_pocketed=%d\n", cs[b].score,
                           (int)cs[b].tactic, cs[b].geom_score, cs[b].plan.placement.x, cs[b].plan.placement.y, cs[b].plan.aim_angle, cs[b].plan.power,
                           cs[b].sim_result.pocketed_count, cs[b].sim_result.striker_pocketed ? 1 : 0);
                    cs[b].score = -1e30f;
                }
                free(cs);
                physics_snapshot_destroy(sn);
            }
        }

        ShotFacts facts;
        match_extract_facts(&game, &res, &facts);
        RulesOutcome out = rules_resolve(&match, &game, &facts);
        game = out.next_game_state;
        match = out.next_match_state;
        rep.pocketed += res.pocketed_count;
        physics_consume_pocketed(pw);
        physics_reset_turn_timer(pw);   /* as the app does; without it the 30 s settle timeout ends every later shot instantly */
        striker_state_init(&game.board.striker, game.turn_seat);
        board_place_striker_on_baseline(&game.board.striker, game.turn_seat);
        if (!stale) board_apply_final_positions(&game.board, res.final_positions);
        if (game.board.pieces[QUEEN_ID].on_board && physics_is_piece_pocketed(pw, QUEEN_ID)) physics_sync_from_board(pw, &game.board, game.turn_seat);
        rep.turns++;
        if (verbose) {
            printf("  turn %3d seat=%d tactic=%d power=%.2f aim=%.2f pocketed=%d moved=%.3f left W=%d B=%d\n", rep.turns,
                   (int)game.turn_seat, (int)plan.tactic, plan.power, plan.aim_angle, res.pocketed_count, moved,
                   game.board.white_on_board, game.board.black_on_board);
        }
        if (out.turn_decision == TURN_BOARD_OVER || out.turn_decision == TURN_GAME_OVER || out.turn_decision == TURN_MATCH_OVER) {
            boards_done++;
            if (boards_done >= boards || out.turn_decision != TURN_BOARD_OVER) { rep.finished = 1; break; }
            /* next board, exactly as the app does it: fresh rack, coins back in the physics world, striker and timer reset */
            match_start_board(&match, &game, &rng);
            physics_sync_from_board(pw, &game.board, game.turn_seat);
            physics_reset_turn_timer(pw);
            int alive = 0;
            for (int i = 0; i < MAX_PIECES; i++) { Vec2 v; if (physics_get_piece_velocity(pw, i, &v)) alive++; }
            if (alive != MAX_PIECES) {
                printf("  BROKEN NEW BOARD %d: only %d of %d coins exist in the physics world%s\n", boards_done + 1, alive, MAX_PIECES, "");
                rep.stalls += 100;
                break;
            }
            still = 0;
        }
    }
    for (int i = 0; i < 4; i++) controller_destroy(ctl[i]);
    physics_destroy(pw);
    return rep;
}

int main(int argc, char** argv) {
    setvbuf(stdout, NULL, _IOLBF, 0);
    uint64_t seed = 1;
    int seeds = 1, max_turns = 300;
    bool stale = false, verbose = false, expect_finish = false;
    int boards = 1;
    int i = 1;
    while (i < argc) {
        const char* arg_cur = argv[i++];
        if (!strcmp(arg_cur, "--seed") && i < argc) seed = strtoull(argv[i++], NULL, 10);
        else if (!strcmp(arg_cur, "--seeds") && i < argc) seeds = atoi(argv[i++]);
        else if (!strcmp(arg_cur, "--max-turns") && i < argc) max_turns = atoi(argv[i++]);
        else if (!strcmp(arg_cur, "--stale")) stale = true;
        else if (!strcmp(arg_cur, "--verbose")) verbose = true;
        else if (!strcmp(arg_cur, "--boards") && i < argc) boards = atoi(argv[i++]);
        else if (!strcmp(arg_cur, "--expect-finish")) expect_finish = true;   /* exit 1 if any board fails to finish (CI) */
    }
    int finished = 0, total_turns = 0, total_stalls = 0;
    for (int k = 0; k < seeds; k++) {
        Report r = play_board(seed + (uint64_t)k, max_turns, stale, verbose, boards);
        printf("seed %llu: %s after %d turns, %d coins pocketed, longest motionless run %d turns%s\n",
               (unsigned long long)(seed + (uint64_t)k), r.finished ? "FINISHED" : "NOT FINISHED", r.turns, r.pocketed, r.max_still,
               r.stalls ? "  <-- STALL" : "");
        finished += r.finished;
        total_turns += r.turns;
        total_stalls += r.stalls;
    }
    printf("%s mode: %d/%d boards finished, %d turns total, %d stalls\n", stale ? "STALE-POSITIONS" : "FIXED", finished, seeds, total_turns, total_stalls);
    return (expect_finish && finished != seeds) ? 1 : 0;
}

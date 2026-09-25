#include "unity.h"
#include "common/types.h"
#include "common/vecmath.h"
#include "game/board.h"
#include "physics/physics.h"
#include "ai/geometry_planner.h"
#include <math.h>
#include <stdio.h>

void setUp(void) {}
void tearDown(void) {}

static void empty_board_with_white(BoardState* b, Vec2 pos) {
    board_state_init(b);
    for (int i = 0; i < MAX_PIECES; i++) { b->pieces[i].on_board = false; b->pieces[i].pocketed = false; }
    b->pieces[0].position = pos;
    b->pieces[0].on_board = true;
    b->pieces[0].color = PIECE_WHITE;
    b->white_on_board = 1;
    b->queen_on_board = false;
}

void test_ghost_ball_is_behind_target_on_pocket_line(void) {
    Vec2 t = {0.1f, -0.05f};
    Vec2 g = geometry_ghost_ball(t, POCKET_CENTERS[1]);
    float dist = hypotf(g.x - t.x, g.y - t.y);
    TEST_ASSERT_FLOAT_WITHIN(1e-4f, PIECE_RADIUS_NORM + STRIKER_RADIUS_NORM + 0.001f, dist);
    /* target lies between ghost and pocket */
    Vec2 to_p = {POCKET_CENTERS[1].x - t.x, POCKET_CENTERS[1].y - t.y};
    Vec2 to_g = {g.x - t.x, g.y - t.y};
    TEST_ASSERT_TRUE(to_p.x * to_g.x + to_p.y * to_g.y < 0.0f);
}

void test_segment_clear_detects_blockers(void) {
    Vec2 obs[1] = {{0.0f, 0.0f}};
    int ids[1] = {3};
    TEST_ASSERT_FALSE(geometry_segment_clear((Vec2){-0.3f, 0.005f}, (Vec2){0.3f, 0.005f}, 0.04f, obs, ids, 1, -1));
    TEST_ASSERT_TRUE(geometry_segment_clear((Vec2){-0.3f, 0.2f}, (Vec2){0.3f, 0.2f}, 0.04f, obs, ids, 1, -1));
    TEST_ASSERT_TRUE(geometry_segment_clear((Vec2){-0.3f, 0.005f}, (Vec2){0.3f, 0.005f}, 0.04f, obs, ids, 1, 3));
}

void test_power_grows_with_distance(void) {
    float near = geometry_power_for(0.2f, 0.2f, 1.0f, 1.0f, 1.0f);
    float far = geometry_power_for(0.6f, 0.6f, 1.0f, 1.0f, 1.0f);
    float cut = geometry_power_for(0.2f, 0.2f, 0.5f, 1.0f, 1.0f);
    TEST_ASSERT_TRUE(far > near);
    TEST_ASSERT_TRUE(cut > near);
    TEST_ASSERT_TRUE(near > 0.0f && far < 2.0f);
}

void test_blocked_pocket_line_is_not_planned(void) {
    BoardState b;
    empty_board_with_white(&b, (Vec2){0.2f, 0.2f});
    /* black blocker sitting right on the line from the white piece to the NE pocket */
    b.pieces[9].on_board = true; b.pieces[9].color = PIECE_BLACK;
    b.pieces[9].position = (Vec2){0.335f, 0.335f};
    GeomShot shots[512];
    int n = geometry_plan_shots(&b, SEAT_SOUTH, shots, 512);
    TEST_ASSERT_TRUE(n > 0);
    for (int i = 0; i < n; i++) {
        if ((shots[i].tactic == TACTIC_DIRECT || shots[i].tactic == TACTIC_CUT) && shots[i].target_id == 0)
            TEST_ASSERT_NOT_EQUAL(1, shots[i].pocket_index); /* pocket 1 = NE */
    }
}

static bool simulate_pockets_white(const BoardState* b, const GeomShot* s, Seat seat) {
    PhysicsWorld* pw = physics_create();
    physics_sync_from_board(pw, b, seat);
    physics_place_striker(pw, seat, s->placement);
    physics_apply_shot(pw, s->aim_angle, s->power);
    for (int i = 0; i < 120 * 8; i++) {
        physics_step(pw, PHYSICS_DT);
        if (physics_is_settled(pw)) break;
    }
    bool ok = physics_is_piece_pocketed(pw, 0);
    physics_destroy(pw);
    return ok;
}

/* The planned direct/cut shots must actually pocket the piece in the real physics */
void test_planned_shots_pocket_in_simulation(void) {
    const Vec2 spots[] = {{0.15f, -0.10f}, {-0.20f, 0.05f}, {0.05f, 0.25f}, {-0.10f, -0.20f}};
    int tried = 0, pocketed = 0;
    for (int k = 0; k < 4; k++) {
        BoardState b;
        empty_board_with_white(&b, spots[k]);
        GeomShot shots[512];
        int n = geometry_plan_shots(&b, SEAT_SOUTH, shots, 512);
        int used = 0;
        for (int i = 0; i < n && used < 6; i++) {
            if (shots[i].tactic != TACTIC_DIRECT && shots[i].tactic != TACTIC_CUT) continue;
            used++; tried++;
            if (simulate_pockets_white(&b, &shots[i], SEAT_SOUTH)) pocketed++;
        }
    }
    printf("PLANNER: %d of %d direct/cut shots pocketed in simulation\n", pocketed, tried);
    TEST_ASSERT_TRUE(tried >= 8);
    TEST_ASSERT_TRUE(pocketed * 10 >= tried * 7); /* at least 70% */
}

/* Every legal baseline placement must survive being placed: the pocket sensor must not capture the striker */
void test_legal_placements_are_never_captured_by_a_pocket(void) {
    const Seat seats[4] = { SEAT_NORTH, SEAT_EAST, SEAT_SOUTH, SEAT_WEST };
    int legal = 0, illegal = 0;
    for (int s = 0; s < 4; s++) {
        for (int k = 0; k <= 200; k++) {
            float off = -BASELINE_MAX_OFFSET + (float)k / 200.0f * 2.0f * BASELINE_MAX_OFFSET;
            Vec2 pos;
            switch (seats[s]) {
                case SEAT_NORTH: pos = (Vec2){ off, BASELINE_Y_NORTH }; break;
                case SEAT_SOUTH: pos = (Vec2){ off, BASELINE_Y_SOUTH }; break;
                case SEAT_EAST:  pos = (Vec2){ BASELINE_X_EAST, off }; break;
                default:         pos = (Vec2){ BASELINE_X_WEST, off }; break;
            }
            PhysicsWorld* pw = physics_create();
            BoardState b;
            board_state_init(&b);
            for (int i = 0; i < MAX_PIECES; i++) b.pieces[i].on_board = false;
            physics_sync_from_board(pw, &b, seats[s]);
            physics_place_striker(pw, seats[s], pos);
            physics_step(pw, PHYSICS_DT);
            physics_step(pw, PHYSICS_DT);
            bool captured = physics_is_striker_pocketed(pw);
            physics_destroy(pw);
            if (board_is_legal_placement(seats[s], pos)) {
                legal++;
                TEST_ASSERT_FALSE_MESSAGE(captured, "a legal placement is inside a pocket capture zone");
            } else {
                illegal++;
            }
        }
    }
    TEST_ASSERT_TRUE(legal > 400);      /* most of the baseline is still usable */
    TEST_ASSERT_TRUE(illegal > 0);      /* the pocket ends are excluded */
}

/* Regression for the endgame loop: after a shot settles the game state must show where the coins really are, otherwise the
 * AI keeps planning against the initial rack and every player repeats the same shot. */
void test_board_positions_follow_the_settled_coins(void) {
    BoardState b;
    empty_board_with_white(&b, (Vec2){0.0f, 0.0f});
    PhysicsWorld* pw = physics_create();
    physics_sync_from_board(pw, &b, SEAT_SOUTH);
    physics_place_striker(pw, SEAT_SOUTH, (Vec2){-0.2f, 0.0f});
    physics_apply_shot(pw, 0.0f, 0.3f);
    for (int i = 0; i < 120 * 6; i++) {
        physics_step(pw, PHYSICS_DT);
        if (physics_is_settled(pw)) break;
    }
    Vec2 fin[MAX_PIECES];
    physics_get_final_positions(pw, fin);
    physics_destroy(pw);
    TEST_ASSERT_TRUE(fabsf(fin[0].x) > 0.02f);            /* the coin was really moved by the striker */
    board_apply_final_positions(&b, fin);
    TEST_ASSERT_FLOAT_WITHIN(1e-6f, fin[0].x, b.pieces[0].position.x);
    TEST_ASSERT_FLOAT_WITHIN(1e-6f, fin[0].y, b.pieces[0].position.y);
    /* and the planner now aims at the coin where it is */
    GeomShot shots[512];
    int n = geometry_plan_shots(&b, SEAT_SOUTH, shots, 512);
    TEST_ASSERT_TRUE(n > 0);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_ghost_ball_is_behind_target_on_pocket_line);
    RUN_TEST(test_segment_clear_detects_blockers);
    RUN_TEST(test_power_grows_with_distance);
    RUN_TEST(test_blocked_pocket_line_is_not_planned);
    RUN_TEST(test_planned_shots_pocket_in_simulation);
    RUN_TEST(test_legal_placements_are_never_captured_by_a_pocket);
    RUN_TEST(test_board_positions_follow_the_settled_coins);
    return UNITY_END();
}

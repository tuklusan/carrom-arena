#include "unity.h"
#include "common/types.h"
#include "game/board.h"
#include "physics/physics.h"
#include <math.h>
#include <stdio.h>

void setUp(void) {}
void tearDown(void) {}

/* Full-power striker across an empty board: how far, how long, how many cushion hits ("runs" in ICF terms).
 * Units: 1.0 = 74 cm board side, so 1 u/s = 0.74 m/s. */
void test_full_power_striker_realism(void) {
    PhysicsWorld* pw = physics_create();
    BoardState board;
    board_state_init(&board);
    physics_sync_from_board(pw, &board, SEAT_SOUTH);
    physics_place_striker(pw, SEAT_SOUTH, (Vec2){0.0f, BASELINE_Y_SOUTH});
    physics_apply_shot(pw, (float)M_PI / 2.0f, 1.0f);

    Vec2 prev, cur, vel, prev_vel = {0, 0};
    physics_get_striker_position(pw, &prev);
    float path = 0.0f, t_first_cross = -1.0f, t_rest = -1.0f;
    int hits = 0;
    for (int i = 0; i < 120 * 30; i++) {
        physics_step(pw, PHYSICS_DT);
        physics_get_striker_position(pw, &cur);
        physics_get_striker_velocity(pw, &vel);
        path += hypotf(cur.x - prev.x, cur.y - prev.y);
        if (t_first_cross < 0 && cur.y > 0.5f - CUSHION_THICKNESS - STRIKER_RADIUS_NORM - 0.005f)
            t_first_cross = (float)(i + 1) * PHYSICS_DT;
        if (prev_vel.y * vel.y < 0 && fabsf(prev_vel.y) > 0.05f) hits++;
        prev = cur; prev_vel = vel;
        if (vel.x == 0 && vel.y == 0) { t_rest = (float)(i + 1) * PHYSICS_DT; break; }
    }
    physics_destroy(pw);
    printf("REALISM: first crossing %.3f s (%.2f m/s launch), rest after %.2f s, path %.2f board widths = %.2f m, cushion hits %d\n",
           t_first_cross, 5.0f * 0.74f, t_rest, path, path * 0.74f, hits);
    /* A hard flick crosses a 74 cm board in roughly 0.15-0.4 s and keeps running for several seconds */
    TEST_ASSERT_TRUE(t_first_cross > 0.1f && t_first_cross < 0.4f);
    TEST_ASSERT_TRUE(t_rest > 1.5f && t_rest < 8.0f);
    /* ICF smoothness: at least 3.5 runs = 3 rebounds off the far/near cushions */
    TEST_ASSERT_TRUE(hits >= 3);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_full_power_striker_realism);
    return UNITY_END();
}

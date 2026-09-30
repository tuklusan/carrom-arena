/*
 * Copyright (c) 2026 Supratim Sanyal of SANYALnet Labs.
 * Licensed under the SANYALnet Labs Non-Commercial License; see LICENSE.
 */

#include <math.h>
#include <stdbool.h>
#include <stdlib.h>
#include "unity.h"
#include "common/types.h"
#include "game/striker_path.h"

void setUp(void) {}
void tearDown(void) {}

static uint32_t rng_state = 12345u;
static float rnd(void) {
    rng_state = rng_state * 1664525u + 1013904223u;
    return (float)(rng_state >> 8) / 16777216.0f;
}

/* every segment of the path keeps STRIKER_PATH_CLEAR from every coin, and the path runs from `from` to `to` */
static void check_path(const Vec2* path, int n, Vec2 from, Vec2 to, const Vec2* coins, int nc) {
    TEST_ASSERT_TRUE(n >= 2);
    TEST_ASSERT_FLOAT_WITHIN(1e-6f, from.x, path[0].x);
    TEST_ASSERT_FLOAT_WITHIN(1e-6f, from.y, path[0].y);
    TEST_ASSERT_FLOAT_WITHIN(1e-6f, to.x, path[n - 1].x);
    TEST_ASSERT_FLOAT_WITHIN(1e-6f, to.y, path[n - 1].y);
    for (int i = 1; i < n; i++) {
        Vec2 a = path[i - 1], b = path[i];
        float dx = b.x - a.x, dy = b.y - a.y, len2 = dx * dx + dy * dy;
        for (int c = 0; c < nc; c++) {
            float t = len2 > 1e-12f ? ((coins[c].x - a.x) * dx + (coins[c].y - a.y) * dy) / len2 : 0.0f;
            if (t < 0.0f) t = 0.0f;
            if (t > 1.0f) t = 1.0f;
            float qx = a.x + dx * t - coins[c].x, qy = a.y + dy * t - coins[c].y;
            TEST_ASSERT_TRUE_MESSAGE(sqrtf(qx * qx + qy * qy) >= STRIKER_PATH_CLEAR - 1e-3f, "the striker's path touches a coin");
        }
    }
}

void test_empty_board_is_a_straight_line(void) {
    Vec2 path[32];
    Vec2 from = POCKET_CENTERS[0], to = { 0.0f, BASELINE_Y_SOUTH };
    int n = striker_path_plan(from, to, NULL, 0, path, 32);
    check_path(path, n, from, to, NULL, 0);
    TEST_ASSERT_TRUE(n <= 3);
}

void test_wall_of_coins_is_gone_around(void) {
    Vec2 coins[13];
    int nc = 0;
    for (int i = -6; i <= 6; i++) coins[nc++] = (Vec2){ (float)i * 0.045f, 0.0f };   /* a row across the middle of the board */
    Vec2 from = POCKET_CENTERS[0], to = { 0.0f, BASELINE_Y_SOUTH };
    if (from.y < 0.0f) from = POCKET_CENTERS[1];
    if (from.y < 0.0f) from = POCKET_CENTERS[2];
    if (from.y < 0.0f) from = POCKET_CENTERS[3];
    Vec2 path[64];
    int n = striker_path_plan(from, to, coins, nc, path, 64);
    check_path(path, n, from, to, coins, nc);
    TEST_ASSERT_TRUE_MESSAGE(n > 2, "the wall needs a detour");
}

void test_random_coin_fields_never_touched(void) {
    for (int trial = 0; trial < 60; trial++) {
        Vec2 coins[12];
        int nc = 0;
        for (int i = 0; i < 12; i++) {
            Vec2 c = { (rnd() - 0.5f) * 0.8f, (rnd() - 0.5f) * 0.8f };
            bool ok = true;
            for (int j = 0; j < nc; j++) {
                float dx = c.x - coins[j].x, dy = c.y - coins[j].y;
                if (dx * dx + dy * dy < 4.0f * PIECE_RADIUS_NORM * PIECE_RADIUS_NORM) ok = false;
            }
            if (ok) coins[nc++] = c;
        }
        Vec2 from = POCKET_CENTERS[trial % 4];
        Vec2 to;
        switch (trial % 4) {
            case 0: to = (Vec2){ 0.0f, BASELINE_Y_NORTH }; break;
            case 1: to = (Vec2){ 0.0f, BASELINE_Y_SOUTH }; break;
            case 2: to = (Vec2){ BASELINE_X_EAST, 0.0f }; break;
            default: to = (Vec2){ BASELINE_X_WEST, 0.0f }; break;
        }
        bool blocked = false;
        for (int c = 0; c < nc; c++) {
            float dx = to.x - coins[c].x, dy = to.y - coins[c].y;
            if (sqrtf(dx * dx + dy * dy) < STRIKER_PATH_CLEAR) blocked = true;
        }
        if (blocked) continue;                       /* the caller moves such a goal along the baseline */
        Vec2 path[64];
        int n = striker_path_plan(from, to, coins, nc, path, 64);
        check_path(path, n, from, to, coins, nc);
    }
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_empty_board_is_a_straight_line);
    RUN_TEST(test_wall_of_coins_is_gone_around);
    RUN_TEST(test_random_coin_fields_never_touched);
    return UNITY_END();
}

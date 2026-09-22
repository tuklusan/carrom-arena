#include "unity.h"
#include "common/types.h"

void setUp(void) {}
void tearDown(void) {}

#define LOCKED_MSG "LOCKED by operator 2026-09-21 (beta-0.0.6): do not change without an explicit operator instruction"

void test_board_side_norm(void) {
    TEST_ASSERT_FLOAT_WITHIN(1e-6f, 1.0f, BOARD_SIDE_NORM);
    if (BOARD_SIDE_NORM != 1.0f) {
        printf("%s\n", LOCKED_MSG);
    }
}

void test_cushion_thickness(void) {
    TEST_ASSERT_FLOAT_WITHIN(1e-6f, 0.025f, CUSHION_THICKNESS);
    if (CUSHION_THICKNESS != 0.025f) {
        printf("%s\n", LOCKED_MSG);
    }
}

void test_pocket_radius(void) {
    TEST_ASSERT_FLOAT_WITHIN(1e-6f, 0.030f, POCKET_RADIUS_NORM);
    if (POCKET_RADIUS_NORM != 0.030f) {
        printf("%s\n", LOCKED_MSG);
    }
}

void test_piece_radius(void) {
    TEST_ASSERT_FLOAT_WITHIN(1e-6f, 0.021f, PIECE_RADIUS_NORM);
    if (PIECE_RADIUS_NORM != 0.021f) {
        printf("%s\n", LOCKED_MSG);
    }
}

void test_striker_radius(void) {
    TEST_ASSERT_FLOAT_WITHIN(1e-6f, 0.028f, STRIKER_RADIUS_NORM);
    if (STRIKER_RADIUS_NORM != 0.028f) {
        printf("%s\n", LOCKED_MSG);
    }
}

void test_pocket_centers(void) {
    extern const Vec2 POCKET_CENTERS[4];
    Vec2 expected[4] = {
        {-0.470f,  0.470f},
        { 0.470f,  0.470f},
        {-0.470f, -0.470f},
        { 0.470f, -0.470f}
    };
    for (int i = 0; i < 4; i++) {
        TEST_ASSERT_FLOAT_WITHIN(1e-6f, expected[i].x, POCKET_CENTERS[i].x);
        TEST_ASSERT_FLOAT_WITHIN(1e-6f, expected[i].y, POCKET_CENTERS[i].y);
        if (POCKET_CENTERS[i].x != expected[i].x || POCKET_CENTERS[i].y != expected[i].y) {
            printf("Pocket %d mismatch: %s\n", i, LOCKED_MSG);
        }
    }
}

void test_baselines(void) {
    // These are derived but should be checked against their intended target values
    // BASELINE_Y_NORTH = 0.5 - 0.025 - 0.028 = 0.447
    // BASELINE_Y_SOUTH = -0.5 + 0.025 + 0.028 = -0.447
    // BASELINE_X_EAST  = 0.5 - 0.025 - 0.028 = 0.447
    // BASELINE_X_WEST  = -0.5 + 0.025 + 0.028 = -0.447
    TEST_ASSERT_FLOAT_WITHIN(1e-6f, 0.447f, BASELINE_Y_NORTH);
    TEST_ASSERT_FLOAT_WITHIN(1e-6f, -0.447f, BASELINE_Y_SOUTH);
    TEST_ASSERT_FLOAT_WITHIN(1e-6f, 0.447f, BASELINE_X_EAST);
    TEST_ASSERT_FLOAT_WITHIN(1e-6f, -0.447f, BASELINE_X_WEST);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_board_side_norm);
    RUN_TEST(test_cushion_thickness);
    RUN_TEST(test_pocket_radius);
    RUN_TEST(test_piece_radius);
    RUN_TEST(test_striker_radius);
    RUN_TEST(test_pocket_centers);
    RUN_TEST(test_baselines);
    return UNITY_END();
}

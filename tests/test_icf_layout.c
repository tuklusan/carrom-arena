#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include "unity.h"
#include "common/vecmath.h"
#include "common/types.h"
#include "game/board.h"

void setUp(void) {}
void tearDown(void) {}

typedef struct {
    int id;
    float angle;
    float dist;
    PieceColor color;
} PieceInfo;

int compare_piece_info(const void* a, const void* b) {
    const PieceInfo* p1 = (const PieceInfo*)a;
    const PieceInfo* p2 = (const PieceInfo*)b;
    if (p1->angle < p2->angle) return -1;
    if (p1->angle > p2->angle) return 1;
    return 0;
}

void test_ICF_Layout_Counts_And_IDs(void) {
    BoardState board;
    board_state_init(&board);
    board_setup_initial_formation(&board, NULL);

    TEST_ASSERT_EQUAL_INT(9, board.white_on_board);
    TEST_ASSERT_EQUAL_INT(9, board.black_on_board);
    TEST_ASSERT_TRUE(board.queen_on_board);

    for (int i = 0; i < 9; i++) {
        TEST_ASSERT_EQUAL_MESSAGE(PIECE_WHITE, board.pieces[i].color, "ID 0-8 must be White");
    }
    for (int i = 9; i < 18; i++) {
        TEST_ASSERT_EQUAL_MESSAGE(PIECE_BLACK, board.pieces[i].color, "ID 9-17 must be Black");
    }
}

void test_ICF_Layout_Queen_Position(void) {
    BoardState board;
    board_state_init(&board);
    board_setup_initial_formation(&board, NULL);

    TEST_ASSERT_FLOAT_WITHIN(1e-5f, 0.0f, board.pieces[QUEEN_ID].position.x);
    TEST_ASSERT_FLOAT_WITHIN(1e-5f, 0.0f, board.pieces[QUEEN_ID].position.y);
    TEST_ASSERT_EQUAL(PIECE_QUEEN, board.pieces[QUEEN_ID].color);
}

void test_ICF_Layout_Geometric_And_Symmetry(void) {
    BoardState board;
    board_state_init(&board);
    board_setup_initial_formation(&board, NULL);

    float r = PIECE_RADIUS_NORM;
    int inner_count = 0, tip_count = 0, notch_count = 0;

    for (int i = 0; i < 18; i++) {
        float dx = board.pieces[i].position.x;
        float dy = board.pieces[i].position.y;
        float dist = sqrtf(dx * dx + dy * dy);

        if (dist < 2.1f * r) {
            inner_count++;
            TEST_ASSERT_FLOAT_WITHIN(1e-4f, 2.0f * r, dist);
        } else if (dist > 3.9f * r) {
            tip_count++;
            TEST_ASSERT_FLOAT_WITHIN(1e-4f, 4.0f * r, dist);
        } else {
            notch_count++;
            TEST_ASSERT_FLOAT_WITHIN(1e-4f, 2.0f * 1.73205081f * r, dist);
        }
    }

    TEST_ASSERT_EQUAL_INT(6, inner_count);
    TEST_ASSERT_EQUAL_INT(6, tip_count);
    TEST_ASSERT_EQUAL_INT(6, notch_count);

    // Verify 6-fold symmetry
    for (int i = 0; i < 18; i++) {
        Vec2 pos = board.pieces[i].position;
        float dist = sqrtf(pos.x * pos.x + pos.y * pos.y);
        float angle = atan2f(pos.y, pos.x);

        for (int k = 1; k < 6; k++) {
            float rotated_angle = angle + (float)k * (M_PI / 3.0f);
            Vec2 rotated_pos = { dist * cosf(rotated_angle), dist * sinf(rotated_angle) };
            
            bool found = false;
            for (int j = 0; j < 18; j++) {
                float d_sq = powf(board.pieces[j].position.x - rotated_pos.x, 2) + 
                            powf(board.pieces[j].position.y - rotated_pos.y, 2);
                if (d_sq < 1e-4f) {
                    found = true;
                    break;
                }
            }
            TEST_ASSERT_TRUE_MESSAGE(found, "6-fold symmetry violated");
        }
    }
}

void test_ICF_Layout_Color_Alternation(void) {
    BoardState board;
    board_state_init(&board);
    board_setup_initial_formation(&board, NULL);

    float r = PIECE_RADIUS_NORM;
    PieceInfo inner[6], tips[6], notches[6];
    int ic = 0, tc = 0, nc = 0;

    for (int i = 0; i < 18; i++) {
        float dist = sqrtf(board.pieces[i].position.x * board.pieces[i].position.x + 
                           board.pieces[i].position.y * board.pieces[i].position.y);
        float angle = atan2f(board.pieces[i].position.y, board.pieces[i].position.x);
        PieceColor color = board.pieces[i].color;

        if (dist < 2.1f * r) {
            inner[ic++] = (PieceInfo){i, angle, dist, color};
        } else if (dist > 3.9f * r) {
            tips[tc++] = (PieceInfo){i, angle, dist, color};
        } else {
            notches[nc++] = (PieceInfo){i, angle, dist, color};
        }
    }

    qsort(inner, 6, sizeof(PieceInfo), compare_piece_info);
    qsort(tips, 6, sizeof(PieceInfo), compare_piece_info);
    qsort(notches, 6, sizeof(PieceInfo), compare_piece_info);

    for (int i = 0; i < 6; i++) {
        // All rings should now start with PIECE_WHITE when sorted by angle
        TEST_ASSERT_EQUAL_MESSAGE(i % 2 == 0 ? PIECE_WHITE : PIECE_BLACK, inner[i].color, "Inner ring alternation failed");
        TEST_ASSERT_EQUAL_MESSAGE(i % 2 == 0 ? PIECE_WHITE : PIECE_BLACK, tips[i].color, "Tips ring alternation failed");
        TEST_ASSERT_EQUAL_MESSAGE(i % 2 == 0 ? PIECE_WHITE : PIECE_BLACK, notches[i].color, "Notches ring alternation failed");
    }
}

void test_ICF_Layout_No_Overlaps(void) {
    BoardState board;
    board_state_init(&board);
    board_setup_initial_formation(&board, NULL);

    float min_dist = 2.0f * PIECE_RADIUS_NORM;

    for (int i = 0; i < 18; i++) {
        for (int j = i + 1; j < 18; j++) {
            float dx = board.pieces[i].position.x - board.pieces[j].position.x;
            float dy = board.pieces[i].position.y - board.pieces[j].position.y;
            float dist = sqrtf(dx * dx + dy * dy);
            TEST_ASSERT_TRUE_MESSAGE(dist >= min_dist - 1e-5f, "Pieces overlap!");
        }
    }
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_ICF_Layout_Counts_And_IDs);
    RUN_TEST(test_ICF_Layout_Queen_Position);
    RUN_TEST(test_ICF_Layout_Geometric_And_Symmetry);
    RUN_TEST(test_ICF_Layout_Color_Alternation);
    RUN_TEST(test_ICF_Layout_No_Overlaps);
    return UNITY_END();
}

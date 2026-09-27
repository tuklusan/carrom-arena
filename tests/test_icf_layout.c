#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include "unity.h"
#include "common/vecmath.h"
#include "common/types.h"
#include "game/board.h"

void setUp(void) {}
void tearDown(void) {}

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

/* ICF Rule 41(a): queen in the centre circle; the coins around her in the first row alternate black and white; in the second
 * row three white coins form a "Y" with the three white coins of the first row, and the remaining space is filled up by
 * placing black and white coins alternately. */
typedef struct { float angle; PieceColor color; } RingSlot;

static int compare_slot(const void* a, const void* b) {
    float d = ((const RingSlot*)a)->angle - ((const RingSlot*)b)->angle;
    return (d > 0.0f) - (d < 0.0f);
}

void test_ICF_Layout_Rule41a_First_Row_Alternates(void) {
    BoardState board;
    board_state_init(&board);
    board_setup_initial_formation(&board, NULL);
    float r = PIECE_RADIUS_NORM;

    RingSlot inner[6];
    int n = 0;
    for (int i = 0; i < 18; i++) {
        Vec2 p = board.pieces[i].position;
        if (sqrtf(p.x * p.x + p.y * p.y) < 2.1f * r) inner[n++] = (RingSlot){ atan2f(p.y, p.x), board.pieces[i].color };
    }
    TEST_ASSERT_EQUAL_INT(6, n);
    qsort(inner, 6, sizeof(RingSlot), compare_slot);
    for (int i = 0; i < 6; i++) {
        TEST_ASSERT_TRUE_MESSAGE(inner[i].color != inner[(i + 1) % 6].color, "Rule 41(a): the first row must alternate black and white");
    }
}

void test_ICF_Layout_Rule41a_Second_Row_Y_And_Alternation(void) {
    BoardState board;
    board_state_init(&board);
    board_setup_initial_formation(&board, NULL);
    float r = PIECE_RADIUS_NORM;

    RingSlot second[12];
    int n = 0;
    for (int i = 0; i < 18; i++) {
        Vec2 p = board.pieces[i].position;
        if (sqrtf(p.x * p.x + p.y * p.y) > 2.1f * r) second[n++] = (RingSlot){ atan2f(p.y, p.x), board.pieces[i].color };
    }
    TEST_ASSERT_EQUAL_INT(12, n);
    /* the second row alternates black and white all the way round its 12 places */
    qsort(second, 12, sizeof(RingSlot), compare_slot);
    for (int i = 0; i < 12; i++) {
        TEST_ASSERT_TRUE_MESSAGE(second[i].color != second[(i + 1) % 12].color, "Rule 41(a): the second row must alternate black and white");
    }

    /* the three second-row coins lined up with the three white first-row coins are white: the arms of the "Y" */
    int y_arms = 0;
    for (int i = 0; i < 18; i++) {
        Vec2 p = board.pieces[i].position;
        float dist = sqrtf(p.x * p.x + p.y * p.y);
        if (dist >= 2.1f * r || dist < 1.9f * r || board.pieces[i].color != PIECE_WHITE) continue;
        float ang = atan2f(p.y, p.x);
        bool found = false;
        for (int j = 0; j < 18; j++) {
            Vec2 q = board.pieces[j].position;
            if (sqrtf(q.x * q.x + q.y * q.y) > 3.9f * r) {
                float da = atan2f(q.y, q.x) - ang;
                if (fabsf(da) < 1e-3f || fabsf(fabsf(da) - 2.0f * (float)M_PI) < 1e-3f) {
                    TEST_ASSERT_EQUAL_MESSAGE(PIECE_WHITE, board.pieces[j].color, "Rule 41(a): the coin behind a white first-row coin must be white (the Y)");
                    found = true;
                }
            }
        }
        TEST_ASSERT_TRUE_MESSAGE(found, "no second-row coin behind a white first-row coin");
        y_arms++;
    }
    TEST_ASSERT_EQUAL_INT_MESSAGE(3, y_arms, "the Y has three arms");
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
    RUN_TEST(test_ICF_Layout_Rule41a_First_Row_Alternates);
    RUN_TEST(test_ICF_Layout_Rule41a_Second_Row_Y_And_Alternation);
    RUN_TEST(test_ICF_Layout_No_Overlaps);
    return UNITY_END();
}

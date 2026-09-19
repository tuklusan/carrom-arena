#include <math.h>
#include <stdlib.h>
#include "unity.h"
#include "common/types.h"
#include "game/board.h"

void setUp(void) {}
void tearDown(void) {}

void test_ICF_Layout_Counts(void) {
    BoardState board;
    board_state_init(&board);
    board_setup_initial_formation(&board, NULL);

    TEST_ASSERT_EQUAL_INT(9, board.white_on_board);
    TEST_ASSERT_EQUAL_INT(9, board.black_on_board);
    TEST_ASSERT_TRUE(board.queen_on_board);
    TEST_ASSERT_EQUAL_INT(0, board.pocketed_count);
}

void test_ICF_Layout_Queen_Position(void) {
    BoardState board;
    board_state_init(&board);
    board_setup_initial_formation(&board, NULL);

    TEST_ASSERT_FLOAT_WITHIN(1e-5f, 0.0f, board.pieces[QUEEN_ID].position.x);
    TEST_ASSERT_FLOAT_WITHIN(1e-5f, 0.0f, board.pieces[QUEEN_ID].position.y);
    TEST_ASSERT_EQUAL(PIECE_QUEEN, board.pieces[QUEEN_ID].color);
}

void test_ICF_Layout_Rings_Positions(void) {
    BoardState board;
    board_state_init(&board);
    board_setup_initial_formation(&board, NULL);

    // Inner ring: pieces 0-5
    for (int i = 0; i < 6; i++) {
        float dist = sqrtf(board.pieces[i].position.x * board.pieces[i].position.x + 
                           board.pieces[i].position.y * board.pieces[i].position.y);
        TEST_ASSERT_FLOAT_WITHIN(1e-5f, 0.04f, dist); // INNER_RING_RADIUS
    }

    // Outer ring: pieces 6-17
    for (int i = 6; i < 18; i++) {
        float dist = sqrtf(board.pieces[i].position.x * board.pieces[i].position.x + 
                           board.pieces[i].position.y * board.pieces[i].position.y);
        TEST_ASSERT_FLOAT_WITHIN(1e-5f, 0.08f, dist); // OUTER_RING_RADIUS
    }
}

void test_ICF_Layout_Alternating_Colors(void) {
    BoardState board;
    board_state_init(&board);
    board_setup_initial_formation(&board, NULL);

    // Inner ring colors: 0=W, 1=B, 2=W, 3=B, 4=W, 5=B
    for (int i = 0; i < 6; i++) {
        PieceColor expected = (i % 2 == 0) ? PIECE_WHITE : PIECE_BLACK;
        TEST_ASSERT_EQUAL(expected, board.pieces[i].color);
    }

    // Outer ring colors: 6=W, 7=B ... 17=B
    for (int i = 0; i < 12; i++) {
        PieceColor expected = (i % 2 == 0) ? PIECE_WHITE : PIECE_BLACK;
        TEST_ASSERT_EQUAL(expected, board.pieces[i + 6].color);
    }
}

void test_ICF_Layout_No_Overlaps(void) {
    BoardState board;
    board_state_init(&board);
    board_setup_initial_formation(&board, NULL);

    float min_dist_sq = (2.0f * PIECE_RADIUS_NORM) * (2.0f * PIECE_RADIUS_NORM);

    for (int i = 0; i < MAX_PIECES; i++) {
        if (!board.pieces[i].on_board) continue;
        for (int j = i + 1; j < MAX_PIECES; j++) {
            if (!board.pieces[j].on_board) continue;
            
            float dx = board.pieces[i].position.x - board.pieces[j].position.x;
            float dy = board.pieces[i].position.y - board.pieces[j].position.y;
            float dist_sq = dx*dx + dy*dy;
            
            TEST_ASSERT_TRUE_MESSAGE(dist_sq > min_dist_sq - 1e-6f, "Pieces overlap!");
        }
    }
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_ICF_Layout_Counts);
    RUN_TEST(test_ICF_Layout_Queen_Position);
    RUN_TEST(test_ICF_Layout_Rings_Positions);
    RUN_TEST(test_ICF_Layout_Alternating_Colors);
    RUN_TEST(test_ICF_Layout_No_Overlaps);
    return UNITY_END();
}

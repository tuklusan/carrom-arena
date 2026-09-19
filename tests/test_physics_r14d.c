#include "unity.h"
#include "common/types.h"
#include "physics/physics.h"
#include "common/vecmath.h"
#include "game/board.h"

void setUp(void) {}
void tearDown(void) {}

void test_restitution_low_speed(void) {
    PhysicsWorld* pw = physics_create();
    BoardState board;
    board_state_init(&board);
    
    board.pieces[0].position = (Vec2){-0.1f, 0.0f};
    board.pieces[0].velocity = (Vec2){0.5f, 0.0f};
    board.pieces[0].on_board = true;
    board.pieces[0].pocketed = false;
    
    board.pieces[1].position = (Vec2){0.1f, 0.0f};
    board.pieces[1].velocity = (Vec2){0.0f, 0.0f};
    board.pieces[1].on_board = true;
    board.pieces[1].pocketed = false;
    
    board.white_on_board = 2;
    physics_sync_from_board(pw, &board, SEAT_NORTH);
    
    for (int i = 0; i < 200; i++) physics_step(pw, PHYSICS_DT);
    
    Vec2 pos[MAX_PIECES];
    physics_get_positions(pw, pos);
    bool bounced = (pos[0].x < pos[1].x);
    
    physics_destroy(pw);
    TEST_ASSERT_TRUE_MESSAGE(bounced, "Low speed collision was inelastic");
}

void test_momentum_transfer(void) {
    float speeds[] = {0.3f, 1.0f, 3.0f};
    for (int s = 0; s < 3; s++) {
        PhysicsWorld* pw = physics_create();
        BoardState board;
        board_state_init(&board);
        board.pieces[0].position = (Vec2){0.0f, 0.0f};
        board.pieces[0].on_board = true;
        board.white_on_board = 1;
        physics_sync_from_board(pw, &board, SEAT_NORTH);
        
        physics_place_striker(pw, SEAT_NORTH, (Vec2){-0.1f, 0.0f});
        physics_apply_shot(pw, 0.0f, speeds[s] / 5.0f);
        
        for (int i = 0; i < 500; i++) physics_step(pw, PHYSICS_DT);
        
        Vec2 pos[MAX_PIECES];
        physics_get_positions(pw, pos);
        physics_destroy(pw);
        TEST_ASSERT_TRUE_MESSAGE(pos[0].x > 0.001f, "No momentum transfer");
    }
}

void test_no_tunnelling(void) {
    PhysicsWorld* pw = physics_create();
    BoardState board;
    board_state_init(&board);
    board.pieces[0].position = (Vec2){0.0f, 0.0f};
    board.pieces[0].on_board = true;
    board.white_on_board = 1;
    physics_sync_from_board(pw, &board, SEAT_NORTH);
    
    physics_place_striker(pw, SEAT_NORTH, (Vec2){-0.1f, 0.0f});
    physics_apply_shot(pw, 0.0f, 1.0f); // 5 units/s
    
    for (int i = 0; i < 200; i++) physics_step(pw, PHYSICS_DT);
    
    Vec2 s_pos;
    physics_get_striker_position(pw, &s_pos);
    physics_destroy(pw);
    TEST_ASSERT_FALSE_MESSAGE(s_pos.x > 0.1f, "Striker tunneled through piece");
}

void test_rack_stability(void) {
    PhysicsWorld* pw = physics_create();
    BoardState board;
    board_state_init(&board);
    // Place pieces far apart to ensure they are stable
    board.pieces[0].position = (Vec2){-0.2f, 0.0f};
    board.pieces[0].on_board = true;
    board.pieces[1].position = (Vec2){0.2f, 0.0f};
    board.pieces[1].on_board = true;
    board.white_on_board = 2;
    physics_sync_from_board(pw, &board, SEAT_NORTH);
    
    Vec2 start_pos[MAX_PIECES];
    physics_get_positions(pw, start_pos);
    
    for (int i = 0; i < 360; i++) physics_step(pw, PHYSICS_DT); // 3s
    
    Vec2 end_pos[MAX_PIECES];
    physics_get_positions(pw, end_pos);
    
    physics_destroy(pw);
    TEST_ASSERT_FLOAT_WITHIN(1e-3f, start_pos[0].x, end_pos[0].x);
    TEST_ASSERT_FLOAT_WITHIN(1e-3f, start_pos[1].x, end_pos[1].x);
}

void test_cushion_bounce(void) {
    PhysicsWorld* pw = physics_create();
    // Place striker and shoot East
    physics_place_striker(pw, SEAT_NORTH, (Vec2){0.1f, 0.0f});
    physics_apply_shot(pw, 0.0f, 0.5f); // East
    
    // Step until it hits the cushion (at x=0.5)
    for (int i = 0; i < 200; i++) physics_step(pw, PHYSICS_DT);
    
    Vec2 vel;
    physics_get_striker_velocity(pw, &vel);
    physics_destroy(pw);
    TEST_ASSERT_TRUE_MESSAGE(vel.x < 0, "Striker did not bounce off right cushion");
}

void test_pocket_capture(void) {
    PhysicsWorld* pw = physics_create();
    BoardState board;
    board_state_init(&board);
    // Place piece exactly where a pocket sensor is (corner)
    // POCKET_CENTERS[0] is typically top-left
    board.pieces[0].position = (Vec2){-0.47f, 0.47f}; 
    board.pieces[0].on_board = true;
    board.white_on_board = 1;
    physics_sync_from_board(pw, &board, SEAT_NORTH);
    
    // Step to allow sensor to trigger
    for (int i = 0; i < 100; i++) physics_step(pw, PHYSICS_DT);
    
    Vec2 pos[MAX_PIECES];
    physics_get_positions(pw, pos);
    physics_destroy(pw);
    // Pocketed pieces are removed/set to 0 in get_positions
    TEST_ASSERT_EQUAL_FLOAT(0.0f, pos[0].x);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_restitution_low_speed);
    RUN_TEST(test_momentum_transfer);
    RUN_TEST(test_no_tunnelling);
    RUN_TEST(test_rack_stability);
    RUN_TEST(test_cushion_bounce);
    RUN_TEST(test_pocket_capture);
    return UNITY_END();
}

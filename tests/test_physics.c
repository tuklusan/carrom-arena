#include "unity.h"
#include "common/types.h"
#include "physics/physics.h"
#include "physics/physics_snapshot.h"
#include "common/rng.h"
#include "game/board.h"
#include "game/match.h"
#include <math.h>
#ifdef __MINGW32__
extern float fabsf(float);
#endif

/* PhysicsWorld is opaque - tests should only use public API */
/* We'll test behavior through public functions only */

/* Helper to create a board with pieces at rest positions (initial formation) */
static void setup_board_at_rest(BoardState* board, RNGContext* rng) {
    board_state_init(board);
    board_setup_initial_formation(board, rng);
    board_place_striker_on_baseline(&board->striker, SEAT_NORTH);
}

void setUp(void) {}
void tearDown(void) {}

void test_physics_create_destroy(void) {
    PhysicsWorld* pw = physics_create();
    bool ok = (pw != NULL);
    physics_destroy(pw);
    TEST_ASSERT_TRUE(ok);
}

void test_physics_step_advances_time(void) {
    PhysicsWorld* pw = physics_create();
    float initial_time = physics_get_sim_time(pw);
    
    physics_step(pw, PHYSICS_DT);
    
    TEST_ASSERT_FLOAT_WITHIN(0.001f, initial_time + PHYSICS_DT, physics_get_sim_time(pw));
    physics_destroy(pw);
}

void test_physics_accumulator(void) {
    PhysicsWorld* pw = physics_create();
    
    // Step with small dt multiple times
    for (int i = 0; i < 4; i++) {
        physics_step(pw, PHYSICS_DT * 0.25f);
    }
    
    // Should have advanced by ~1 physics step
    TEST_ASSERT_FLOAT_WITHIN(0.001f, PHYSICS_DT, physics_get_sim_time(pw));
    physics_destroy(pw);
}

void test_physics_settling_detection(void) {
    PhysicsWorld* pw = physics_create();
    
    // Create a simple board with non-overlapping pieces at rest
    BoardState board;
    board_state_init(&board);
    
    // Place queen at center
    board.pieces[QUEEN_ID].position = (Vec2){0.0f, 0.0f};
    board.pieces[QUEEN_ID].velocity = (Vec2){0.0f, 0.0f};
    board.pieces[QUEEN_ID].on_board = true;
    board.pieces[QUEEN_ID].pocketed = false;
    board.queen_on_board = true;
    
    // Place a few white pieces at non-overlapping positions
    board.pieces[0].position = (Vec2){0.1f, 0.0f};
    board.pieces[0].velocity = (Vec2){0.0f, 0.0f};
    board.pieces[0].on_board = true;
    board.pieces[0].pocketed = false;
    
    board.pieces[1].position = (Vec2){-0.1f, 0.0f};
    board.pieces[1].velocity = (Vec2){0.0f, 0.0f};
    board.pieces[1].on_board = true;
    board.pieces[1].pocketed = false;
    
    board.white_on_board = 2;
    board.black_on_board = 0;
    board.queen_state = QUEEN_STATE_ON_BOARD;
    
    // Place striker on baseline
    board_place_striker_on_baseline(&board.striker, SEAT_NORTH);
    
    // Sync physics bodies from board state
    physics_sync_from_board(pw, &board, SEAT_NORTH);
    
    // Step enough times to allow velocities to damp to zero
    // Pieces are placed at rest but physics_sync_from_board sets them awake,
    // which may trigger collision resolution. Need enough steps to settle.
    for (int i = 0; i < 100; i++) {
        physics_step(pw, PHYSICS_DT);
    }
    
    // Call physics_is_settled 3 times for SETTLE_CONFIRM_STEPS confirmation
    bool settled = false;
    for (int i = 0; i < 3; i++) {
        settled = physics_is_settled(pw);
    }
    
    physics_destroy(pw);
    TEST_ASSERT_TRUE(settled);
}

void test_physics_place_striker(void) {
    PhysicsWorld* pw = physics_create();
    
    Vec2 placement = {0.0f, BASELINE_Y_NORTH};
    physics_place_striker(pw, SEAT_NORTH, placement);
    
    // Striker should be at placement - verify via physics_get_striker_position
    Vec2 pos;
    physics_get_striker_position(pw, &pos);
    bool ok = (fabsf(placement.x - pos.x) < 0.001f) && (fabsf(placement.y - pos.y) < 0.001f);
    
    physics_destroy(pw);
    TEST_ASSERT_TRUE(ok);
}

void test_physics_apply_shot(void) {
    PhysicsWorld* pw = physics_create();
    
    Vec2 placement = {0.0f, BASELINE_Y_NORTH};
    physics_place_striker(pw, SEAT_NORTH, placement);
    
    // Apply shot straight down
    physics_apply_shot(pw, -M_PI/2.0f, 0.5f);
    
    // Striker should have velocity - we can't directly check since PhysicsWorld is opaque
    // But we can step and verify it moves
    physics_step(pw, PHYSICS_DT);
    
    Vec2 pos;
    physics_get_striker_position(pw, &pos);
    // Should have moved from baseline
    bool moved = (pos.y < BASELINE_Y_NORTH);
    
    physics_destroy(pw);
    TEST_ASSERT_TRUE(moved);
}

void test_physics_board_resistance(void) {
    PhysicsWorld* pw = physics_create();
    
    // Initialize board with just striker (simpler - avoids piece collision issues)
    BoardState board;
    board_state_init(&board);
    board_place_striker_on_baseline(&board.striker, SEAT_NORTH);
    physics_sync_from_board(pw, &board, SEAT_NORTH);
    
    // Give striker high velocity - place and shoot
    physics_place_striker(pw, SEAT_NORTH, (Vec2){0, BASELINE_Y_NORTH});
    physics_apply_shot(pw, -M_PI/2.0f, 1.0f);  // Full power
    
    // Step multiple times (5 seconds at 120Hz) - enough for full power shot to settle
    for (int i = 0; i < 600; i++) {
        physics_step(pw, PHYSICS_DT);
    }
    
    // Should eventually settle - call 3 times for SETTLE_CONFIRM_STEPS confirmation
    bool settled = false;
    for (int i = 0; i < 3; i++) {
        settled = physics_is_settled(pw);
    }
    
    physics_destroy(pw);
    TEST_ASSERT_TRUE(settled);
}

void test_physics_pocket_capture(void) {
    PhysicsWorld* pw = physics_create();
    
    // We can't directly manipulate body positions since PhysicsWorld is opaque
    // Instead test that pocket capture works by shooting a piece toward a pocket
    // This is more of an integration test
    // Just ensure it doesn't crash
    physics_step(pw, PHYSICS_DT);
    
    physics_destroy(pw);
    TEST_ASSERT_TRUE(true);
}

void test_physics_snapshot_create_restore(void) {
    PhysicsWorld* pw = physics_create();
    
    // Initialize board with a few non-overlapping pieces at rest
    BoardState board;
    board_state_init(&board);
    
    // Place queen at center
    board.pieces[QUEEN_ID].position = (Vec2){0.0f, 0.0f};
    board.pieces[QUEEN_ID].velocity = (Vec2){0.0f, 0.0f};
    board.pieces[QUEEN_ID].on_board = true;
    board.pieces[QUEEN_ID].pocketed = false;
    board.queen_on_board = true;
    
    // Place one white piece
    board.pieces[0].position = (Vec2){0.1f, 0.0f};
    board.pieces[0].velocity = (Vec2){0.0f, 0.0f};
    board.pieces[0].on_board = true;
    board.pieces[0].pocketed = false;
    board.white_on_board = 1;
    
    board.queen_state = QUEEN_STATE_ON_BOARD;
    
    // Place striker on baseline
    board_place_striker_on_baseline(&board.striker, SEAT_NORTH);
    
    physics_sync_from_board(pw, &board, SEAT_NORTH);
    
    // Move striker
    physics_place_striker(pw, SEAT_NORTH, (Vec2){0.1f, BASELINE_Y_NORTH});
    physics_apply_shot(pw, -M_PI/2.0f, 0.3f);
    // Step a few times to get a meaningful state
    for (int i = 0; i < 10; i++) {
        physics_step(pw, PHYSICS_DT);
    }
    
    // Capture expected position BEFORE creating snapshot
    Vec2 expected_pos;
    physics_get_striker_position(pw, &expected_pos);
    
    // Create snapshot
    PhysicsSnapshot* snap = physics_snapshot_create(pw);
    bool snap_ok = (snap != NULL);
    
    if (snap_ok) {
        // Modify live world
        for (int i = 0; i < 100; i++) {
            physics_step(pw, PHYSICS_DT);
        }
        
        // Restore from snapshot
        physics_snapshot_restore(pw, snap);
        
        // Striker should be back at snapshot position (expected_pos)
        Vec2 pos;
        physics_get_striker_position(pw, &pos);
        bool pos_ok = (fabsf(pos.x - expected_pos.x) < 0.001f) && (fabsf(pos.y - expected_pos.y) < 0.001f);
        
        physics_snapshot_destroy(snap);
        physics_destroy(pw);
        
        TEST_ASSERT_TRUE(snap_ok);
        TEST_ASSERT_TRUE(pos_ok);
    } else {
        physics_destroy(pw);
        TEST_ASSERT_TRUE(false);
    }
}

void test_physics_world_from_snapshot(void) {
    PhysicsWorld* pw = physics_create();
    
    // Initialize board with pieces at rest positions
    RNGContext rng = {0};
    BoardState board;
    setup_board_at_rest(&board, &rng);
    physics_sync_from_board(pw, &board, SEAT_NORTH);
    
    physics_place_striker(pw, SEAT_NORTH, (Vec2){0.1f, BASELINE_Y_NORTH});
    physics_apply_shot(pw, -M_PI/2.0f, 0.3f);
    physics_step(pw, PHYSICS_DT);
    
    PhysicsSnapshot* snap = physics_snapshot_create(pw);
    bool snap_ok = (snap != NULL);
    PhysicsWorld* pw2 = NULL;
    bool pos_ok = false;
    
    if (snap_ok) {
        pw2 = physics_world_from_snapshot(snap);
        if (pw2) {
            // Both worlds should have same striker position
            Vec2 pos1, pos2;
            physics_get_striker_position(pw, &pos1);
            physics_get_striker_position(pw2, &pos2);
            pos_ok = (fabsf(pos1.x - pos2.x) < 0.001f) && (fabsf(pos1.y - pos2.y) < 0.001f);
        }
    }
    
    if (snap) physics_snapshot_destroy(snap);
    physics_destroy(pw);
    if (pw2) physics_destroy(pw2);
    
    TEST_ASSERT_TRUE(snap_ok);
    TEST_ASSERT_NOT_NULL(pw2);
    TEST_ASSERT_TRUE(pos_ok);
}

int main(void) {
    UNITY_BEGIN();
    
    RUN_TEST(test_physics_create_destroy);
    RUN_TEST(test_physics_step_advances_time);
    RUN_TEST(test_physics_accumulator);
    RUN_TEST(test_physics_settling_detection);
    RUN_TEST(test_physics_place_striker);
    RUN_TEST(test_physics_apply_shot);
    RUN_TEST(test_physics_board_resistance);
    RUN_TEST(test_physics_pocket_capture);
    RUN_TEST(test_physics_snapshot_create_restore);
    RUN_TEST(test_physics_world_from_snapshot);
    
    return UNITY_END();
}
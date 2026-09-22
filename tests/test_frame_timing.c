#include "unity.h"
#include "common/types.h"
#include "physics/physics.h"
#include "common/vecmath.h"

void setUp(void) {}
void tearDown(void) {}

/* 
 * Helper to simulate a set of frames and return the final striker position.
 * This mimics the loop in app_simulation_step.
 */
static Vec2 simulate_with_fps(float fps, float total_sim_time, float playback_speed) {
    PhysicsWorld* pw = physics_create();
    
    // Initial state: Striker at baseline, shot applied
    Vec2 placement = {0.1f, BASELINE_Y_NORTH};
    physics_place_striker(pw, SEAT_NORTH, placement);
    physics_apply_shot(pw, -M_PI/2.0f, 0.5f);

    double accumulator = 0.0;
    double dt = 1.0 / fps;
    double current_sim_time = 0.0;

    while (current_sim_time < total_sim_time) {
        accumulator += dt * playback_speed;
        
        int substeps = 0;
        while (accumulator >= PHYSICS_DT && substeps < MAX_SUBSTEPS) {
            physics_step(pw, PHYSICS_DT);
            accumulator -= PHYSICS_DT;
            substeps++;
        }
        current_sim_time += dt;
    }
    
    // Drain remaining accumulator to ensure both runs perform the same number of steps
    // given the same total_sim_time.
    while (accumulator >= PHYSICS_DT) {
        physics_step(pw, PHYSICS_DT);
        accumulator -= PHYSICS_DT;
    }

    Vec2 final_pos;
    physics_get_striker_position(pw, &final_pos);
    physics_destroy(pw);
    return final_pos;
}

void test_frame_timing_independence(void) {
    float total_sim_time = 1.0f;
    float playback_speed = 1.0f;

    // Simulate at 15 FPS
    Vec2 pos_15 = simulate_with_fps(15.0f, total_sim_time, playback_speed);
    
    // Simulate at 60 FPS
    Vec2 pos_60 = simulate_with_fps(60.0f, total_sim_time, playback_speed);

    // Results should be identical because the internal physics step is fixed
    TEST_ASSERT_FLOAT_WITHIN(0.0001f, pos_15.x, pos_60.x);
    TEST_ASSERT_FLOAT_WITHIN(0.0001f, pos_15.y, pos_60.y);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_frame_timing_independence);
    return UNITY_END();
}

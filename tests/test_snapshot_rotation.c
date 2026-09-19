#include <stdlib.h>
#include "unity.h"
#include "physics/physics.h"
#include "physics/physics_snapshot.h"
#include "common/types.h"
#include "common/vecmath.h"

void setUp(void) {}
void tearDown(void) {}

void test_Snapshot_Rotation_Preservation(void) {
    PhysicsWorld* pw = physics_create();
    
    // Select a piece and set a specific angle
    int piece_id = 0;
    float target_angle = 0.785398f; // 45 degrees
    
    // We can't easily set a body's rotation in physics.h without access to PhysicsWorld internals
    // But we can use a snapshot to inject one.
    PhysicsSnapshot* snap = calloc(1, sizeof(PhysicsSnapshot));
    
    // Setup snapshot with target angle
    snap->pieces[piece_id].position = (Vec2){0.1f, 0.1f};
    snap->pieces[piece_id].angle = target_angle;
    snap->pieces[piece_id].pocketed = false;
    snap->pieces[piece_id].awake = true;
    
    // Restore snapshot
    physics_restore_snapshot(pw, snap);
    
    // To verify, we need to get the rotation back. 
    // Since physics.h doesn't provide physics_get_rotation, 
    // we can take another snapshot and check its angle.
    PhysicsSnapshot* snap_back = physics_snapshot(pw);
    
    TEST_ASSERT_FLOAT_WITHIN(1e-5f, target_angle, snap_back->pieces[piece_id].angle);
    
    physics_snapshot_destroy(snap);
    physics_snapshot_destroy(snap_back);
    physics_destroy(pw);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_Snapshot_Rotation_Preservation);
    return UNITY_END();
}

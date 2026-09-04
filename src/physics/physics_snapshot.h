#ifndef CARROM_PHYSICS_SNAPSHOT_H
#define CARROM_PHYSICS_SNAPSHOT_H

#include "types.h"
#include <math.h>  // Must be before Box2D for sqrtf, atan2f, cosf, sinf
#include <box2d/box2d.h>

#ifdef __cplusplus
extern "C" {
#endif

/* -----------------------------------------------------------------------------
 * Physics Snapshot for AI Scratch Simulation
 * Deep clone/restore of Box2D world state
 * --------------------------------------------------------------------------- */

typedef struct PhysicsSnapshot {
    // Body states (positions, velocities, awake)
    struct {
        Vec2 position;
        Vec2 velocity;
        float angle;
        float angular_velocity;
        bool awake;
        bool pocketed;
    } pieces[MAX_PIECES];
    
    struct {
        Vec2 position;
        Vec2 velocity;
        float angle;
        float angular_velocity;
        bool awake;
        bool pocketed;
    } striker;
    
    // Pocketed tracking
    int pocketed_count;
    uint8_t pocketed_ids[19];
    PieceColor pocketed_colors[19];
    bool striker_pocketed;
    
    float sim_time;
    uint64_t step_count;
} PhysicsSnapshot;

/* Create deep snapshot of physics world */
PhysicsSnapshot* physics_snapshot_create(const PhysicsWorld* pw);

/* Destroy snapshot */
void physics_snapshot_destroy(PhysicsSnapshot* snap);

/* Restore physics world from snapshot */
void physics_snapshot_restore(PhysicsWorld* pw, const PhysicsSnapshot* snap);

/* Apply snapshot to a new physics world (for scratch sim) */
PhysicsWorld* physics_world_from_snapshot(const PhysicsSnapshot* snap);

#ifdef __cplusplus
}
#endif

#endif // CARROM_PHYSICS_SNAPSHOT_H
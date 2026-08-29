#ifndef CARROM_PHYSICS_H
#define CARROM_PHYSICS_H

#include "types.h"
#include <math.h>
#include <box2d/box2d.h>
#include "physics_snapshot.h"

#ifdef __cplusplus
extern "C" {
#endif

/* -----------------------------------------------------------------------------
 * Physics World Wrapper (Box2D v3)
 * Fixed timestep 1/120s, accumulator with max 4 substeps
 * --------------------------------------------------------------------------- */

#define PHYSICS_HZ 120
#define PHYSICS_DT (1.0f / PHYSICS_HZ)
#define MAX_SUBSTEPS 4

// Physics constants
#define BOARD_COULOMB     0.12f   // Coulomb friction coefficient
#define BOARD_VISCOUS     0.85f   // Viscous damping coefficient
#define SETTLE_SPEED_EPS  1e-4f   // Speed threshold for settling
#define SETTLE_ACCEL_EPS  1e-4f   // Acceleration threshold for settling
#define SETTLE_TIMEOUT_SECONDS 30.0f  // Max simulation time before forced settle
#define SETTLE_CONFIRM_STEPS 3      // Consecutive steps below threshold

// Opaque physics world
typedef struct PhysicsWorld PhysicsWorld;

// Create/destroy
PhysicsWorld* physics_create(void);
void physics_destroy(PhysicsWorld* pw);

// Fixed timestep step
void physics_step(PhysicsWorld* pw, float dt);

// Board resistance (Coulomb + viscous) applied post-step
void physics_apply_board_resistance(PhysicsWorld* pw);

// Settling detection
bool physics_is_settled(PhysicsWorld* pw);
float physics_get_sim_time(PhysicsWorld* pw);

// Striker placement and shot execution
void physics_place_striker(PhysicsWorld* pw, Seat seat, Vec2 placement);
void physics_apply_shot(PhysicsWorld* pw, float aim_angle, float power);

// Pocket capture queries
void physics_collect_pocketed(PhysicsWorld* pw, ShotResult* result);
void physics_get_final_positions(PhysicsWorld* pw, Vec2* positions);

// Snapshot for AI scratch simulation
PhysicsSnapshot* physics_snapshot(PhysicsWorld* pw);
void physics_snapshot_destroy(PhysicsSnapshot* snap);
void physics_restore_snapshot(PhysicsWorld* pw, const PhysicsSnapshot* snap);

// Body queries
int physics_get_body_count(PhysicsWorld* pw);
b2BodyId* physics_get_bodies(PhysicsWorld* pw, int* out_count);

// Get current positions for rendering (read-only)
void physics_get_positions(const PhysicsWorld* pw, Vec2* positions);
void physics_get_striker_position(const PhysicsWorld* pw, Vec2* pos);

// Sync physics bodies from board state (initial placement)
void physics_sync_from_board(PhysicsWorld* pw, const BoardState* board, Seat striker_seat);

#ifdef __cplusplus
}
#endif

#endif // CARROM_PHYSICS_H
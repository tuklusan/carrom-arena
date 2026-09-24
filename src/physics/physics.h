#ifndef CARROM_PHYSICS_H
#define CARROM_PHYSICS_H

#include "types.h"
#include <math.h>  // Must be before Box2D for sqrtf, atan2f, cosf, sinf
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
#define MAX_SUBSTEPS 40

// Physics constants
#define BOARD_COULOMB     0.10f    // Coulomb friction coefficient (stronger dry friction)
#define BOARD_VISCOUS     0.20f    // Viscous damping coefficient (stronger speed-dependent drag)
#define SETTLE_SPEED_EPS  1e-3f    // Speed threshold for settling (practical visible stop)
#define SETTLE_ACCEL_EPS  0.60f    // Acceleration threshold for settling (must exceed COULOMB)
#define SETTLE_TIMEOUT_SECONDS 30.0f   // Max simulation time before forced settle (restored for pocket capture)
#define SETTLE_CONFIRM_STEPS 5      // Consecutive steps below threshold (increased for reliability)

// Pocket capture
#define POCKET_CAPTURE_RADIUS_NORM (POCKET_RADIUS_NORM + PIECE_RADIUS_NORM)  // 0.051, original sensor radius

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

// Reset turn timer (sim_time and settle_confirm_steps) at turn transitions
void physics_reset_turn_timer(PhysicsWorld* pw);

// Striker placement and shot execution
void physics_place_striker(PhysicsWorld* pw, Seat seat, Vec2 placement);
void physics_apply_shot(PhysicsWorld* pw, float aim_angle, float power);

// Pocket capture queries
void physics_collect_pocketed(PhysicsWorld* pw, ShotResult* result);
bool physics_is_piece_pocketed(const PhysicsWorld* pw, int id);
/* Live linear velocity of a piece; returns false (vel zeroed) if it is pocketed or absent */
bool physics_get_piece_velocity(const PhysicsWorld* pw, int id, Vec2* vel);
void physics_consume_pocketed(PhysicsWorld* pw);
void physics_get_final_positions(PhysicsWorld* pw, Vec2* positions);

// Snapshot for AI scratch simulation
PhysicsSnapshot* physics_snapshot(PhysicsWorld* pw);
void physics_snapshot_destroy(PhysicsSnapshot* snap);
void physics_restore_snapshot(PhysicsWorld* pw, const PhysicsSnapshot* snap);


// Get current positions for rendering (read-only)
void physics_get_positions(const PhysicsWorld* pw, Vec2* positions);
void physics_get_striker_position(const PhysicsWorld* pw, Vec2* pos);

// Get striker velocity for diagnostics
void physics_get_striker_velocity(const PhysicsWorld* pw, Vec2* vel);

// Get previous positions for interpolation (read-only)
void physics_get_prev_positions(const PhysicsWorld* pw, Vec2* positions);
void physics_get_prev_striker_position(const PhysicsWorld* pw, Vec2* pos);


// Sync physics bodies from board state (initial placement)
void physics_sync_from_board(PhysicsWorld* pw, const BoardState* board, Seat striker_seat);

#ifdef __cplusplus
}
#endif

#endif // CARROM_PHYSICS_H
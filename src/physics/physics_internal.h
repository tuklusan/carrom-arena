#ifndef CARROM_PHYSICS_INTERNAL_H
#define CARROM_PHYSICS_INTERNAL_H

/* The one definition of PhysicsWorld, shared by physics.c and physics_snapshot.c (the snapshot code used to carry its own
 * hand-copied, shorter version of this struct: two different layouts of the same object, so a snapshot restore wrote
 * sim_time and settle_confirm_steps over unrelated fields). Not for use outside src/physics. */

#include "physics.h"

#define SOUND_QUEUE_MAX 128

struct PhysicsWorld {
    b2WorldId world_id;
    float accumulator;
    uint64_t step_count;
    int substeps;

    // Body tracking
    b2BodyId piece_bodies[MAX_PIECES];
    b2BodyId striker_body;
    b2BodyId cushion_bodies[4];
    b2BodyId pocket_sensors[4];
    b2ShapeId pocket_sensor_shapes[4];

    // Piece colors for pocketing (must match board->pieces[i].color)
    PieceColor piece_colors[MAX_PIECES];

    // Pocketed tracking
    bool piece_pocketed[MAX_PIECES];
    bool striker_pocketed;
    int pocketed_count;
    uint8_t pocketed_ids[MAX_PIECES];
    PieceColor pocketed_colors[MAX_PIECES];
    uint8_t pocketed_pocket_indices[MAX_PIECES];  // Which pocket each piece went into (0-3)
    Vec2 pocketed_last_pos[MAX_PIECES];           // where / how fast each pocketed piece was at the moment it fell in
    Vec2 pocketed_last_vel[MAX_PIECES];
    Vec2 striker_last_pos, striker_last_vel;
    int striker_pocket_index;

    // Previous-frame positions for render interpolation
    Vec2 prev_piece_positions[MAX_PIECES];
    Vec2 prev_striker_position;

    // Sound events recorded while stepping (drained by the app once per frame)
    SoundEvent sound_queue[SOUND_QUEUE_MAX];
    int sound_count;

    // Simulation time
    float sim_time;

    // Settling tracking
    uint32_t settle_confirm_steps;
};

/* Body factories shared with the snapshot restore (a coin or the striker whose body was destroyed by a pocket needs a fresh
 * one). The striker is a bullet body so that it cannot tunnel through a coin at full power. */
void physics_internal_make_piece_body(PhysicsWorld* pw, int i, Vec2 position);
void physics_internal_make_striker_body(PhysicsWorld* pw, Vec2 position);

#endif /* CARROM_PHYSICS_INTERNAL_H */

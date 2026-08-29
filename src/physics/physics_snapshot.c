#include <math.h>
#include "physics_snapshot.h"
#include "physics.h"
#include "common/types.h"
#include <stdlib.h>
#include <string.h>

/* Include internal PhysicsWorld definition */
struct PhysicsWorld {
    b2WorldId world_id;
    float accumulator;
    uint64_t step_count;
    int substeps;
    
    b2BodyId piece_bodies[MAX_PIECES];
    b2BodyId striker_body;
    b2BodyId cushion_bodies[4];
    b2BodyId pocket_sensors[4];
    
    bool piece_pocketed[MAX_PIECES];
    bool striker_pocketed;
    int pocketed_count;
    uint8_t pocketed_ids[19];
    PieceColor pocketed_colors[19];
    
    float sim_time;
    uint32_t settle_confirm_steps;
};

PhysicsSnapshot* physics_snapshot_create(const PhysicsWorld* pw) {
    PhysicsSnapshot* snap = calloc(1, sizeof(PhysicsSnapshot));
    if (!snap) return NULL;
    
    // Snapshot pieces
    for (int i = 0; i < MAX_PIECES; i++) {
        if (b2Body_IsValid(pw->piece_bodies[i]) && !pw->piece_pocketed[i]) {
            b2Vec2 pos = b2Body_GetPosition(pw->piece_bodies[i]);
            b2Vec2 vel = b2Body_GetLinearVelocity(pw->piece_bodies[i]);
            b2Rot rot = b2Body_GetRotation(pw->piece_bodies[i]);
            snap->pieces[i].position = (Vec2){ pos.x, pos.y };
            snap->pieces[i].velocity = (Vec2){ vel.x, vel.y };
            snap->pieces[i].angle = atan2f(rot.s, rot.c);
            snap->pieces[i].angular_velocity = b2Body_GetAngularVelocity(pw->piece_bodies[i]);
            snap->pieces[i].awake = b2Body_IsAwake(pw->piece_bodies[i]);
            snap->pieces[i].pocketed = false;
        } else {
            snap->pieces[i].pocketed = pw->piece_pocketed[i];
        }
    }
    
    // Snapshot striker
    if (b2Body_IsValid(pw->striker_body) && !pw->striker_pocketed) {
        b2Vec2 pos = b2Body_GetPosition(pw->striker_body);
        b2Vec2 vel = b2Body_GetLinearVelocity(pw->striker_body);
        b2Rot rot = b2Body_GetRotation(pw->striker_body);
        snap->striker.position = (Vec2){ pos.x, pos.y };
        snap->striker.velocity = (Vec2){ vel.x, vel.y };
        snap->striker.angle = atan2f(rot.s, rot.c);
        snap->striker.angular_velocity = b2Body_GetAngularVelocity(pw->striker_body);
        snap->striker.awake = b2Body_IsAwake(pw->striker_body);
        snap->striker.pocketed = false;
    } else {
        snap->striker.pocketed = pw->striker_pocketed;
    }
    
    // Pocketed tracking
    snap->pocketed_count = pw->pocketed_count;
    memcpy(snap->pocketed_ids, pw->pocketed_ids, sizeof(pw->pocketed_ids));
    memcpy(snap->pocketed_colors, pw->pocketed_colors, sizeof(pw->pocketed_colors));
    snap->striker_pocketed = pw->striker_pocketed;
    
    snap->sim_time = pw->sim_time;
    snap->step_count = pw->step_count;
    
    return snap;
}

void physics_snapshot_destroy(PhysicsSnapshot* snap) {
    free(snap);
}

void physics_snapshot_restore(PhysicsWorld* pw, const PhysicsSnapshot* snap) {
    // Restore pieces
    for (int i = 0; i < MAX_PIECES; i++) {
        if (b2Body_IsValid(pw->piece_bodies[i]) && !snap->pieces[i].pocketed) {
            b2Body_SetTransform(pw->piece_bodies[i], 
                (b2Vec2){ snap->pieces[i].position.x, snap->pieces[i].position.y },
                (b2Rot){ cosf(snap->pieces[i].angle * 0.5f), sinf(snap->pieces[i].angle * 0.5f) });
            b2Body_SetLinearVelocity(pw->piece_bodies[i], 
                (b2Vec2){ snap->pieces[i].velocity.x, snap->pieces[i].velocity.y });
            b2Body_SetAngularVelocity(pw->piece_bodies[i], snap->pieces[i].angular_velocity);
            if (snap->pieces[i].awake) {
                b2Body_SetAwake(pw->piece_bodies[i], true);
            }
        }
        pw->piece_pocketed[i] = snap->pieces[i].pocketed;
    }
    
    // Restore striker
    if (b2Body_IsValid(pw->striker_body) && !snap->striker.pocketed) {
        b2Body_SetTransform(pw->striker_body,
            (b2Vec2){ snap->striker.position.x, snap->striker.position.y },
            (b2Rot){ cosf(snap->striker.angle * 0.5f), sinf(snap->striker.angle * 0.5f) });
        b2Body_SetLinearVelocity(pw->striker_body,
            (b2Vec2){ snap->striker.velocity.x, snap->striker.velocity.y });
        b2Body_SetAngularVelocity(pw->striker_body, snap->striker.angular_velocity);
        if (snap->striker.awake) {
            b2Body_SetAwake(pw->striker_body, true);
        }
    }
    pw->striker_pocketed = snap->striker.pocketed;
    
    // Restore pocketed tracking
    pw->pocketed_count = snap->pocketed_count;
    memcpy(pw->pocketed_ids, snap->pocketed_ids, sizeof(snap->pocketed_ids));
    memcpy(pw->pocketed_colors, snap->pocketed_colors, sizeof(snap->pocketed_colors));
    pw->striker_pocketed = snap->striker_pocketed;
    
    pw->sim_time = snap->sim_time;
    pw->step_count = snap->step_count;
    pw->settle_confirm_steps = 0;
}

PhysicsWorld* physics_world_from_snapshot(const PhysicsSnapshot* snap) {
    // Create new physics world and restore from snapshot
    PhysicsWorld* pw = physics_create();
    if (!pw) return NULL;
    
    physics_snapshot_restore(pw, snap);
    return pw;
}
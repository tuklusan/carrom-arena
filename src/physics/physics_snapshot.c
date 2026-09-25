#define __USE_MINGW_ANSI_STDIO 1
#include <math.h>
#ifdef __MINGW32__
extern float atan2f(float, float);
extern float cosf(float);
extern float sinf(float);
#endif
#include <stdlib.h>
#include <string.h>
#include "physics_snapshot.h"
#include "common/types.h"

#include "physics_internal.h"

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
    memcpy(snap->pocketed_pocket_indices, pw->pocketed_pocket_indices, sizeof(pw->pocketed_pocket_indices));
    snap->striker_pocketed = pw->striker_pocketed;
    
    snap->sim_time = pw->sim_time;
    snap->step_count = pw->step_count;
    
    return snap;
}

void physics_snapshot_destroy(PhysicsSnapshot* snap) {
    free(snap);
}

void physics_snapshot_restore(PhysicsWorld* pw, const PhysicsSnapshot* snap) {
    // Restore pieces. A coin the snapshot has on the board but whose body was destroyed since (pocketed in a scratch
    // simulation) gets a fresh body; a coin the snapshot has pocketed must NOT keep a body: a world made from a snapshot
    // starts with every coin at the origin, and the pocketed ones would stay there as phantom coins in the middle of the
    // board, colliding with everything (and sliding without friction, because the pocketed flag exempts them from it).
    for (int i = 0; i < MAX_PIECES; i++) {
        if (snap->pieces[i].pocketed) {
            if (b2Body_IsValid(pw->piece_bodies[i])) {
                b2DestroyBody(pw->piece_bodies[i]);
                pw->piece_bodies[i] = (b2BodyId){0};
            }
        } else {
            if (!b2Body_IsValid(pw->piece_bodies[i])) physics_internal_make_piece_body(pw, i, snap->pieces[i].position);
            b2Body_SetTransform(pw->piece_bodies[i],
                (b2Vec2){ snap->pieces[i].position.x, snap->pieces[i].position.y },
                (b2Rot){ .c = cosf(snap->pieces[i].angle), .s = sinf(snap->pieces[i].angle) });
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
    if (snap->striker.pocketed) {
        if (b2Body_IsValid(pw->striker_body)) {
            b2DestroyBody(pw->striker_body);
            pw->striker_body = (b2BodyId){0};
        }
    } else {
        if (!b2Body_IsValid(pw->striker_body)) physics_internal_make_striker_body(pw, snap->striker.position);
        b2Body_SetTransform(pw->striker_body,
            (b2Vec2){ snap->striker.position.x, snap->striker.position.y },
            (b2Rot){ .c = cosf(snap->striker.angle), .s = sinf(snap->striker.angle) });
        b2Body_SetLinearVelocity(pw->striker_body,
            (b2Vec2){ snap->striker.velocity.x, snap->striker.velocity.y });
        b2Body_SetAngularVelocity(pw->striker_body, snap->striker.angular_velocity);
        if (snap->striker.awake) {
            b2Body_SetAwake(pw->striker_body, true);
        }
    }
    pw->striker_pocketed = snap->striker_pocketed;

    // Restore pocketed tracking
    pw->pocketed_count = snap->pocketed_count;
    memcpy(pw->pocketed_ids, snap->pocketed_ids, sizeof(snap->pocketed_ids));
    memcpy(pw->pocketed_colors, snap->pocketed_colors, sizeof(snap->pocketed_colors));
    memcpy(pw->pocketed_pocket_indices, snap->pocketed_pocket_indices, sizeof(snap->pocketed_pocket_indices));

    pw->sim_time = snap->sim_time;
    pw->step_count = snap->step_count;
    pw->settle_confirm_steps = 0;
}

PhysicsWorld* physics_world_from_snapshot(const PhysicsSnapshot* snap) {
    // Create new physics world and restore from snapshot
    PhysicsWorld* pw = physics_create();
    if (!pw) return NULL;
    
    physics_snapshot_restore(pw, snap);
    /* a scratch simulation is timed from its own start: the live world's clock must not run it into the settle timeout */
    pw->sim_time = 0.0f;
    return pw;
}
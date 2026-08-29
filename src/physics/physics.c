#include <math.h>
#include "physics.h"
#include "types.h"
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

/* -----------------------------------------------------------------------------
 * PhysicsWorld Structure
 * --------------------------------------------------------------------------- */
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
    
    // Pocketed tracking
    bool piece_pocketed[MAX_PIECES];
    bool striker_pocketed;
    int pocketed_count;
    uint8_t pocketed_ids[19];
    PieceColor pocketed_colors[19];
    
    // Simulation time
    float sim_time;
    
    // Settling tracking
    uint32_t settle_confirm_steps;
};

/* Forward declarations */
static void physics_create_board_geometry(PhysicsWorld* pw);
static void physics_create_pieces(PhysicsWorld* pw);
static void physics_create_striker(PhysicsWorld* pw);
static b2ShapeDef physics_make_shape_def(float restitution, float friction);
static b2ShapeDef physics_make_sensor_shape_def(void);
static void physics_check_pocket_events(PhysicsWorld* pw);
static void physics_check_pockets(PhysicsWorld* pw);

/* -----------------------------------------------------------------------------
 * Physics Creation / Destruction
 * --------------------------------------------------------------------------- */
PhysicsWorld* physics_create(void) {
    PhysicsWorld* pw = calloc(1, sizeof(PhysicsWorld));
    if (!pw) return NULL;
    
    // Box2D world definition
    b2WorldDef world_def = b2DefaultWorldDef();
    world_def.gravity = (b2Vec2){0.0f, 0.0f};  // Top-down, no gravity
    
    pw->world_id = b2CreateWorld(&world_def);
    if (b2World_IsValid(pw->world_id) == false) {
        free(pw);
        return NULL;
    }
    
    // Create board geometry
    physics_create_board_geometry(pw);
    
    // Create pieces
    physics_create_pieces(pw);
    
    // Create striker
    physics_create_striker(pw);
    
    pw->accumulator = 0.0f;
    pw->step_count = 0;
    pw->substeps = 0;
    pw->sim_time = 0.0f;
    pw->settle_confirm_steps = 0;
    
    return pw;
}

void physics_destroy(PhysicsWorld* pw) {
    if (!pw) return;
    
    if (b2World_IsValid(pw->world_id)) {
        b2DestroyWorld(pw->world_id);
    }
    free(pw);
}

/* -----------------------------------------------------------------------------
 * Shape Definition Helpers (Box2D v3 API)
 * --------------------------------------------------------------------------- */
static b2ShapeDef physics_make_shape_def(float restitution, float friction) {
    b2ShapeDef shape_def = b2DefaultShapeDef();
    shape_def.material.restitution = restitution;
    shape_def.material.friction = friction;
    return shape_def;
}

static b2ShapeDef physics_make_sensor_shape_def(void) {
    b2ShapeDef shape_def = b2DefaultShapeDef();
    shape_def.isSensor = true;
    shape_def.enableSensorEvents = true;
    return shape_def;
}

/* -----------------------------------------------------------------------------
 * Board Geometry Creation
 * --------------------------------------------------------------------------- */
static void physics_create_board_geometry(PhysicsWorld* pw) {
    float half_side = 0.5f;
    float cushion_thick = CUSHION_THICKNESS;
    
    // Cushion shape definition - Box2D v3 uses material for restitution/friction
    b2ShapeDef cushion_shape = physics_make_shape_def(0.9f, 0.1f);
    
    // Top cushion
    b2BodyDef top_def = b2DefaultBodyDef();
    top_def.type = b2_staticBody;
    top_def.position = (b2Vec2){0.0f, half_side - cushion_thick * 0.5f};
    pw->cushion_bodies[0] = b2CreateBody(pw->world_id, &top_def);
    b2Polygon box = b2MakeBox(half_side, cushion_thick * 0.5f);
    b2CreatePolygonShape(pw->cushion_bodies[0], &cushion_shape, &box);
    
    // Bottom cushion
    b2BodyDef bottom_def = b2DefaultBodyDef();
    bottom_def.type = b2_staticBody;
    bottom_def.position = (b2Vec2){0.0f, -half_side + cushion_thick * 0.5f};
    pw->cushion_bodies[1] = b2CreateBody(pw->world_id, &bottom_def);
    b2CreatePolygonShape(pw->cushion_bodies[1], &cushion_shape, &box);
    
    // Left cushion
    b2BodyDef left_def = b2DefaultBodyDef();
    left_def.type = b2_staticBody;
    left_def.position = (b2Vec2){-half_side + cushion_thick * 0.5f, 0.0f};
    pw->cushion_bodies[2] = b2CreateBody(pw->world_id, &left_def);
    b2Polygon box_v = b2MakeBox(cushion_thick * 0.5f, half_side);
    b2CreatePolygonShape(pw->cushion_bodies[2], &cushion_shape, &box_v);
    
    // Right cushion
    b2BodyDef right_def = b2DefaultBodyDef();
    right_def.type = b2_staticBody;
    right_def.position = (b2Vec2){half_side - cushion_thick * 0.5f, 0.0f};
    pw->cushion_bodies[3] = b2CreateBody(pw->world_id, &right_def);
    b2CreatePolygonShape(pw->cushion_bodies[3], &cushion_shape, &box_v);
    
    // Pocket sensors (kinematic sensors at corners)
    b2BodyDef sensor_def = b2DefaultBodyDef();
    sensor_def.type = b2_kinematicBody;
    
    b2Circle circle = { .radius = POCKET_RADIUS_NORM + PIECE_RADIUS_NORM };
    
    for (int i = 0; i < 4; i++) {
        sensor_def.position = (b2Vec2){ POCKET_CENTERS[i].x, POCKET_CENTERS[i].y };
        pw->pocket_sensors[i] = b2CreateBody(pw->world_id, &sensor_def);
        b2ShapeDef sensor_shape = physics_make_sensor_shape_def();
        b2ShapeId shape_id = b2CreateCircleShape(pw->pocket_sensors[i], &sensor_shape, &circle);
        // Enable sensor events on the shape (Box2D v3 API)
        b2Shape_EnableSensorEvents(shape_id, true);
    }
}

static void physics_create_pieces(PhysicsWorld* pw) {
    b2BodyDef body_def = b2DefaultBodyDef();
    body_def.type = b2_dynamicBody;
    body_def.linearDamping = 0.0f;
    body_def.angularDamping = 0.0f;
    body_def.fixedRotation = true;
    
    // Piece shape: restitution=0.95, friction=0.1
    b2ShapeDef shape_def = physics_make_shape_def(0.95f, 0.1f);
    shape_def.density = 1.0f;
    
    b2Circle circle = { .radius = PIECE_RADIUS_NORM };
    
    for (int i = 0; i < MAX_PIECES; i++) {
        body_def.position = (b2Vec2){0, 0};
        pw->piece_bodies[i] = b2CreateBody(pw->world_id, &body_def);
        b2CreateCircleShape(pw->piece_bodies[i], &shape_def, &circle);
        pw->piece_pocketed[i] = false;
    }
}

static void physics_create_striker(PhysicsWorld* pw) {
    b2BodyDef body_def = b2DefaultBodyDef();
    body_def.type = b2_dynamicBody;
    body_def.linearDamping = 0.0f;
    body_def.angularDamping = 0.0f;
    body_def.fixedRotation = true;
    body_def.position = (b2Vec2){0, 0};
    
    // Striker shape: restitution=0.95, friction=0.1
    b2ShapeDef shape_def = physics_make_shape_def(0.95f, 0.1f);
    shape_def.density = 1.0f;
    
    b2Circle circle = { .radius = STRIKER_RADIUS_NORM };
    
    pw->striker_body = b2CreateBody(pw->world_id, &body_def);
    b2CreateCircleShape(pw->striker_body, &shape_def, &circle);
    pw->striker_pocketed = false;
}

/* -----------------------------------------------------------------------------
 * Fixed Timestep Step
 * --------------------------------------------------------------------------- */
void physics_step(PhysicsWorld* pw, float dt) {
    pw->accumulator += dt;
    pw->substeps = 0;
    
    while (pw->accumulator >= PHYSICS_DT && pw->substeps < MAX_SUBSTEPS) {
        b2World_Step(pw->world_id, PHYSICS_DT, 4);
        pw->accumulator -= PHYSICS_DT;
        pw->step_count++;
        pw->substeps++;
        pw->sim_time += PHYSICS_DT;
        
        // Apply board resistance after each substep
        physics_apply_board_resistance(pw);
        
        // Check pocket captures using Box2D v3 sensor events
        physics_check_pocket_events(pw);
    }
    
    pw->substeps = 0;
}

/* -----------------------------------------------------------------------------
 * Board Resistance (Coulomb + Viscous)
 * --------------------------------------------------------------------------- */
void physics_apply_board_resistance(PhysicsWorld* pw) {
    // Apply to all pieces
    for (int i = 0; i < MAX_PIECES; i++) {
        if (pw->piece_pocketed[i]) continue;
        
        if (!b2Body_IsValid(pw->piece_bodies[i])) continue;
        
        b2Vec2 v = b2Body_GetLinearVelocity(pw->piece_bodies[i]);
        float speed = math_sqrtf(v.x * v.x + v.y * v.y);
        
        if (speed < SETTLE_SPEED_EPS) {
            b2Body_SetLinearVelocity(pw->piece_bodies[i], (b2Vec2){0, 0});
            continue;
        }
        
        // Coulomb + Viscous deceleration
        float decel = BOARD_COULOMB + BOARD_VISCOUS * speed;
        float new_speed = speed - decel * PHYSICS_DT;
        if (new_speed < 0) new_speed = 0;
        
        float scale = new_speed / speed;
        b2Body_SetLinearVelocity(pw->piece_bodies[i], (b2Vec2){v.x * scale, v.y * scale});
    }
    
    // Apply to striker
    if (!pw->striker_pocketed && b2Body_IsValid(pw->striker_body)) {
        b2Vec2 v = b2Body_GetLinearVelocity(pw->striker_body);
        float speed = math_sqrtf(v.x * v.x + v.y * v.y);
        
        if (speed < SETTLE_SPEED_EPS) {
            b2Body_SetLinearVelocity(pw->striker_body, (b2Vec2){0, 0});
        } else {
            float decel = BOARD_COULOMB + BOARD_VISCOUS * speed;
            float new_speed = speed - decel * PHYSICS_DT;
            if (new_speed < 0) new_speed = 0;
            float scale = new_speed / speed;
            b2Body_SetLinearVelocity(pw->striker_body, (b2Vec2){v.x * scale, v.y * scale});
        }
    }
}

/* -----------------------------------------------------------------------------
 * Pocket Capture Detection via Sensor Events (Box2D v3)
 * --------------------------------------------------------------------------- */
static void physics_check_pocket_events(PhysicsWorld* pw) {
    // Get sensor events from Box2D v3
    b2SensorEvents sensor_events = b2World_GetSensorEvents(pw->world_id);
    
    // Pre-compute pocket sensor shape IDs for comparison
    b2ShapeId pocket_shape_ids[4] = {0};
    for (int p = 0; p < 4; p++) {
        if (b2Body_IsValid(pw->pocket_sensors[p])) {
            int shape_count = b2Body_GetShapeCount(pw->pocket_sensors[p]);
            if (shape_count > 0) {
                b2ShapeId shapes[1];
                b2Body_GetShapes(pw->pocket_sensors[p], shapes, 1);
                pocket_shape_ids[p] = shapes[0];
            }
        }
    }
    
    for (int i = 0; i < sensor_events.beginCount; i++) {
        b2SensorBeginTouchEvent event = sensor_events.beginEvents[i];
        
        // Find which pocket sensor triggered
        int pocket_idx = -1;
        for (int p = 0; p < 4; p++) {
            if (B2_ID_EQUALS(pocket_shape_ids[p], event.sensorShapeId)) {
                pocket_idx = p;
                break;
            }
        }
        
        if (pocket_idx < 0) continue;
        
        // Get the body that entered the sensor
        b2BodyId visitor_body = b2Shape_GetBody(event.visitorShapeId);
        
        // Check if it's a piece
        for (int p = 0; p < MAX_PIECES; p++) {
            if (pw->piece_pocketed[p]) continue;
            if (b2Body_IsValid(pw->piece_bodies[p]) && 
                B2_ID_EQUALS(pw->piece_bodies[p], visitor_body)) {
                
                // Pocket the piece immediately
                pw->piece_pocketed[p] = true;
                pw->pocketed_ids[pw->pocketed_count] = (uint8_t)p;
                
                if (p == QUEEN_ID) {
                    pw->pocketed_colors[pw->pocketed_count] = PIECE_QUEEN;
                } else if (p < 9) {
                    pw->pocketed_colors[pw->pocketed_count] = PIECE_WHITE;
                } else {
                    pw->pocketed_colors[pw->pocketed_count] = PIECE_BLACK;
                }
                pw->pocketed_count++;
                
                // Destroy the body
                b2DestroyBody(pw->piece_bodies[p]);
                pw->piece_bodies[p] = (b2BodyId){0};
                break;
            }
        }
        
        // Check if it's the striker
        if (!pw->striker_pocketed && b2Body_IsValid(pw->striker_body) &&
            B2_ID_EQUALS(pw->striker_body, visitor_body)) {
            pw->striker_pocketed = true;
            b2DestroyBody(pw->striker_body);
            pw->striker_body = (b2BodyId){0};
        }
    }
}

/* Keep distance-based check as fallback for robustness */
static void physics_check_pockets(PhysicsWorld* pw) {
    float capture_radius = POCKET_RADIUS_NORM + PIECE_RADIUS_NORM;
    float capture_radius_sq = capture_radius * capture_radius;
    
    // Check pieces
    for (int i = 0; i < MAX_PIECES; i++) {
        if (pw->piece_pocketed[i] || !b2Body_IsValid(pw->piece_bodies[i])) continue;
        
        b2Vec2 pos = b2Body_GetPosition(pw->piece_bodies[i]);
        
        for (int p = 0; p < 4; p++) {
            if (!b2Body_IsValid(pw->pocket_sensors[p])) continue;
            
            b2Vec2 pocket_pos = b2Body_GetPosition(pw->pocket_sensors[p]);
            float dx = pos.x - pocket_pos.x;
            float dy = pos.y - pocket_pos.y;
            float dist_sq = dx*dx + dy*dy;
            
            if (dist_sq < capture_radius_sq) {
                pw->piece_pocketed[i] = true;
                pw->pocketed_ids[pw->pocketed_count] = (uint8_t)i;
                
                if (i == QUEEN_ID) {
                    pw->pocketed_colors[pw->pocketed_count] = PIECE_QUEEN;
                } else if (i < 9) {
                    pw->pocketed_colors[pw->pocketed_count] = PIECE_WHITE;
                } else {
                    pw->pocketed_colors[pw->pocketed_count] = PIECE_BLACK;
                }
                pw->pocketed_count++;
                
                b2DestroyBody(pw->piece_bodies[i]);
                pw->piece_bodies[i] = (b2BodyId){0};
                break;
            }
        }
    }
    
    // Check striker
    if (!pw->striker_pocketed && b2Body_IsValid(pw->striker_body)) {
        b2Vec2 pos = b2Body_GetPosition(pw->striker_body);
        
        for (int p = 0; p < 4; p++) {
            if (!b2Body_IsValid(pw->pocket_sensors[p])) continue;
            
            b2Vec2 pocket_pos = b2Body_GetPosition(pw->pocket_sensors[p]);
            float dx = pos.x - pocket_pos.x;
            float dy = pos.y - pocket_pos.y;
            float dist_sq = dx*dx + dy*dy;
            
            if (dist_sq < capture_radius_sq) {
                pw->striker_pocketed = true;
                b2DestroyBody(pw->striker_body);
                pw->striker_body = (b2BodyId){0};
                break;
            }
        }
    }
}

void physics_collect_pocketed(PhysicsWorld* pw, ShotResult* result) {
    result->pocketed_count = (uint8_t)pw->pocketed_count;
    for (int i = 0; i < pw->pocketed_count; i++) {
        result->pocketed_ids[i] = pw->pocketed_ids[i];
        result->pocketed_colors[i] = pw->pocketed_colors[i];
    }
    result->queen_pocketed = false;
    result->striker_pocketed = pw->striker_pocketed;
    
    for (int i = 0; i < pw->pocketed_count; i++) {
        if (pw->pocketed_ids[i] == QUEEN_ID) {
            result->queen_pocketed = true;
            break;
        }
    }
    
    // Reset for next shot
    pw->pocketed_count = 0;
    pw->striker_pocketed = false;
}

void physics_get_final_positions(PhysicsWorld* pw, Vec2* positions) {
    for (int i = 0; i < MAX_PIECES; i++) {
        if (!pw->piece_pocketed[i] && b2Body_IsValid(pw->piece_bodies[i])) {
            b2Vec2 pos = b2Body_GetPosition(pw->piece_bodies[i]);
            positions[i] = (Vec2){ pos.x, pos.y };
        } else {
            positions[i] = (Vec2){ 0, 0 };
        }
    }
}

/* -----------------------------------------------------------------------------
 * Striker Placement & Shot
 * --------------------------------------------------------------------------- */
void physics_place_striker(PhysicsWorld* pw, Seat seat, Vec2 placement) {
    if (b2Body_IsValid(pw->striker_body)) {
        b2Body_SetTransform(pw->striker_body, (b2Vec2){placement.x, placement.y}, b2Rot_identity);
        b2Body_SetLinearVelocity(pw->striker_body, (b2Vec2){0, 0});
        b2Body_SetAwake(pw->striker_body, true);
    }
}

void physics_apply_shot(PhysicsWorld* pw, float aim_angle, float power) {
    const float MAX_SPEED = 5.0f;
    float speed = power * MAX_SPEED;
    
    Vec2 dir = math_vec2_from_angle(aim_angle);
    Vec2 impulse = vec2_mul(dir, speed);
    b2Body_ApplyLinearImpulseToCenter(pw->striker_body, (b2Vec2){impulse.x, impulse.y}, true);
}

/* -----------------------------------------------------------------------------
 * Settling Detection
 * speed < 1e-4 AND accel < 1e-4 for 3 consecutive steps, 30s timeout
 * --------------------------------------------------------------------------- */
bool physics_is_settled(PhysicsWorld* pw) {
    float max_speed = 0.0f;
    float max_accel = 0.0f;
    
    for (int i = 0; i < MAX_PIECES; i++) {
        if (pw->piece_pocketed[i] || !b2Body_IsValid(pw->piece_bodies[i])) continue;
        
        b2Vec2 v = b2Body_GetLinearVelocity(pw->piece_bodies[i]);
        float speed = math_sqrtf(v.x * v.x + v.y * v.y);
        if (speed > max_speed) max_speed = speed;
        
        // Approximate acceleration from velocity change
        if (speed > SETTLE_SPEED_EPS) {
            float decel = BOARD_COULOMB + BOARD_VISCOUS * speed;
            if (decel > max_accel) max_accel = decel;
        }
    }
    
    if (!pw->striker_pocketed && b2Body_IsValid(pw->striker_body)) {
        b2Vec2 v = b2Body_GetLinearVelocity(pw->striker_body);
        float speed = math_sqrtf(v.x * v.x + v.y * v.y);
        if (speed > max_speed) max_speed = speed;
        
        if (speed > SETTLE_SPEED_EPS) {
            float decel = BOARD_COULOMB + BOARD_VISCOUS * speed;
            if (decel > max_accel) max_accel = decel;
        }
    }
    
    // Check 30s timeout
    if (pw->sim_time >= SETTLE_TIMEOUT_SECONDS) {
        return true;
    }
    
    // Require both speed AND acceleration below thresholds
    if (max_speed <= SETTLE_SPEED_EPS && max_accel <= SETTLE_ACCEL_EPS) {
        pw->settle_confirm_steps++;
        return pw->settle_confirm_steps >= SETTLE_CONFIRM_STEPS;
    } else {
        pw->settle_confirm_steps = 0;
        return false;
    }
}

float physics_get_sim_time(PhysicsWorld* pw) {
    return pw->sim_time;
}

/* -----------------------------------------------------------------------------
 * Snapshot (for AI scratch simulation)
 * --------------------------------------------------------------------------- */
PhysicsSnapshot* physics_snapshot(PhysicsWorld* pw) {
    return physics_snapshot_create(pw);
}

void physics_restore_snapshot(PhysicsWorld* pw, const PhysicsSnapshot* snap) {
    physics_snapshot_restore(pw, snap);
}

/* -----------------------------------------------------------------------------
 * Body Queries
 * --------------------------------------------------------------------------- */
int physics_get_body_count(PhysicsWorld* pw) {
    (void)pw;
    return MAX_PIECES + 1;
}

b2BodyId* physics_get_bodies(PhysicsWorld* pw, int* out_count) {
    static b2BodyId bodies[MAX_PIECES + 1];
    int count = 0;
    
    for (int i = 0; i < MAX_PIECES; i++) {
        if (b2Body_IsValid(pw->piece_bodies[i])) {
            bodies[count++] = pw->piece_bodies[i];
        }
    }
    
    if (b2Body_IsValid(pw->striker_body)) {
        bodies[count++] = pw->striker_body;
    }
    
    *out_count = count;
    return bodies;
}

void physics_get_positions(const PhysicsWorld* pw, Vec2* positions) {
    for (int i = 0; i < MAX_PIECES; i++) {
        if (!pw->piece_pocketed[i] && b2Body_IsValid(pw->piece_bodies[i])) {
            b2Vec2 pos = b2Body_GetPosition(pw->piece_bodies[i]);
            positions[i] = (Vec2){ pos.x, pos.y };
        } else {
            positions[i] = (Vec2){ 0, 0 };
        }
    }
}

void physics_get_striker_position(const PhysicsWorld* pw, Vec2* pos) {
    if (!pw->striker_pocketed && b2Body_IsValid(pw->striker_body)) {
        b2Vec2 p = b2Body_GetPosition(pw->striker_body);
        *pos = (Vec2){ p.x, p.y };
    } else {
        *pos = (Vec2){ 0, 0 };
    }
}

void physics_sync_from_board(PhysicsWorld* pw, const BoardState* board, Seat striker_seat) {
    // Sync all pieces
    for (int i = 0; i < MAX_PIECES; i++) {
        if (board->pieces[i].on_board && !board->pieces[i].pocketed) {
            if (b2Body_IsValid(pw->piece_bodies[i])) {
                b2Body_SetTransform(pw->piece_bodies[i], 
                    (b2Vec2){ board->pieces[i].position.x, board->pieces[i].position.y },
                    b2Rot_identity);
                b2Body_SetLinearVelocity(pw->piece_bodies[i], (b2Vec2){ 0, 0 });
                b2Body_SetAwake(pw->piece_bodies[i], true);
            }
            pw->piece_pocketed[i] = false;
        } else {
            pw->piece_pocketed[i] = true;
            if (b2Body_IsValid(pw->piece_bodies[i])) {
                b2DestroyBody(pw->piece_bodies[i]);
                pw->piece_bodies[i] = (b2BodyId){0};
            }
        }
    }
    
    // Sync striker
    if (!board->striker.pocketed) {
        if (b2Body_IsValid(pw->striker_body)) {
            b2Body_SetTransform(pw->striker_body,
                (b2Vec2){ board->striker.position.x, board->striker.position.y },
                b2Rot_identity);
            b2Body_SetLinearVelocity(pw->striker_body, (b2Vec2){ 0, 0 });
            b2Body_SetAwake(pw->striker_body, true);
        }
        pw->striker_pocketed = false;
    } else {
        pw->striker_pocketed = true;
        if (b2Body_IsValid(pw->striker_body)) {
            b2DestroyBody(pw->striker_body);
            pw->striker_body = (b2BodyId){0};
        }
    }
    
    // Reset pocketed tracking
    pw->pocketed_count = 0;
    pw->sim_time = 0.0f;
    pw->settle_confirm_steps = 0;
}
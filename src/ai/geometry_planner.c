#include "geometry_planner.h"
#include "board.h"
#include "physics/physics.h"
#include <math.h>
#include <stdlib.h>
#ifdef __MINGW32__
extern float sqrtf(float);
extern float atan2f(float, float);
#endif

/* -----------------------------------------------------------------------------
 * Geometric shot planner
 *
 * Deterministic shot construction from plane geometry (ghost-ball aiming, wall mirror
 * images) in board units. Shot families, in priority order:
 *   1. DIRECT / CUT  - the striker drives an own piece straight at a pocket
 *   2. BANK          - striker rebounds off a cushion first, or the piece rebounds off a
 *                      cushion into the pocket ("double")
 *   3. BREAK         - advance shots at the nearest own piece / the rack when nothing scores
 * Power is estimated from the board deceleration (constant-deceleration kinematics); the
 * arena controller then verifies every candidate by scratch physics simulation.
 * Strategy inspired by the public description of mehtanihar/carrom-agent (direct, cut,
 * rebound and double shots); no code from that project is used.
 * --------------------------------------------------------------------------- */

#define GP_CLEAR_EPS        0.004f
#define GP_MIN_COS_CUT      0.34f     /* cut angles up to about 70 degrees */
#define GP_DIRECT_COS       0.966f    /* within 15 degrees counts as a straight shot */
#define GP_ARRIVE_SPEED     0.6f      /* speed the piece should still have when it reaches the pocket (u/s) */
#define GP_TRANSFER         1.15f     /* piece speed after impact / striker speed along the line of centres */
#define GP_BANK_LOSS        0.6f      /* fraction of energy kept through a cushion bounce */
#define GP_PLACEMENT_SAMPLES 15

typedef struct { float x, y; } GpVec;

static GpVec gp_v(Vec2 a) { return (GpVec){ a.x, a.y }; }
static Vec2 gp_out(GpVec a) { return (Vec2){ a.x, a.y }; }
static GpVec gp_sub(GpVec a, GpVec b) { return (GpVec){ a.x - b.x, a.y - b.y }; }
static GpVec gp_add(GpVec a, GpVec b) { return (GpVec){ a.x + b.x, a.y + b.y }; }
static GpVec gp_mul(GpVec a, float s) { return (GpVec){ a.x * s, a.y * s }; }
static float gp_dot(GpVec a, GpVec b) { return a.x * b.x + a.y * b.y; }
static float gp_len(GpVec a) { return sqrtf(a.x * a.x + a.y * a.y); }
static GpVec gp_norm(GpVec a) {
    float l = gp_len(a);
    return (l > 1e-9f) ? gp_mul(a, 1.0f / l) : (GpVec){ 1.0f, 0.0f };
}

static float gp_seg_dist(GpVec p, GpVec a, GpVec b) {
    GpVec ab = gp_sub(b, a);
    float l2 = gp_dot(ab, ab);
    float t = (l2 > 1e-12f) ? gp_dot(gp_sub(p, a), ab) / l2 : 0.0f;
    if (t < 0.0f) t = 0.0f;
    if (t > 1.0f) t = 1.0f;
    return gp_len(gp_sub(p, gp_add(a, gp_mul(ab, t))));
}

Vec2 geometry_ghost_ball(Vec2 target, Vec2 pocket) {
    GpVec dir = gp_norm(gp_sub(gp_v(pocket), gp_v(target)));
    return gp_out(gp_sub(gp_v(target), gp_mul(dir, PIECE_RADIUS_NORM + STRIKER_RADIUS_NORM + 0.001f)));
}

bool geometry_segment_clear(Vec2 a, Vec2 b, float clearance, const Vec2* obstacles, const int* ids, int n, int skip_id) {
    for (int i = 0; i < n; i++) {
        if (ids[i] == skip_id) continue;
        if (gp_seg_dist(gp_v(obstacles[i]), gp_v(a), gp_v(b)) < clearance) return false;
    }
    return true;
}

float geometry_power_for(float object_dist, float striker_dist, float cos_cut, float object_keep, float striker_keep) {
    float decel = BOARD_COULOMB;
    float v_obj = sqrtf(2.0f * decel * object_dist / object_keep) + GP_ARRIVE_SPEED;
    float v_contact = v_obj / (GP_TRANSFER * cos_cut);
    float v_launch = sqrtf(v_contact * v_contact + 2.0f * decel * striker_dist / striker_keep);
    return v_launch / STRIKER_MAX_SPEED;
}

/* Box2D contact friction drags the struck piece slightly toward the striker's direction of travel
 * (measured: about 4.5 deg at a 15 deg cut, 6.4 at 30, 9 at 60). Aim the contact normal that much
 * further away from the striker's path so the piece leaves along the pocket line. */
#define GP_THROW_MAX 0.157f   /* 9 degrees at a 60 degree cut */

static GpVec gp_compensated_ghost(GpVec origin, GpVec target, GpVec dop) {
    const float reach = PIECE_RADIUS_NORM + STRIKER_RADIUS_NORM + 0.001f;
    GpVec n = dop;
    GpVec g = gp_sub(target, gp_mul(n, reach));
    for (int k = 0; k < 3; k++) {
        GpVec dir = gp_norm(gp_sub(g, origin));
        float c = gp_dot(dir, dop);
        if (c > 1.0f) c = 1.0f;
        if (c < -1.0f) c = -1.0f;
        float phi = acosf(c);
        float side = (dir.x * dop.y - dir.y * dop.x) >= 0.0f ? 1.0f : -1.0f;
        float ang = side * GP_THROW_MAX * sqrtf(phi / 1.047f);
        float ca = cosf(ang), sa = sinf(ang);
        n = (GpVec){ dop.x * ca - dop.y * sa, dop.x * sa + dop.y * ca };
        g = gp_sub(target, gp_mul(n, reach));
    }
    return g;
}

/* Mirror a point across cushion wall w (0:+x 1:-x 2:+y 3:-y) whose line is at distance `bound` from the centre */
static GpVec gp_mirror(GpVec p, int w, float bound) {
    switch (w) {
        case 0: return (GpVec){ 2.0f * bound - p.x, p.y };
        case 1: return (GpVec){ -2.0f * bound - p.x, p.y };
        case 2: return (GpVec){ p.x, 2.0f * bound - p.y };
        default: return (GpVec){ p.x, -2.0f * bound - p.y };
    }
}

/* Intersection of segment a->b with wall w; returns false when it does not cross inside the wall extent */
static bool gp_wall_hit(GpVec a, GpVec b, int w, float bound, float extent, GpVec* hit) {
    bool vertical = (w == 0 || w == 1);
    float wall = (w == 0 || w == 2) ? bound : -bound;
    float da = vertical ? a.x : a.y, db = vertical ? b.x : b.y;
    if ((da - wall) * (db - wall) > 0.0f) return false;
    float denom = db - da;
    if (fabsf(denom) < 1e-9f) return false;
    float t = (wall - da) / denom;
    GpVec p = gp_add(a, gp_mul(gp_sub(b, a), t));
    float other = vertical ? p.y : p.x;
    if (fabsf(other) > extent) return false;
    *hit = p;
    return true;
}

static void gp_emit(GeomShot* out, int* count, int max_out, GeomShot s) {
    if (*count < max_out) out[(*count)++] = s;
}

static int gp_cmp(const void* a, const void* b) {
    float sa = ((const GeomShot*)a)->geom_score, sb = ((const GeomShot*)b)->geom_score;
    return (sa < sb) - (sa > sb);
}

int geometry_plan_shots(const BoardState* board, Seat seat, GeomShot* out, int max_out) {
    if (!board || !out || max_out <= 0) return 0;

    const float rp = PIECE_RADIUS_NORM, rs = STRIKER_RADIUS_NORM;
    const float striker_bound = 0.5f - CUSHION_THICKNESS - rs;
    const float piece_bound = 0.5f - CUSHION_THICKNESS - rp;
    Team team = (seat == SEAT_NORTH || seat == SEAT_SOUTH) ? TEAM_WHITE : TEAM_BLACK;
    PieceColor own = (team == TEAM_WHITE) ? PIECE_WHITE : PIECE_BLACK;

    /* obstacles: every piece still on the board */
    Vec2 obs[MAX_PIECES];
    int obs_id[MAX_PIECES];
    int nobs = 0;
    for (int i = 0; i < MAX_PIECES; i++) {
        if (board->pieces[i].on_board && !board->pieces[i].pocketed) {
            obs[nobs] = board->pieces[i].position;
            obs_id[nobs++] = i;
        }
    }

    /* legal striker placements along the seat's baseline that do not overlap a piece */
    Vec2 placements[GP_PLACEMENT_SAMPLES];
    int np = 0;
    for (int k = 0; k < GP_PLACEMENT_SAMPLES; k++) {
        float t = (float)k / (float)(GP_PLACEMENT_SAMPLES - 1);
        float off = -BASELINE_MAX_OFFSET + t * 2.0f * BASELINE_MAX_OFFSET;
        Vec2 pos;
        switch (seat) {
            case SEAT_NORTH: pos = (Vec2){ off, BASELINE_Y_NORTH }; break;
            case SEAT_SOUTH: pos = (Vec2){ off, BASELINE_Y_SOUTH }; break;
            case SEAT_EAST:  pos = (Vec2){ BASELINE_X_EAST, off }; break;
            default:         pos = (Vec2){ BASELINE_X_WEST, off }; break;
        }
        if (!board_is_legal_placement(seat, pos)) continue;
        bool free_spot = true;
        for (int i = 0; i < nobs; i++) {
            if (gp_len(gp_sub(gp_v(obs[i]), gp_v(pos))) < rs + rp + 0.002f) { free_spot = false; break; }
        }
        if (free_spot) placements[np++] = pos;
    }

    /* targets: own colour first, the queen too */
    int targets[MAX_PIECES];
    int nt = 0;
    for (int i = 0; i < MAX_PIECES; i++) {
        const PieceState* pc = &board->pieces[i];
        if (!pc->on_board || pc->pocketed) continue;
        if (pc->color == own || pc->color == PIECE_QUEEN) targets[nt++] = i;
    }

    GeomShot* all = out;
    int count = 0;
    int cap = max_out;

    for (int ip = 0; ip < np; ip++) {
        GpVec S = gp_v(placements[ip]);
        for (int it = 0; it < nt; it++) {
            int tid = targets[it];
            GpVec T = gp_v(board->pieces[tid].position);
            float queen_pen = (board->pieces[tid].color == PIECE_QUEEN) ? 15.0f : 0.0f;

            for (int pk = 0; pk < 4; pk++) {
                GpVec P = gp_v(POCKET_CENTERS[pk]);

                /* ---- DIRECT / CUT ---- */
                {
                    GpVec dop = gp_norm(gp_sub(P, T));
                    float d_o = gp_len(gp_sub(P, T));
                    GpVec G = gp_compensated_ghost(S, T, dop);
                    GpVec v = gp_sub(G, S);
                    float d_s = gp_len(v);
                    if (d_s > 1e-4f && fabsf(G.x) <= striker_bound && fabsf(G.y) <= striker_bound &&
                        geometry_segment_clear(gp_out(T), gp_out(P), 2.0f * rp + GP_CLEAR_EPS, obs, obs_id, nobs, tid)) {
                        GpVec dir = gp_mul(v, 1.0f / d_s);
                        float c = gp_dot(dir, dop);
                        if (c >= GP_MIN_COS_CUT &&
                            geometry_segment_clear(gp_out(S), gp_out(G), rs + rp + GP_CLEAR_EPS, obs, obs_id, nobs, tid)) {
                            float pw = geometry_power_for(d_o, d_s, c, 1.0f, 1.0f);
                            if (pw <= 1.08f) {
                                bool straight = c >= GP_DIRECT_COS;
                                GeomShot s = {
                                    .placement = gp_out(S), .aim_angle = atan2f(dir.y, dir.x),
                                    .power = pw > 1.0f ? 1.0f : pw,
                                    .tactic = straight ? TACTIC_DIRECT : TACTIC_CUT,
                                    .geom_score = (straight ? 100.0f : 85.0f) - 40.0f * (1.0f - c) - 25.0f * (d_s + d_o) - queen_pen,
                                    .target_id = tid, .pocket_index = pk
                                };
                                gp_emit(all, &count, cap, s);
                            }
                        }
                    }
                }

                /* ---- BANK, striker rebounds off a cushion before the piece ---- */
                if (geometry_segment_clear(gp_out(T), gp_out(P), 2.0f * rp + GP_CLEAR_EPS, obs, obs_id, nobs, tid)) {
                    GpVec dop = gp_norm(gp_sub(P, T));
                    float d_o = gp_len(gp_sub(P, T));
                    GpVec G = gp_sub(T, gp_mul(dop, rp + rs + 0.001f));
                    if (fabsf(G.x) <= striker_bound && fabsf(G.y) <= striker_bound) {
                        for (int w = 0; w < 4; w++) {
                            GpVec Gm = gp_mirror(G, w, striker_bound);
                            GpVec W;
                            if (!gp_wall_hit(S, Gm, w, striker_bound, striker_bound, &W)) continue;
                            float d1 = gp_len(gp_sub(W, S)), d2 = gp_len(gp_sub(G, W));
                            if (d1 < 0.03f || d2 < 1e-4f) continue;
                            GpVec dir2 = gp_mul(gp_sub(G, W), 1.0f / d2);
                            float c = gp_dot(dir2, dop);
                            if (c < GP_MIN_COS_CUT) continue;
                            float clr = rs + rp + GP_CLEAR_EPS;
                            if (!geometry_segment_clear(gp_out(S), gp_out(W), clr, obs, obs_id, nobs, tid)) continue;
                            if (!geometry_segment_clear(gp_out(W), gp_out(G), clr, obs, obs_id, nobs, tid)) continue;
                            float pw = geometry_power_for(d_o, d1 + d2, c, 1.0f, GP_BANK_LOSS);
                            if (pw > 1.0f) continue;
                            GpVec dir1 = gp_norm(gp_sub(W, S));
                            GeomShot s = {
                                .placement = gp_out(S), .aim_angle = atan2f(dir1.y, dir1.x), .power = pw,
                                .tactic = TACTIC_BANK,
                                .geom_score = 60.0f - 40.0f * (1.0f - c) - 25.0f * (d1 + d2 + d_o) - queen_pen,
                                .target_id = tid, .pocket_index = pk
                            };
                            gp_emit(all, &count, cap, s);
                        }
                    }
                }

                /* ---- BANK, "double": the piece rebounds off a cushion into the pocket ---- */
                for (int w = 0; w < 4; w++) {
                    GpVec Pm = gp_mirror(P, w, piece_bound);
                    GpVec W;
                    if (!gp_wall_hit(T, Pm, w, piece_bound, piece_bound, &W)) continue;
                    float d_a = gp_len(gp_sub(W, T)), d_b = gp_len(gp_sub(P, W));
                    if (d_a < 0.03f || d_b < 0.03f) continue;
                    float clr = 2.0f * rp + GP_CLEAR_EPS;
                    if (!geometry_segment_clear(gp_out(T), gp_out(W), clr, obs, obs_id, nobs, tid)) continue;
                    if (!geometry_segment_clear(gp_out(W), gp_out(P), clr, obs, obs_id, nobs, tid)) continue;
                    GpVec dop = gp_norm(gp_sub(Pm, T));
                    GpVec G = gp_compensated_ghost(S, T, dop);
                    if (fabsf(G.x) > striker_bound || fabsf(G.y) > striker_bound) continue;
                    GpVec v = gp_sub(G, S);
                    float d_s = gp_len(v);
                    if (d_s < 1e-4f) continue;
                    GpVec dir = gp_mul(v, 1.0f / d_s);
                    float c = gp_dot(dir, dop);
                    if (c < GP_MIN_COS_CUT) continue;
                    if (!geometry_segment_clear(gp_out(S), gp_out(G), rs + rp + GP_CLEAR_EPS, obs, obs_id, nobs, tid)) continue;
                    float pw = geometry_power_for(d_a + d_b, d_s, c, GP_BANK_LOSS, 1.0f);
                    if (pw > 1.0f) continue;
                    GeomShot s = {
                        .placement = gp_out(S), .aim_angle = atan2f(dir.y, dir.x), .power = pw,
                        .tactic = TACTIC_BANK,
                        .geom_score = 50.0f - 40.0f * (1.0f - c) - 25.0f * (d_s + d_a + d_b) - queen_pen,
                        .target_id = tid, .pocket_index = pk
                    };
                    gp_emit(all, &count, cap, s);
                }
            }
        }
    }

    /* keep the best geometric shots, leaving room for a few advance shots */
    int reserve = (max_out >= 8) ? 4 : 0;
    qsort(all, (size_t)count, sizeof(GeomShot), gp_cmp);
    if (count > max_out - reserve) count = max_out - reserve;

    /* ---- advance shots (BREAK): hit the nearest own piece, or the rack, from a few spots ---- */
    if (np > 0 && count < max_out) {
        int best = -1;
        float best_d = 1e9f;
        GpVec mid = gp_v(placements[np / 2]);
        for (int it = 0; it < nt; it++) {
            float d = gp_len(gp_sub(gp_v(board->pieces[targets[it]].position), mid));
            if (d < best_d) { best_d = d; best = targets[it]; }
        }
        GpVec aim_at = (best >= 0) ? gp_v(board->pieces[best].position) : (GpVec){ 0.0f, 0.0f };
        for (int k = 0; k < np && count < max_out && k < 5; k++) {
            GpVec S = gp_v(placements[(k * np) / 5]);
            GpVec d = gp_sub(aim_at, S);
            GeomShot s = {
                .placement = gp_out(S), .aim_angle = atan2f(d.y, d.x), .power = 0.55f,
                .tactic = TACTIC_BREAK, .geom_score = 5.0f - (float)k,
                .target_id = best, .pocket_index = -1
            };
            out[count++] = s;
        }
    }
    return count;
}

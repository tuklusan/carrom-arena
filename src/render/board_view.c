#include <math.h>
#include "board_view.h"
#include "piece_draw.h"
#include "striker_draw.h"
#include "effects.h"
#include "theme.h"
#include "renderer.h"
#include "common/types.h"
#include "common/vecmath.h"
#include "physics/physics.h"
#define __USE_MINGW_ANSI_STDIO 1
#include <raylib.h>

/* Inline math functions to avoid implicit declaration issues */
static inline float my_fmodf(float x, float y) {
    return x - y * (float)((int)(x / y));
}
static inline float my_fmaxf(float a, float b) {
    return (a > b) ? a : b;
}

/* Shared flash alpha computation for syncing figure and striker */
static inline float compute_flash_alpha(double wall_time) {
    // alpha = 0.4 + 0.6 * (0.5 + 0.5 * sin(2π * t)) where t = wall time in seconds, ~1Hz
    float t = (float)wall_time;
    return 0.4f + 0.6f * (0.5f + 0.5f * sinf(t * 2.0f * M_PI));
}

/* Colors */
#define COLOR_BOARD (Color){ 10, 20, 40, 120 }         // tinted glass: the backdrop shows through, a shade darker (visual only)
#define COLOR_CUSHION (Color){ 100, 70, 40, 255 }     // Darker brown
#define COLOR_POCKET (Color){ 0, 0, 0, 255 }          // Black
#define COLOR_WHITE_PIECE (Color){ 240, 240, 240, 255 }
#define COLOR_BLACK_PIECE (Color){ 62, 64, 74, 255 }       /* charcoal, with a silver rim: readable on the dark glass */
#define COLOR_QUEEN (Color){ 220, 30, 30, 255 }       // Red
#define COLOR_STRIKER (Color){ 255, 215, 0, 255 }     // Gold
#define COLOR_LINE (Color){ 255, 255, 255, 100 }      // White translucent
#define COLOR_WHITE_COIN_OUTLINE (Color){ 50, 50, 50, 230 }   // thin dark rim: light coins look bigger than dark ones otherwise
#define COLOR_BLACK_COIN_RIM (Color){ 200, 205, 215, 255 }
static Color coin_outline_color(PieceColor c) { return c == PIECE_WHITE ? COLOR_WHITE_COIN_OUTLINE : (c == PIECE_BLACK ? COLOR_BLACK_COIN_RIM : COLOR_LINE); }
/* the rim: 1 px for the light coins, 2 px for the dark ones */
static void draw_coin_rim(Vec2 screen, float r, PieceColor c) {
    Color k = coin_outline_color(c);
    DrawCircleLines((int)screen.x, (int)screen.y, r, k);
    if (c == PIECE_BLACK) DrawCircleLines((int)screen.x, (int)screen.y, r - 1.0f, k);
}
#define COLOR_BASELINE (Color){ 100, 255, 100, 150 }  // Green translucent (muted for baseline)
#define COLOR_BASELINE_MUTED (Color){ 100, 255, 100, 76 }  // 30% alpha of team color

/* Team colors for figure drawing - per spec */
#define COLOR_TEAM_WHITE_FILL (Color){ 240, 240, 220, 255 }   // Light silhouette
#define COLOR_TEAM_WHITE_OUTLINE (Color){ 60, 60, 60, 255 }   // Dark outline
#define COLOR_TEAM_BLACK_FILL (Color){ 40, 40, 40, 255 }      // Dark silhouette
#define COLOR_TEAM_BLACK_OUTLINE (Color){ 200, 200, 200, 255 } // Light outline
#define COLOR_TURN_HIGHLIGHT (Color){ 255, 215, 0, 255 }      // Gold highlight for current turn

/* Aim preview line colors */
#define COLOR_AIM_PREVIEW_LINE (Color){ 214, 160, 70, 235 }    // Subdued amber (distinct from the striker's brighter gold)
#define COLOR_AIM_PREVIEW_OUTLINE (Color){ 0, 0, 0, 255 }     // Black outline
#define COLOR_AIM_PREVIEW_ARROW (Color){ 214, 160, 70, 235 }   // Subdued amber arrowhead

static void draw_tri_any_winding(Vector2 a, Vector2 b, Vector2 c, Color col);

/* -----------------------------------------------------------------------------
 * Smooth visual state: nothing on screen teleports. The striker and each player figure keep a
 * tracked position and glide toward wherever they should be at VISUAL_SLIDE_SPEED (board units
 * per wall-clock second); during a shot the striker simply follows the physics body.
 * --------------------------------------------------------------------------- */
#define VISUAL_SLIDE_SPEED 2.0f

typedef struct {
    bool striker_valid;
    Vec2 striker;
    float fig[4];
    bool fig_valid;
    bool was_gone;      /* the striker fell into a pocket during the last shot */
    float appear;       /* 0..1 fade-in of a fresh striker handed to the next player */
    float reach[4];           /* 0..1 per seat: how far into the "moved to the shot line and reaching" pose */
    Vec2 aim_pose_pos[4];     /* where that seat stands at reach=1: on ITS OWN fixed outside-the-board line, never on the board */
    float aim_pose_angle[4];  /* which way it faces at reach=1 */
    int aim_pose_side[4];     /* 0 forward, 1 left, 2 right: which pair of fingers flicks the strike, see draw_human_figure */
    Vec2 aim_pose_striker[4]; /* the striker's own RESTING position while this seat is planning/taking its shot - frozen the
                               * moment reach starts, so the reaching arm's length keeps targeting where the striker WAS,
                               * never where it flies to after being struck (see draw_human_figure) */
    float arm_spin[4];        /* the right arm's spin angle, held here the instant it starts telescoping so it can ease to 0 smoothly */
    float arm_side[4];         /* +1/-1: which arm reaches, LATCHED the same moment as aim_pose_* below and for the same
                                * reason - recomputing it fresh every frame let it flip mid-reach for a near-dead-straight
                                * shot (the operator: "both arms are ending up on the same side or the striking arm is
                                * suddenly swapping...in a jerky weird flipping"); see draw_human_figure. 0 = undecided. */
    bool aim_preview_active[4]; /* was this seat's aim preview running LAST frame - the true "a new shot just started"
                                 * signal the aim_pose_* latch now uses instead of `reach < 0.002f` (2026-09-28, the
                                 * operator, on a west robot mid-reach: "the...extended arm does not match the striker's
                                 * vector", and an east robot that "never recovered its right arm"). An extra turn (a
                                 * pocketed coin) can hand the SAME seat two shots back to back faster than the ~0.4s
                                 * withdrawal takes to finish, so `reach` may never dip back under that threshold
                                 * between them - the old latch then never re-fired, and the robot kept reaching for a
                                 * target frozen from the shot before last for the ENTIRE new shot. This flag catches
                                 * the actual moment a new aim preview begins for a seat, regardless of what `reach`
                                 * still happens to be. */
    float reach_peak_seen[4];   /* the running maximum of `reach` since this seat last returned fully to rest - how
                                 * draw_human_figure tells "still extending, or holding at full reach" apart from
                                 * "now genuinely withdrawing", instead of a fragile frame-to-frame reach comparison
                                 * that would misfire during the flat hold right before a shot fires (2026-09-28, the
                                 * operator's canonical rule: both arms must fully collapse to their normal position
                                 * before idle-waving resumes). Reset to 0 once the seat is fully back at rest. */
    bool was_shrinking[4];      /* was `reach` genuinely below its own peak LAST frame - the edge this is latched on */
    float withdrawal_reach_ang[4]; /* the reaching arm's own angle, captured the instant real withdrawal begins */
    float withdrawal_idle_ang[4];  /* the OTHER (flapping) arm's angle, captured at that same instant */
    float withdrawal_peak_reach[4]; /* `reach` at that same instant, so the blend back to neutral is correctly
                                     * normalised even if the peak wasn't exactly 1.0 */
} VisualState;

static VisualState g_vis = { .appear = 1.0f };
static float lerp_angle_shortest(float a, float b, float t);   /* used by draw_human_figure, defined later */

/* A rectangle in the robot's own frame: u runs from the head toward the board, v to the robot's right. */
typedef struct { Vec2 o, back, right; } RobotFrame;

static Vector2 robot_pt(const RobotFrame* f, float u, float v) {
    return (Vector2){ f->o.x + f->back.x * u + f->right.x * v, f->o.y + f->back.y * u + f->right.y * v };
}

static void robot_box(const RobotFrame* f, float u0, float u1, float v0, float v1, Color fill, Color line) {
    Vector2 a = robot_pt(f, u0, v0), b = robot_pt(f, u0, v1), c = robot_pt(f, u1, v1), d = robot_pt(f, u1, v0);
    draw_tri_any_winding(a, b, c, fill);
    draw_tri_any_winding(a, c, d, fill);
    DrawLineEx(a, b, 1.5f, line);
    DrawLineEx(b, c, 1.5f, line);
    DrawLineEx(c, d, 1.5f, line);
    DrawLineEx(d, a, 1.5f, line);
}

/* Like robot_box, but rotated by `ang` about the pivot (pu, pv) in the robot frame (used for flapping arms). */
static void robot_rbox(const RobotFrame* f, float pu, float pv, float ang, float u0, float u1, float v0, float v1, Color fill, Color line) {
    float c = cosf(ang), sn = sinf(ang);
    float us[4] = { u0, u0, u1, u1 }, vs[4] = { v0, v1, v1, v0 };
    Vector2 p[4];
    for (int i = 0; i < 4; i++) {
        float du = us[i] - pu, dv = vs[i] - pv;
        p[i] = robot_pt(f, pu + du * c - dv * sn, pv + du * sn + dv * c);
    }
    draw_tri_any_winding(p[0], p[1], p[2], fill);
    draw_tri_any_winding(p[0], p[2], p[3], fill);
    for (int i = 0; i < 4; i++) DrawLineEx(p[i], p[(i + 1) % 4], 1.5f, line);
}

/* Idle animation for a waiting robot: every seat gets its own incommensurate frequencies and phases, so no two match. */
static float idle_wave(int seat, int k, float t) {
    float fa = 0.9f + 0.37f * (float)seat + 0.23f * (float)k;
    float fb = 0.5f + 0.29f * (float)((seat * 3 + k) % 5);
    float pa = 1.7f * (float)(seat + 1) + 2.3f * (float)k, pb = 0.9f * (float)(seat + 2) + 1.1f * (float)k;
    return 0.6f * sinf(t * fa + pa) + 0.4f * sinf(t * fb + pb);   /* -1..1, irregular */
}

/* The robots belong to the PAIRS, not to a coin colour: north/south are always red, east/west always blue. (The `team`
 * argument keeps the old name: TEAM_WHITE draws red, TEAM_BLACK blue.) */
/* A robot player, seen from above: antenna and head at the seat, arms, shoulders and a chest panel reaching toward the board.
 * `angle` is the direction pointing away from the board (as the old figures used it), so the body extends the other way. */
static void draw_human_figure(Viewport vp, const Layout* L, Vec2 world_pos, float angle, Team team, bool is_current_turn, float alpha, int seat, float t, float reach, Vec2 striker_world, int strike_side, float arm_spin, Vec2 final_pos, float final_angle_param) {
    Vec2 screen = math_world_to_screen(vp, world_pos);
    float fref = (float)L->board_size * FIG_SCALE;
    float hr = fref / 25.0f;                       /* head half-size */

    Color base  = (team == TEAM_WHITE) ? THEME_RED : THEME_BLUE;
    Color light = (team == TEAM_WHITE) ? THEME_RED_LIGHT : THEME_BLUE_LIGHT;
    Color dark  = (team == TEAM_WHITE) ? THEME_RED_DARK : THEME_BLUE_DARK;
    Color line  = (Color){ 20, 20, 28, 255 };
    Color steel = (Color){ 150, 156, 168, 255 };
    Color eye   = (Color){ 255, 236, 110, 255 };
    Color* all[] = { &base, &light, &dark, &line, &steel, &eye };
    for (size_t i = 0; i < sizeof(all) / sizeof(all[0]); i++) all[i]->a = (unsigned char)(all[i]->a * alpha);

    Vec2 away = { cosf(angle), sinf(angle) };      /* pointing away from the board */
    RobotFrame f = { screen, { -away.x, -away.y }, { away.y, -away.x } };

    float gap = 2.0f;
    float sh0 = hr + gap;                          /* shoulders start */
    float sh1 = sh0 + 0.75f * hr;
    float torso_end = sh0 + fref / 12.0f;          /* same reach as the old figure */

    /* arms hang from the shoulders, with square hands; a waiting robot flaps each arm on its own, swinging about the shoulder.
     *
     * ROOT-CAUSE FIX (2026-09-27, third pass): a deep per-frame trace (shoulder pivot vs. the arm's own rendered near
     * corner, logged for every frame of every reach) showed the gap between them growing CONTINUOUSLY with `reach` -
     * not a transient during rotation, but on every single strike, up to 300px at full extension, on all four seats.
     * Root cause: the reaching arm was an axis-aligned box in body (u,v) space whose v0/v1 (BOTH corners at the near,
     * shoulder-side edge AND the far, hand-side edge share the same v-range) were interpolated together toward the
     * striker's v-offset. That SLIDES the whole box sideways as reach grows - including the near edge, which is
     * supposed to stay AT the shoulder - so the near edge drifts away from the true pivot in direct proportion to
     * reach. Rotating that same box by `flap_r` (which eases to 0) does not fix this: at flap_r = 0 the "rotation" is
     * the identity, so the drifted box is drawn exactly where the drifted numbers put it, with nothing anchoring it
     * back to the shoulder.
     *
     * Correct model: the arm is a RIGID shape - fixed length and width in its own local axes - that only ROTATES about
     * the true, fixed shoulder pivot (never moves sideways) and TELESCOPES (extends) along its own local axis as reach
     * grows. Its near (shoulder) corner sits at zero offset from the pivot in u and a small, CONSTANT offset in v, so
     * after any rotation it stays within that same small, bounded distance of the pivot - it can never drift off
     * arbitrarily far the way a laterally-shifting box could. The one rotation angle carries both jobs the old
     * `flap_r` had (easing the spin to a stop) and the new aiming job (turning to point at the striker), blended
     * together as `reach` goes 0 to 1, so the arm visibly swings from its spin into pointing at the striker as it
     * extends - one continuous, physically coherent motion. */
    float arm_v = 1.3f * hr;
    float pu = sh0 + 0.1f * hr;

    /* Where the striker sits in THIS robot's own (u,v) terms, recomputed fresh every frame from the CURRENT, live body
     * frame `f` - the same frame everything else about the arm is drawn in, so the target can never fall out of step
     * with the frame used to draw it (an earlier fix's mistake). */
    Vec2 s_screen = math_world_to_screen(vp, striker_world);
    float sdx = s_screen.x - screen.x, sdy = s_screen.y - screen.y;
    float u_to_striker = sdx * f.back.x + sdy * f.back.y;
    float v_to_striker = sdx * f.right.x + sdy * f.right.y;
    float striker_r = math_world_to_screen_dist(vp, STRIKER_RADIUS_NORM);

    /* Which arm reaches: whichever SIDE the striker is actually on, so the arm never has to cross in front of the body
     * (the operator: "have the robots use...the left arm if...easier to get to the striker"). Decided from the body's
     * FINAL settled pose (`final_pos`/`final_angle_param`, already computed once per shot and held fixed) rather than
     * the live, still-moving one, so the choice cannot flicker mid-turn as the body is still sliding into place. */
    Vec2 final_screen = math_world_to_screen(vp, final_pos);
    Vec2 final_away = { cosf(final_angle_param), sinf(final_angle_param) };
    RobotFrame final_f = { final_screen, { -final_away.x, -final_away.y }, { final_away.y, -final_away.x } };
    float ffdx = s_screen.x - final_screen.x, ffdy = s_screen.y - final_screen.y;
    float final_v_to_striker = ffdx * final_f.right.x + ffdy * final_f.right.y;
    /* Which arm reaches is decided once, in the outer per-seat update loop, the moment this shot's aim preview actually
     * begins (not merely whenever `reach` happens to dip near 0 - an extra turn can start a new shot before that ever
     * happens; see the loop for the full story). Just read the decision here. */
    float side_sign = g_vis.arm_side[seat];   /* +1 = right arm reaches, -1 = left arm reaches */

    /* `reaching_active` stays true for as long as `reach` itself is still meaningfully above 0, regardless of whose
     * turn the game now says it is (2026-09-28: the game hands the turn to the NEXT seat as soon as a shot resolves,
     * well before this seat's own visual withdrawal has actually finished, since that easing is purely cosmetic and
     * the game logic has no reason to wait for it). The actual flap/reach angles are worked out further down, once
     * `theta_target` is available - see there for the full withdrawal story. */
    bool reaching_active = is_current_turn || reach > 0.002f;
    float pivot_v = side_sign * (arm_v + 0.2f * hr);   /* the TRUE shoulder pivot for the reaching arm, on whichever side: (pu, pivot_v), fixed regardless of reach */

    /* THE CONTACT POINT (2026-09-28, found from the operator's own screenshot: the hand was reaching to the side of the
     * striker instead of the side diametrically opposite the shot's own arrowhead direction - "no realistic physical
     * way that hand can launch the striker in the direction of the arrowhead"). Root cause: the target the arm aimed
     * at was "whichever point on the striker's circle is closest to the ARM'S PIVOT" (found via atan2 from the pivot
     * to the striker's CENTRE) - not the point fixed by the shot itself. Those are different points whenever the
     * pivot sits off the aim line, which it always does a little (the arm attaches to the SIDE of the body, not its
     * centre) and can do a lot for a close-in shot - exactly the case in the screenshot. The correct contact point is
     * NOT "nearest the pivot": it is the FIXED point on the striker's rim, diametrically opposite the direction the
     * striker is meant to travel, however near or far the pivot happens to be from it. Since this robot's body
     * already faces along that exact direction (`f.back`/`final_f.back`, by construction, always equal the shot's own
     * aim direction), that point is simply the striker's centre offset by the striker's radius along the frame's own
     * +u axis - a pure `u` shift, `v` untouched - true for every seat and every angle, with no per-shot geometry
     * needed at all. The PIVOT no longer decides WHERE that point is; it only decides the rotation and length needed
     * to reach it. */
    float near_u = u_to_striker - striker_r * 1.05f, near_v = v_to_striker;   /* the live, correct contact point: aims the ROTATION */
    float theta_target = atan2f(near_v - pivot_v, near_u - pu);

    /* `r_near` is a LENGTH, not a (u,v) point, so - unlike a direction or a coordinate - it can safely be measured
     * against the body's FINAL settled position instead of its live, still-moving one, to keep it growing smoothly
     * instead of swinging with the body's own approach (the operator, an earlier report: "the telescoping arms...are
     * extending too much...crossing beyond the required exact distance"). Measured from the FINAL pose, using the
     * SAME fixed-contact-point geometry above (never the old pivot-relative one), this distance is a true constant
     * for the whole reach. */
    float final_u_to_striker = ffdx * final_f.back.x + ffdy * final_f.back.y;
    float final_near_u = final_u_to_striker - striker_r * 1.05f, final_near_v = final_v_to_striker;
    float r_near = sqrtf((final_near_u - pu) * (final_near_u - pu) + (final_near_v - pivot_v) * (final_near_v - pivot_v));

    /* SECOND overshoot source (2026-09-28, an earlier report): even though the LENGTH target above is a stable
     * constant, it is measured against the body's FINAL position while the arm is actually DRAWN from the body's
     * LIVE, still-moving one (needed so its rotation and shape stay attached). While the body has not yet arrived,
     * those two pivots are in different places, so a length correct for the final pivot can still carry the rendered
     * tip past the striker's true edge as measured from where the arm is actually being drawn right now. Fixed with a
     * hard geometric clamp: the LIVE distance to the same fixed contact point is computed fresh every frame and used
     * only as a ceiling (never as the smooth target, so it cannot reintroduce jitter) - the arm (the hand's own tip;
     * see below on the fingers) can never be drawn past the striker's true edge, in either direction, at any point in
     * the motion. */
    float r_near_live_limit = sqrtf((near_u - pu) * (near_u - pu) + (near_v - pivot_v) * (near_v - pivot_v));
    r_near = fminf(r_near, r_near_live_limit);

    /* Resting (reach = 0) lengths along the pivot's own local +u axis - identical to the old resting arm's distances
     * from this same pivot, so at reach = 0 the shape is unchanged from before. */
    float fore_rest_len  = (torso_end - 0.2f  * hr) - pu;
    float hand_near_rest = (torso_end - 0.55f * hr) - pu;
    float hand_far_rest  = (torso_end + 0.15f * hr) - pu;

    float len_fore_far  = fore_rest_len  + ((r_near - 0.35f * hr) - fore_rest_len)  * reach;
    float len_hand_near = hand_near_rest + ((r_near - 0.5f  * hr) - hand_near_rest) * reach;
    float len_hand_far  = hand_far_rest  + (r_near                - hand_far_rest)  * reach;
    float hw_fore = 0.20f * hr + (0.16f * hr - 0.20f * hr) * reach;   /* half-widths taper slightly as it extends, but stay CENTRED on pivot_v - never shift sideways */
    float hw_hand = 0.25f * hr + (0.16f * hr - 0.25f * hr) * reach;

    /* BOTH ARMS MUST RETURN TO NEUTRAL BEFORE IDLE-WAVING RESUMES (2026-09-28, the operator's canonical rule: "At the
     * end of a turn, a robot MUST collapse and return BOTH arms to their normal extents (lengths) and positions
     * (joined to the shoulder, parallel to the correct side of the robot)...Only after returning arms to normal
     * positions, the idle-robot hand animation will resume."). LENGTH already did this correctly - `len_hand_far` and
     * friends are all `lerp(rest_value, target, reach)`, so they land exactly on the resting length the instant
     * `reach` reaches 0, by construction. ANGLE did not: the reaching arm eased, during withdrawal, back toward
     * `arm_spin` - the essentially RANDOM angle it happened to be spinning at when reaching began, not the straight,
     * parallel-to-body neutral pose - and the OTHER arm's "excited flapping" (an ever-increasing angle, never reset)
     * simply stopped and jumped straight into idle-waving the moment `reaching_active` went false, with no guarantee
     * the two even agreed. Neither one was actually returning to a canonical rest position; both just stopped
     * wherever they happened to be and handed off to a completely unrelated function.
     *
     * Fixed with a proper three-state machine, using `reach_peak_seen` (the running maximum of `reach` since this
     * seat last rested) rather than a frame-to-frame comparison, so the flat HOLD right before a shot fires - reach
     * sitting still at its cap, not yet declining - is never mistaken for withdrawal already starting:
     *   - RESTING (`!reaching_active`): plain idle-waving, as always; `reach_peak_seen` resets to 0 so the next shot
     *     starts its own tracking fresh.
     *   - GROWING or HOLDING (`reach` at or above its own peak so far): the reaching arm eases from its live spin
     *     toward the target, the other arm flaps excitedly - unchanged from before.
     *   - WITHDRAWING (`reach` measurably below its own peak): the INSTANT this begins, both arms' current angles are
     *     captured once as the withdrawal's own starting point; from then on both ease from THAT captured angle back
     *     to exactly 0 (straight, parallel to the body) purely as a function of `reach` falling back to 0 - so by the
     *     time `reach` reaches 0, both arms are provably back at their canonical rest position, and idle-waving can
     *     only ever pick up from there. */
    if (reach > g_vis.reach_peak_seen[seat]) g_vis.reach_peak_seen[seat] = reach;
    bool shrinking_now = reaching_active && (reach < g_vis.reach_peak_seen[seat] - 0.0001f);
    if (shrinking_now && !g_vis.was_shrinking[seat]) {
        g_vis.withdrawal_reach_ang[seat] = lerp_angle_shortest(arm_spin, theta_target, reach);
        g_vis.withdrawal_idle_ang[seat] = 10.5f * t + 2.0f;
        g_vis.withdrawal_peak_reach[seat] = (g_vis.reach_peak_seen[seat] > 0.0001f) ? g_vis.reach_peak_seen[seat] : 1.0f;
    }
    g_vis.was_shrinking[seat] = shrinking_now;

    float flap_r = 0.0f, flap_l = 0.0f;
    float reach_ang;
    if (!reaching_active) {
        /* IDLE SWAY AMPLITUDE (2026-09-28): this used to scale idle_wave() (range -1..1) by 0.9f, i.e. up to about
         * 51 degrees of swing per arm, independently per arm. The withdrawal state machine above provably lands both
         * arms EXACTLY at neutral (0) the moment reach reaches 0 - but idle-waving then took over and was free to
         * swing either arm up to 51 degrees off the body, which at an unlucky phase (both arms swung outward at
         * once) looks exactly like the operator's "arms NOT in their normal positions" screenshots, even though the
         * state machine itself was correct. This is a genuinely different bug from anything fixed earlier this
         * session: it is not about the withdrawal transition at all, it is that the resting idle animation's own
         * range was never checked against the canonical "parallel to the correct side of the robot" rule. Cut to a
         * small twitch (about 8 degrees) so a waiting robot still reads as alive without ever looking un-tucked. */
        flap_r = 0.15f * idle_wave(seat, 0, t * 1.8f);
        flap_l = 0.15f * idle_wave(seat, 1, t * 1.8f);
        reach_ang = (side_sign > 0.0f) ? flap_r : flap_l;
        g_vis.reach_peak_seen[seat] = 0.0f;
    } else if (!shrinking_now) {
        reach_ang = lerp_angle_shortest(arm_spin, theta_target, reach);
        float excited = 10.5f * t + 2.0f;
        if (side_sign > 0.0f) { flap_l = excited; } else { flap_r = excited; }
    } else {
        float frac = reach / g_vis.withdrawal_peak_reach[seat];
        if (frac > 1.0f) frac = 1.0f;
        if (frac < 0.0f) frac = 0.0f;
        reach_ang = lerp_angle_shortest(0.0f, g_vis.withdrawal_reach_ang[seat], frac);
        float w_idle = lerp_angle_shortest(0.0f, g_vis.withdrawal_idle_ang[seat], frac);
        if (side_sign > 0.0f) { flap_l = w_idle; } else { flap_r = w_idle; }
    }
    float idle_ang = (side_sign > 0.0f) ? flap_l : flap_r;
    robot_rbox(&f, pu, pivot_v, reach_ang, pu, pu + len_fore_far, pivot_v - hw_fore, pivot_v + hw_fore, steel, line);
    robot_rbox(&f, pu, pivot_v, reach_ang, pu + len_hand_near, pu + len_hand_far, pivot_v - hw_hand, pivot_v + hw_hand, dark, line);
    robot_rbox(&f, pu, -pivot_v, idle_ang, sh0 + 0.1f * hr, torso_end - 0.2f * hr, -arm_v - 0.4f * hr, -arm_v, steel, line);
    robot_rbox(&f, pu, -pivot_v, idle_ang, torso_end - 0.55f * hr, torso_end + 0.15f * hr, -arm_v - 0.45f * hr, -arm_v + 0.05f * hr, dark, line);
    /* torso with a chest panel and three lights */
    robot_box(&f, sh0, torso_end, -1.1f * hr, 1.1f * hr, base, line);
    robot_box(&f, sh0 + 0.9f * hr, torso_end - 0.25f * hr, -0.8f * hr, 0.8f * hr, dark, line);
    for (int k = -1; k <= 1; k++) {
        Vector2 p = robot_pt(&f, sh0 + 1.35f * hr, (float)k * 0.42f * hr);
        DrawCircleV(p, hr * 0.15f, eye);
    }
    /* Telescoping FINGERS (thumb, index, middle) used to continue on past the hand's own tip, all the way through to
     * the striker's FAR side, to visually "flick" it - removed for now (2026-09-28), the operator: "take the fingers
     * out; that logic is another whole software evolution, we will push it to a future enhancement." The hand alone
     * (above) already reaches exactly to the striker's near rim, correctly and without overshoot; `strike_side` is
     * still computed and passed in for whenever the finger mechanism is rebuilt, just unused for now. */
    /* shoulders */
    robot_box(&f, sh0, sh1, -1.45f * hr, 1.45f * hr, light, line);
    /* neck */
    robot_box(&f, hr - 1.0f, sh0 + 1.0f, -0.32f * hr, 0.32f * hr, steel, line);
    /* head, with two eyes toward the board and a mouth grille */
    robot_box(&f, -hr, hr, -1.05f * hr, 1.05f * hr, light, line);
    robot_box(&f, 0.05f * hr, 0.55f * hr, -0.7f * hr, -0.2f * hr, eye, line);
    robot_box(&f, 0.05f * hr, 0.55f * hr, 0.2f * hr, 0.7f * hr, eye, line);
    /* pupils: a waiting robot rolls its eyes along its own circle, at its own speed and direction */
    if (!is_current_turn) {
        float dir = (seat & 1) ? -1.0f : 1.0f;
        float ph = dir * t * (1.6f + 0.55f * (float)seat) + 1.3f * (float)seat + 0.8f * idle_wave(seat, 2, t);
        float ru = 0.13f * hr * cosf(ph), rv = 0.13f * hr * sinf(ph);
        Vector2 pl = robot_pt(&f, 0.3f * hr + ru, -0.45f * hr + rv), pr = robot_pt(&f, 0.3f * hr + ru, 0.45f * hr + rv);
        DrawCircleV(pl, hr * 0.11f, (Color){ 20, 20, 28, (unsigned char)(255 * alpha) });
        DrawCircleV(pr, hr * 0.11f, (Color){ 20, 20, 28, (unsigned char)(255 * alpha) });
    }
    robot_box(&f, 0.72f * hr, 0.9f * hr, -0.5f * hr, 0.5f * hr, dark, line);
    /* antenna with a ball */
    Vector2 a0 = robot_pt(&f, -hr, 0.0f), a1 = robot_pt(&f, -1.75f * hr, 0.0f);
    DrawLineEx(a0, a1, 2.0f, line);
    DrawCircleV(a1, hr * 0.3f, is_current_turn ? COLOR_TURN_HIGHLIGHT : base);
    DrawCircleLines((int)a1.x, (int)a1.y, hr * 0.3f, line);
}

static BoardViewDebug g_dbg;

void board_view_get_debug(BoardViewDebug* out) {
    *out = g_dbg;
    out->striker_valid = g_vis.striker_valid;
    out->striker_vis = g_vis.striker;
    for (int i = 0; i < 4; i++) out->figures[i] = g_vis.fig[i];
}

static float approach_f(float cur, float target, float max_step) {
    float d = target - cur;
    if (d > max_step) return cur + max_step;
    if (d < -max_step) return cur - max_step;
    return target;
}

/* Shortest-path interpolation between two angles (radians), so a blend never spins the long way round. */
static float lerp_angle_shortest(float a, float b, float t) {
    float d = b - a;
    while (d > (float)M_PI) d -= 2.0f * (float)M_PI;
    while (d < -(float)M_PI) d += 2.0f * (float)M_PI;
    return a + d * t;
}

/* OPTIMAL STANDING POSITION (2026-09-28, the operator: "find the optimal algorithm for placing the robot...that causes
 * the minimum mathematically possible rotation and arm extension"). The two goals turn out not to compete at all: the
 * body's ROTATION is fixed the instant the shot is chosen - it must point along the shot's own back_angle so the arm
 * approaches from the correct side, and that requirement does not depend in any way on WHERE along the boundary the
 * robot is standing. Rotation is therefore already at its one possible (so trivially minimal) value regardless of
 * position, and the only thing left to optimise is EXTENSION: how far the arm has to reach. That is minimised by
 * simply standing at the point on the boundary closest to `target` (the exact point the arm needs to reach) - a plain
 * nearest-point-on-a-rectangle's-edge computation, closed-form, no search needed. (This measures from the BODY'S
 * centre, not its arm's own shoulder pivot, which sits a small, fixed distance to the side of centre - a deliberate
 * simplification: that offset is only a few percent of a typical reach and does not change which side is closest.)
 * Only the seat's OWN side and its two ADJACENT sides are ever considered, never the OPPOSITE one - a seat's robot
 * should never stand on the far side of the board, however the maths might otherwise tempt it. The old approach
 * (ray-cast from the striker along back_angle until it exits the box) is gone: it did not minimise anything - it just
 * placed the robot wherever a straight line in the aim-reverse direction happened to land, which could be far short of
 * the closest point for a raking shot. The box is the four robots' own fixed outside-the-board lines, taken together
 * as one rectangle (`x_min`..`x_max`, `y_min`..`y_max`); the robot's body never enters the board because this box is
 * strictly outside it. */
static Vec2 closest_standing_point(Seat seat, Vec2 target, float x_min, float x_max, float y_min, float y_max) {
    float cx = target.x < x_min ? x_min : (target.x > x_max ? x_max : target.x);
    float cy = target.y < y_min ? y_min : (target.y > y_max ? y_max : target.y);
    float d_top = y_max - target.y, d_bottom = target.y - y_min;
    float d_left = target.x - x_min, d_right = x_max - target.x;
    Vec2 p_top = { cx, y_max }, p_bottom = { cx, y_min }, p_left = { x_min, cy }, p_right = { x_max, cy };
    float best_d; Vec2 best;
    switch (seat) {
        case SEAT_NORTH: best_d = d_top;    best = p_top;    break;
        case SEAT_SOUTH: best_d = d_bottom; best = p_bottom; break;
        case SEAT_EAST:  best_d = d_right;  best = p_right;  break;
        case SEAT_WEST:  best_d = d_left;   best = p_left;   break;
        default:         best_d = d_top;    best = p_top;    break;
    }
    /* BUG FIX (2026-09-28, found from a per-frame trace showing the arm's target angle nowhere near the striker at
     * full reach, up to +-180 degrees off, on seats whose shot happened to put the striker closer to the OPPOSITE
     * edge than their own): these exclusion guards had the seat pairings backwards. `d_bottom` is SOUTH's own line
     * and NORTH's opposite, so it must be excluded when seat == NORTH, not when seat == SOUTH (SOUTH excluding
     * itself here was harmless - its own side is already the starting candidate from the switch above - but every
     * OTHER seat, including NORTH, was then wrongly free to jump to the far side of the board whenever the maths
     * said it was closer). Same mistake, mirrored, for the other three. */
    if (seat != SEAT_NORTH && d_bottom < best_d) { best_d = d_bottom; best = p_bottom; }
    if (seat != SEAT_SOUTH && d_top    < best_d) { best_d = d_top;    best = p_top;    }
    if (seat != SEAT_WEST  && d_right  < best_d) { best_d = d_right;  best = p_right;  }
    if (seat != SEAT_EAST  && d_left   < best_d) { best_d = d_left;   best = p_left;   }
    return best;
}

/* Which pair of the three fingers (thumb/index/middle) flicks the strike, from how far the shot's own direction (back_angle,
 * pointing back through the striker) deviates from the seat's ordinary square-on facing (seat_default_angle): close to it is
 * a forward strike (thumb+middle pinch it from both sides); off to one side or the other is a parallel/low strike, flicked
 * with the two fingers on that side (thumb+index, or index+middle). Purely cosmetic: the shot itself is unchanged. */
static int classify_strike_side(float back_angle, float seat_default_angle) {
    float dev = math_wrap_angle(back_angle - seat_default_angle);
    const float FORWARD_HALF_ANGLE = 0.45f;   /* about 26 degrees either side counts as "forward" */
    if (fabsf(dev) < FORWARD_HALF_ANGLE) return 0;      /* forward: thumb + middle */
    return (dev > 0.0f) ? 2 : 1;                        /* right: index + middle; left: thumb + index */
}

static Vec2 approach_v(Vec2 cur, Vec2 target, float max_step) {
    float dx = target.x - cur.x, dy = target.y - cur.y;
    float dist = sqrtf(dx * dx + dy * dy);
    if (dist <= max_step || dist < 1e-6f) return target;
    float k = max_step / dist;
    return (Vec2){ cur.x + dx * k, cur.y + dy * k };
}

/* Compute thinking striker baseline coordinate for a given seat and time.
 * Returns the oscillating baseline coordinate (x for N/S, y for E/W).
 * Mirrors the logic in effects.c:draw_thinking_striker()
 */
static float compute_thinking_striker_baseline_coord(Seat seat, double wall_time) {
    // Triangle wave along the baseline (continuous, no jump): one full there-and-back every 2.4 s
    float slide_period = 2.4f;
    float slide_phase = my_fmodf((float)wall_time, slide_period) / slide_period;  // 0 to 1
    float slide_t = slide_phase <= 0.5f ? slide_phase * 2.0f : (1.0f - slide_phase) * 2.0f;  // 0->1->0
    float slide_offset = BASELINE_MIN_OFFSET + slide_t * (BASELINE_MAX_OFFSET - BASELINE_MIN_OFFSET);
    return slide_offset;
}

static Vec2 thinking_striker_world(Seat seat, double wall_time) {
    float c = compute_thinking_striker_baseline_coord(seat, wall_time);
    switch (seat) {
        case SEAT_NORTH: return (Vec2){ c, BASELINE_Y_NORTH };
        case SEAT_SOUTH: return (Vec2){ c, BASELINE_Y_SOUTH };
        case SEAT_EAST:  return (Vec2){ BASELINE_X_EAST, c };
        default:         return (Vec2){ BASELINE_X_WEST, c };
    }
}

/* The upcoming stroke's aim line, drawn INSIDE BeginMode2D() AFTER the striker.
 * Origin = the striker's current rendered position. Direction = game->computed_shot_plan.aim_angle.
 * Length is strictly proportional to the strike force (see aim_line_length) and grows in over the aim
 * preview (see aim_line_progress). Thin line, subdued amber, with an arrowhead at the far end. */
/* raylib culls clockwise triangles, so draw both windings to be sure the arrowhead is visible */
static void draw_tri_any_winding(Vector2 a, Vector2 b, Vector2 c, Color col) {
    DrawTriangle(a, b, c, col);
    DrawTriangle(a, c, b, col);
}

static void draw_aim_preview_line(Viewport vp, const GameState* game, const Layout* L, Vec2 striker_pos) {
    if (!game || !game->computed_shot_valid) return;

    float aim_angle = game->computed_shot_plan.aim_angle;
    float power = game->computed_shot_plan.power;
    (void)L;

    /* The line grows from the middle of the striker to its final length, which is strictly proportional to the strike force
     * (full power = AIM_LINE_FULL_POWER_LEN board widths). It takes the first 85% of the aim preview (at most 3 s in all). */
    float grow = game->aim_line_progress / 0.85f;
    if (grow < 0.0f) grow = 0.0f;
    if (grow > 1.0f) grow = 1.0f;
    grow = 1.0f - (1.0f - grow) * (1.0f - grow);                 /* quick at first, settling into place */
    float clamped_len = aim_line_length(power) * grow;

    Vec2 start_screen = math_world_to_screen(vp, striker_pos);
    Vec2 end_world = { striker_pos.x + cosf(aim_angle) * clamped_len, striker_pos.y + sinf(aim_angle) * clamped_len };
    Vec2 end_screen = math_world_to_screen(vp, end_world);
    g_dbg.aim_drawn = true;
    g_dbg.aim_start = striker_pos;
    g_dbg.aim_end = end_world;

    /* Direction in SCREEN space (world y is flipped on screen), so the head points along the drawn line */
    float sdx = end_screen.x - start_screen.x, sdy = end_screen.y - start_screen.y;
    float slen = sqrtf(sdx * sdx + sdy * sdy);
    if (slen < 1.0f) return;
    Vec2 dir = { sdx / slen, sdy / slen };
    Vec2 perp = { -dir.y, dir.x };

    float thickness = my_fmaxf(1.5f, math_world_to_screen_dist(vp, 0.004f));      /* a thin line */
    float arrow_size = my_fmaxf(11.0f, thickness * 6.0f);
    if (arrow_size > slen) arrow_size = slen;                                      /* never longer than the line itself */

    /* The line stops inside the arrowhead: a line running on to the tip would poke out of the point as a little fork */
    Vec2 line_end = { end_screen.x - dir.x * arrow_size * 0.6f, end_screen.y - dir.y * arrow_size * 0.6f };
    DrawLineEx((Vector2){ start_screen.x, start_screen.y }, (Vector2){ line_end.x, line_end.y }, thickness + 2.0f, COLOR_AIM_PREVIEW_OUTLINE);
    DrawLineEx((Vector2){ start_screen.x, start_screen.y }, (Vector2){ line_end.x, line_end.y }, thickness, COLOR_AIM_PREVIEW_LINE);

    Vec2 tip = end_screen;
    Vec2 left = { tip.x - dir.x * arrow_size + perp.x * (arrow_size * 0.5f), tip.y - dir.y * arrow_size + perp.y * (arrow_size * 0.5f) };
    Vec2 right = { tip.x - dir.x * arrow_size - perp.x * (arrow_size * 0.5f), tip.y - dir.y * arrow_size - perp.y * (arrow_size * 0.5f) };
    draw_tri_any_winding((Vector2){ tip.x, tip.y }, (Vector2){ left.x, left.y }, (Vector2){ right.x, right.y }, COLOR_AIM_PREVIEW_OUTLINE);

    /* the yellow fill, one pixel inside the black outline all round */
    float in = 1.0f, fs = arrow_size - 2.0f * in;
    if (fs > 1.0f) {
        Vec2 ft = { tip.x - dir.x * (in * 2.2f), tip.y - dir.y * (in * 2.2f) };
        Vec2 fl = { ft.x - dir.x * fs + perp.x * (fs * 0.5f * 0.85f), ft.y - dir.y * fs + perp.y * (fs * 0.5f * 0.85f) };
        Vec2 fr = { ft.x - dir.x * fs - perp.x * (fs * 0.5f * 0.85f), ft.y - dir.y * fs - perp.y * (fs * 0.5f * 0.85f) };
        draw_tri_any_winding((Vector2){ ft.x, ft.y }, (Vector2){ fl.x, fl.y }, (Vector2){ fr.x, fr.y }, COLOR_AIM_PREVIEW_ARROW);
    }
}

void board_view_draw(Viewport vp, const BoardState* board, const PhysicsWorld* physics, float alpha, const Layout* L, int game_phase, const GameState* game, double placement_timer) {
    g_dbg.aim_drawn = false;
    g_dbg.drawn_from_physics_mask = 0;
    // Determine current turn seat from game turn (not striker owner, which is stale during THINKING/PLACEMENT/AIM_PREVIEW)
    Seat current_turn_seat = game ? game->turn_seat : board->striker.owner_seat;
    
    // Clamp alpha to [0, 1]
    if (alpha < 0.0f) alpha = 0.0f;
    if (alpha > 1.0f) alpha = 1.0f;
    
    // Get current and previous physics positions for interpolation
    Vec2 curr_positions[MAX_PIECES];
    Vec2 prev_positions[MAX_PIECES];
    Vec2 curr_striker_pos = {0, 0};
    Vec2 prev_striker_pos = {0, 0};
    bool use_physics = false;
    
    if (physics) {
        physics_get_positions(physics, curr_positions);
        physics_get_prev_positions(physics, prev_positions);
        physics_get_striker_position(physics, &curr_striker_pos);
        physics_get_prev_striker_position(physics, &prev_striker_pos);
        
        // Check if physics has valid positions (not all zero)
        for (int i = 0; i < MAX_PIECES; i++) {
            if (curr_positions[i].x != 0 || curr_positions[i].y != 0) {
                use_physics = true;
                break;
            }
        }
    }
    
    // Board surface - draw from camera-local (0, 0) to (board_size, board_size)
    // math_world_to_screen flips Y, so world top-left (-0.5, 0.5) maps to camera-local (0, 0)
    // and world bottom-right (0.5, -0.5) maps to camera-local (board_size, board_size)
    float board_half = BOARD_SIDE_NORM * 0.5f;
    Vec2 tl = math_world_to_screen(vp, (Vec2){ -board_half, board_half });   // World top-left -> camera-local (0, 0)
    Vec2 br = math_world_to_screen(vp, (Vec2){ board_half, -board_half });   // World bottom-right -> camera-local (board_size, board_size)
    float board_w = br.x - tl.x;
    float board_h = br.y - tl.y;
    DrawRectangle((int)tl.x, (int)tl.y, (int)board_w, (int)board_h, COLOR_BOARD);
    
    // Cushions
    float cushion_t = math_world_to_screen_dist(vp, CUSHION_THICKNESS);
    
    // Top cushion
    DrawRectangle((int)tl.x, (int)tl.y, (int)board_w, (int)cushion_t, COLOR_CUSHION);
    // Bottom cushion
    DrawRectangle((int)tl.x, (int)(br.y - cushion_t), (int)board_w, (int)cushion_t, COLOR_CUSHION);
    // Left cushion
    DrawRectangle((int)tl.x, (int)tl.y, (int)cushion_t, (int)board_h, COLOR_CUSHION);
    // Right cushion
    DrawRectangle((int)(br.x - cushion_t), (int)tl.y, (int)cushion_t, (int)board_h, COLOR_CUSHION);
    
    // Center circle
    Vec2 center = math_world_to_screen(vp, (Vec2){0, 0});
    float circle_r = math_world_to_screen_dist(vp, 0.08f);
    DrawCircleLines((int)center.x, (int)center.y, circle_r, COLOR_LINE);
    
    // Baselines
    float baseline_y_n = math_world_to_screen(vp, (Vec2){0, BASELINE_Y_NORTH}).y;
    float baseline_y_s = math_world_to_screen(vp, (Vec2){0, BASELINE_Y_SOUTH}).y;
    float baseline_x_e = math_world_to_screen(vp, (Vec2){BASELINE_X_EAST, 0}).x;
    float baseline_x_w = math_world_to_screen(vp, (Vec2){BASELINE_X_WEST, 0}).x;
    
    float baseline_len = math_world_to_screen_dist(vp, BASELINE_MAX_OFFSET * 2);
    float baseline_x_start = center.x - baseline_len * 0.5f;
    float baseline_y_start = center.y - baseline_len * 0.5f;
    
    // Draw muted baseline lines (30% alpha)
    DrawLine((int)baseline_x_start, (int)baseline_y_n, (int)(baseline_x_start + baseline_len), (int)baseline_y_n, COLOR_BASELINE_MUTED);
    DrawLine((int)baseline_x_start, (int)baseline_y_s, (int)(baseline_x_start + baseline_len), (int)baseline_y_s, COLOR_BASELINE_MUTED);
    DrawLine((int)baseline_x_e, (int)baseline_y_start, (int)baseline_x_e, (int)(baseline_y_start + baseline_len), COLOR_BASELINE_MUTED);
    DrawLine((int)baseline_x_w, (int)baseline_y_start, (int)baseline_x_w, (int)(baseline_y_start + baseline_len), COLOR_BASELINE_MUTED);
    
    // =========================================================================
    // FIGURE POSITIONING LOGIC (R8-2, R8-3)
    // =========================================================================
    
    // Compute figure height in screen pixels: head_radius + gap + torso_height
    // Figure faces toward board center, so extends from head toward board
    // Margin from board boundary = required_margin, where required_margin = max(board_size/10, 24px)
    // The figure's NEAREST point (bottom of torso for N/S) should be at board_boundary + required_margin
    // Head center is at board_boundary + required_margin + body_length
    float required_margin_px = (float)FIG_MARGIN_PX;
    float margin_world = required_margin_px / vp.world_to_screen;
    
    // Figure body length in world units (head_radius + gap + torso_height)
    float head_radius = (float)L->board_size * FIG_SCALE / 25.0f;
    float torso_height = (float)L->board_size * FIG_SCALE / 12.0f;
    float gap = 2.0f; // pixels
    float body_length_world = (head_radius + gap + torso_height) / vp.world_to_screen;
    
    // Figure perpendicular offset: distance from cushion INNER edge to figure's nearest point
    // Figure faces toward board, so nearest point is bottom of torso (N/S) or side of torso (E/W)
    // Head center must be at: cushion_inner + margin + body_length
    // Board boundaries (where the playing area ends, at ±0.5)
    // The figure must stay entirely OUTSIDE the board boundaries (±0.5)
    const float BOARD_BOUNDARY_Y_NORTH = 0.5f;
    const float BOARD_BOUNDARY_Y_SOUTH = -0.5f;
    const float BOARD_BOUNDARY_X_EAST = 0.5f;
    const float BOARD_BOUNDARY_X_WEST = -0.5f;
    
    // Figure perpendicular offset: distance from board boundary to figure's nearest point
    // Figure faces toward board, so nearest point is bottom of torso (N/S) or side of torso (E/W)
    // Head center is at: board_boundary + margin + body_length
    // The fixed offset for head center = margin + body_length
    float figure_head_offset = margin_world + body_length_world;
    
    // Store fixed perpendicular offset per seat (world units) - NEVER changes
    // Figure head center at board_boundary + margin + body_length
    float figure_fixed_offset_world[4] = {
        figure_head_offset,  // SEAT_NORTH: head at BOARD_BOUNDARY_Y_NORTH + margin + body_length
        figure_head_offset,  // SEAT_EAST:  head at BOARD_BOUNDARY_X_EAST + margin + body_length
        figure_head_offset,  // SEAT_SOUTH: head at BOARD_BOUNDARY_Y_SOUTH - margin - body_length
        figure_head_offset   // SEAT_WEST:  head at BOARD_BOUNDARY_X_WEST - margin - body_length
    };
    
    // Figure alpha: pulsing 40-100% during THINKING and AIM_PREVIEW phases (synced with striker flash), 100% otherwise
    double wall_time = GetTime();
    float figure_alpha = 1.0f;
    bool is_thinking_or_preview = (game_phase == PHASE_THINKING || game_phase == PHASE_AIM_PREVIEW);
    (void)is_thinking_or_preview;   /* the figures no longer fade; waiting robots animate instead */
    
    // Current time for the robots' animations
    float current_time = (float)wall_time;
    
    // Per-seat figure world positions
    Vec2 north_world = {0, 0};
    Vec2 south_world = {0, 0};
    Vec2 east_world = {0, 0};
    Vec2 west_world = {0, 0};
    
    // Phase-specific logic
    bool is_thinking = (game_phase == PHASE_THINKING);
    bool is_placement = (game_phase == PHASE_PLACEMENT);
    bool is_aim_preview = (game_phase == PHASE_AIM_PREVIEW);
    
    // For PLACEMENT phase: ease from THINKING-end position to final position
    // placement_timer goes from PLACEMENT_HOLD_TIME (1.0s) down to 0
    // Progress = 1 - (placement_timer / PLACEMENT_HOLD_TIME), so 0 at start, 1 at end
    const float PLACEMENT_HOLD_TIME = 1.0f;
    float placement_progress = 0.0f;
    if (is_placement && placement_timer > 0.0) {
        placement_progress = 1.0f - (float)(placement_timer / PLACEMENT_HOLD_TIME);
        if (placement_progress < 0.0f) placement_progress = 0.0f;
        if (placement_progress > 1.0f) placement_progress = 1.0f;
    } else if (is_aim_preview) {
        // During AIM_PREVIEW, hold figure at final position (progress = 1.0)
        placement_progress = 1.0f;
    }
    
    // NORTH seat (top) - WHITE team, faces down (angle = -PI/2) toward board center
    // Figure extends DOWNWARD from head. Head center at CUSHION_INNER_Y_NORTH + margin + body_length.
    
    // Fixed perpendicular coordinate (y) for NORTH: head at BOARD_BOUNDARY_Y_NORTH + figure_fixed_offset_world
    // figure_fixed_offset_world already includes margin + body_length
    float north_fixed_y = BOARD_BOUNDARY_Y_NORTH + figure_fixed_offset_world[SEAT_NORTH];
    
    // Baseline-axis coordinate (x) varies by phase
    float north_baseline_x;
    if (is_thinking && game && game->turn_seat == SEAT_NORTH) {
        // THINKING: mirror striker's oscillating baseline coordinate
        north_baseline_x = compute_thinking_striker_baseline_coord(SEAT_NORTH, wall_time);
    } else if ((is_placement || is_aim_preview) && game && game->turn_seat == SEAT_NORTH) {
        // PLACEMENT: ease from THINKING-end position to final striker position
        // AIM_PREVIEW: hold at final position
        float thinking_x = compute_thinking_striker_baseline_coord(SEAT_NORTH, wall_time);
        float final_x = board->striker.position.x;  // striker already placed
        north_baseline_x = thinking_x + placement_progress * (final_x - thinking_x);
    } else {
        // IDLE, or not current turn: rest at center of baseline (0.0)
        north_baseline_x = 0.0f;
    }
    north_world = (Vec2){ north_baseline_x, north_fixed_y };
    
    // SOUTH seat (bottom) - WHITE team, faces up (angle = PI/2) toward board center
    // Figure extends UPWARD from head. Head center at BOARD_BOUNDARY_Y_SOUTH - margin - body_length.
    
    // Fixed perpendicular coordinate (y) for SOUTH: head at BOARD_BOUNDARY_Y_SOUTH - figure_fixed_offset_world
    // figure_fixed_offset_world already includes margin + body_length
    float south_fixed_y = BOARD_BOUNDARY_Y_SOUTH - figure_fixed_offset_world[SEAT_SOUTH];
    
    // Baseline-axis coordinate (x) varies by phase
    float south_baseline_x;
    if (is_thinking && game && game->turn_seat == SEAT_SOUTH) {
        south_baseline_x = compute_thinking_striker_baseline_coord(SEAT_SOUTH, wall_time);
    } else if ((is_placement || is_aim_preview) && game && game->turn_seat == SEAT_SOUTH) {
        float thinking_x = compute_thinking_striker_baseline_coord(SEAT_SOUTH, wall_time);
        float final_x = board->striker.position.x;
        south_baseline_x = thinking_x + placement_progress * (final_x - thinking_x);
    } else {
        south_baseline_x = 0.0f;
    }
    south_world = (Vec2){ south_baseline_x, south_fixed_y };
    
    // EAST seat (right) - BLACK team, faces left (angle = PI) toward board center
    // Figure extends LEFTWARD from head. Head center should be RIGHT of board boundary by margin + body_length.
    
    // Fixed perpendicular coordinate (x) for EAST: head at BOARD_BOUNDARY_X_EAST + figure_fixed_offset_world
    // figure_fixed_offset_world already includes margin + body_length
    float east_fixed_x = BOARD_BOUNDARY_X_EAST + figure_fixed_offset_world[SEAT_EAST];
    
    // Baseline-axis coordinate (y) varies by phase
    float east_baseline_y;
    if (is_thinking && game && game->turn_seat == SEAT_EAST) {
        east_baseline_y = compute_thinking_striker_baseline_coord(SEAT_EAST, wall_time);
    } else if ((is_placement || is_aim_preview) && game && game->turn_seat == SEAT_EAST) {
        float thinking_y = compute_thinking_striker_baseline_coord(SEAT_EAST, wall_time);
        float final_y = board->striker.position.y;
        east_baseline_y = thinking_y + placement_progress * (final_y - thinking_y);
    } else {
        east_baseline_y = 0.0f;
    }
    east_world = (Vec2){ east_fixed_x, east_baseline_y };
    
    // WEST seat (left) - BLACK team, faces right (angle = 0) toward board center
    // Figure extends RIGHTWARD from head. Head center should be LEFT of board boundary by margin + body_length.
    
    // Fixed perpendicular coordinate (x) for WEST: head at BOARD_BOUNDARY_X_WEST - figure_fixed_offset_world
    // figure_fixed_offset_world already includes margin + body_length
    float west_fixed_x = BOARD_BOUNDARY_X_WEST - figure_fixed_offset_world[SEAT_WEST];
    
    // Baseline-axis coordinate (y) varies by phase
    float west_baseline_y;
    if (is_thinking && game && game->turn_seat == SEAT_WEST) {
        west_baseline_y = compute_thinking_striker_baseline_coord(SEAT_WEST, wall_time);
    } else if ((is_placement || is_aim_preview) && game && game->turn_seat == SEAT_WEST) {
        float thinking_y = compute_thinking_striker_baseline_coord(SEAT_WEST, wall_time);
        float final_y = board->striker.position.y;
        west_baseline_y = thinking_y + placement_progress * (final_y - thinking_y);
    } else {
        west_baseline_y = 0.0f;
    }
    west_world = (Vec2){ west_fixed_x, west_baseline_y };
    
    // ---- Smooth visual state update (striker + figures)
    {
        static double last_wall = -1.0;
        float frame_dt = (last_wall < 0.0) ? 0.0f : (float)(wall_time - last_wall);
        last_wall = wall_time;
        if (frame_dt < 0.0f) frame_dt = 0.0f;
        if (frame_dt > 0.1f) frame_dt = 0.1f;
        float step = VISUAL_SLIDE_SPEED * frame_dt;
        bool planning = is_thinking || is_placement || is_aim_preview;
        bool striker_gone = physics && physics_is_striker_pocketed(physics);
        if (striker_gone && !planning) g_vis.was_gone = true;
        if (g_vis.appear < 1.0f) { g_vis.appear += frame_dt / 0.6f; if (g_vis.appear > 1.0f) g_vis.appear = 1.0f; }

        if (game && is_thinking) {
            Vec2 target = thinking_striker_world(game->turn_seat, wall_time);
            if (g_vis.was_gone && !effects_striker_recovering()) {
                Vec2 slid_to;
                if (effects_take_striker_slide_done(&slid_to)) { g_vis.striker = slid_to; g_vis.appear = 1.0f; }   /* it has just slid back to the player */
                else { g_vis.striker = target; g_vis.appear = 0.0f; }
                g_vis.was_gone = false; g_vis.striker_valid = true;
            }
            g_vis.striker = g_vis.striker_valid ? approach_v(g_vis.striker, target, step) : target;
            g_vis.striker_valid = true;
        } else if (game && (is_placement || is_aim_preview)) {
            Vec2 target = board->striker.position;
            if (g_vis.was_gone && !effects_striker_recovering()) {
                Vec2 slid_to;
                if (effects_take_striker_slide_done(&slid_to)) { g_vis.striker = slid_to; g_vis.appear = 1.0f; }   /* it has just slid back to the player */
                else { g_vis.striker = target; g_vis.appear = 0.0f; }
                g_vis.was_gone = false; g_vis.striker_valid = true;
            }
            g_vis.striker = g_vis.striker_valid ? approach_v(g_vis.striker, target, step) : target;
            g_vis.striker_valid = true;
        } else if (use_physics && !striker_gone && !board->striker.pocketed) {
            g_vis.striker = vec2_lerp(prev_striker_pos, curr_striker_pos, alpha);
            g_vis.striker_valid = true;
        }

        for (int s = 0; s < 4; s++) {
            float target = 0.0f;
            if (game && planning && game->turn_seat == (Seat)s) {
                target = (s == SEAT_NORTH || s == SEAT_SOUTH) ? g_vis.striker.x : g_vis.striker.y;
            }
            g_vis.fig[s] = g_vis.fig_valid ? approach_f(g_vis.fig[s], target, step) : target;
        }
        g_vis.fig_valid = true;
        north_world.x = g_vis.fig[SEAT_NORTH];
        south_world.x = g_vis.fig[SEAT_SOUTH];
        east_world.y  = g_vis.fig[SEAT_EAST];
        west_world.y  = g_vis.fig[SEAT_WEST];

        /* Turning to the shot: as the aim line grows (AIM_PREVIEW), the shooting robot's body stays exactly on its own
         * fixed outside-the-board line (never enters the board) but slides along it, in exact lock-step with the line's own
         * growth, to where that line - extended BACKWARDS through the striker - crosses that same fixed line; it turns to
         * face straight down the line, and its telescoping arm (see draw_human_figure) reaches across the cushion to the
         * striker. Once the shot fires it all smoothly eases back over about 0.4 s (`REACH_WITHDRAW_SPEED`), not a snap. */
        #define REACH_WITHDRAW_SPEED 2.5f
        static const float SEAT_DEFAULT_ANGLE[4] = { -(float)M_PI / 2.0f, 0.0f, (float)M_PI / 2.0f, (float)M_PI };  /* N, E, S, W */
        for (int s = 0; s < 4; s++) {
            float target = 0.0f;
            bool this_seat_aim_preview = (game && is_aim_preview && game->computed_shot_valid && game->turn_seat == (Seat)s);
            if (this_seat_aim_preview) {
                target = game->aim_line_progress;
                /* LATCHED, not recomputed every frame (2026-09-28): the operator saw the standing position - and with
                 * it, which arm reaches - occasionally jump mid-shot ("suddenly swapping...in a jerky weird flipping").
                 * Recomputing this every frame from `g_vis.striker` meant that if the striker's own smoothing had not
                 * quite finished settling exactly when the reach animation began, the target position (especially near
                 * a corner, where two sides of the boundary are close to equally near) could shift again WHILE the arm
                 * was already visibly reaching - a real, if brief, jump. Computed once instead, at the one frame THIS
                 * SEAT's aim preview actually begins (`!g_vis.aim_preview_active[s]`, i.e. it was not already running
                 * last frame) - not at "reach is back near 0", which an extra turn (a pocketed coin, handing the SAME
                 * seat a second shot before its first one has finished withdrawing) can reach well before the new shot
                 * starts, leaving the old, now-wrong target latched for the entire new shot (the operator: the west
                 * robot's "extended arm does not match the striker's vector"; the east robot that "never recovered its
                 * right arm" was the same bug persisting across MULTIPLE later turns, never getting a fresh latch). It
                 * is a true constant for the rest of the shot: nothing touches it again until the NEXT one begins. */
                if (!g_vis.aim_preview_active[s]) {
                    /* TWO angles, not one, and this is the actual root cause behind everything from "the extended arm
                     * does not match the striker's vector" to "never recovered its right arm" (2026-09-28, found by
                     * tracing theta_target at full reach across a long multi-board run: it averaged 93 degrees off
                     * zero, sometimes nearly 180, when a fully converged reach should always put it near zero).
                     *
                     * `game->computed_shot_plan.aim_angle` is a WORLD-space angle (Y increases away from the board,
                     * matching the physics engine and the board boundary constants) - confirmed by the aim arrow
                     * itself, which adds cosf/sinf(aim_angle) directly to a world-space point before converting the
                     * RESULT to screen space. `math_world_to_screen` flips Y (screen.y = centre - world.y * scale), so
                     * a world-space direction's screen-space equivalent negates its y-component - equivalently, the
                     * angle a WORLD vector needs to be treated as, once everything downstream of it is done in screen
                     * pixels, is its MIRROR: `PI - aim_angle`, not `aim_angle`.
                     *
                     * The robot's own body-orientation system (`away`/`f.back` inside draw_human_figure, and the
                     * SEAT_DEFAULT_ANGLE constants they are blended from) is NOT converted through math_world_to_screen
                     * at all - `angle` is used directly, via cosf/sinf, as a SCREEN vector. SEAT_DEFAULT_ANGLE was
                     * chosen to already work correctly in that screen-native system (confirmed by every idle robot in
                     * every screenshot this whole feature has ever produced facing the right way at rest) - but the
                     * shot's own `back_angle` was being computed as `aim_angle + PI`, the WORLD-space "reverse
                     * direction", and fed DIRECTLY into that same screen-native system with no conversion. For a shot
                     * needing very little rotation the mismatch barely shows; for one needing a real turn, the arm
                     * ends up pointing up to 180 degrees away from the striker.
                     *
                     * `back_angle_world` (world convention, unconverted) is still exactly right for anything computed
                     * in WORLD units, like the contact point below - it must stay as `aim_angle + PI`, matching the
                     * arrow's own convention. `back_angle_screen` (`PI - aim_angle`) is its screen-native mirror, and
                     * is what the robot's OWN orientation - and everything measured relative to it - must use instead. */
                    float back_angle_world = math_wrap_angle(game->computed_shot_plan.aim_angle + (float)M_PI);
                    float back_angle_screen = math_wrap_angle((float)M_PI - game->computed_shot_plan.aim_angle);
                    g_vis.aim_pose_angle[s] = back_angle_screen;
                    g_vis.aim_pose_side[s] = classify_strike_side(back_angle_screen, SEAT_DEFAULT_ANGLE[s]);
                    /* The exact point the arm needs to reach: the striker's near rim, diametrically opposite the shot's
                     * own travel direction. Built the same way the (already screenshot-verified) aim arrow itself is -
                     * cosf/sinf of the shot's own angle added directly to a world-space point - so there is no risk of
                     * this world-space computation disagreeing with the world-to-screen convention used elsewhere. */
                    Vec2 back_dir_world = { cosf(back_angle_world), sinf(back_angle_world) };
                    Vec2 contact_point_world = { g_vis.striker.x + back_dir_world.x * (STRIKER_RADIUS_NORM * 1.05f),
                                                  g_vis.striker.y + back_dir_world.y * (STRIKER_RADIUS_NORM * 1.05f) };
                    g_vis.aim_pose_pos[s] = closest_standing_point((Seat)s, contact_point_world, west_fixed_x, east_fixed_x, south_fixed_y, north_fixed_y);
                    g_vis.aim_pose_striker[s] = g_vis.striker;   /* frozen here; stops updating (and so stays put) once the strike ends the aim preview */
                    /* Which arm reaches: whichever side the striker is actually on, computed once here (alongside
                     * everything else this shot needs) instead of being separately, repeatedly re-derived inside
                     * draw_human_figure - one latch point for a shot's whole geometry, not two. Screen-native, like
                     * the body orientation itself - so it must use `back_angle_screen`, not the world one. */
                    Vec2 side_final_screen = math_world_to_screen(vp, g_vis.aim_pose_pos[s]);
                    Vec2 side_away = { cosf(back_angle_screen), sinf(back_angle_screen) };
                    Vec2 side_right = { side_away.y, -side_away.x };
                    Vec2 side_striker_screen = math_world_to_screen(vp, g_vis.striker);
                    float side_dx = side_striker_screen.x - side_final_screen.x, side_dy = side_striker_screen.y - side_final_screen.y;
                    float side_v = side_dx * side_right.x + side_dy * side_right.y;
                    g_vis.arm_side[s] = (side_v >= 0.0f) ? 1.0f : -1.0f;
                }
            }
            g_vis.aim_preview_active[s] = this_seat_aim_preview;
            if (target > g_vis.reach[s]) g_vis.reach[s] = target;
            else g_vis.reach[s] = approach_f(g_vis.reach[s], target, REACH_WITHDRAW_SPEED * frame_dt);

            /* The right arm's spin: held here (and kept live-updated) while NOT telescoping, so that when it starts
             * (`reach` rising) it eases from wherever it actually was to a straight rest, rather than a decaying-amplitude
             * spin that still visibly whips around right up to the last moment. */
            if (g_vis.reach[s] < 0.002f) {
                float spin_now = 14.0f * current_time - 2.0f * (float)M_PI * floorf(14.0f * current_time / (2.0f * (float)M_PI));
                g_vis.arm_spin[s] = spin_now;
            }
        }
    }
    north_world = vec2_lerp(north_world, g_vis.aim_pose_pos[SEAT_NORTH], g_vis.reach[SEAT_NORTH]);
    south_world = vec2_lerp(south_world, g_vis.aim_pose_pos[SEAT_SOUTH], g_vis.reach[SEAT_SOUTH]);
    east_world  = vec2_lerp(east_world,  g_vis.aim_pose_pos[SEAT_EAST],  g_vis.reach[SEAT_EAST]);
    west_world  = vec2_lerp(west_world,  g_vis.aim_pose_pos[SEAT_WEST],  g_vis.reach[SEAT_WEST]);
    float north_angle = lerp_angle_shortest(-(float)M_PI / 2.0f, g_vis.aim_pose_angle[SEAT_NORTH], g_vis.reach[SEAT_NORTH]);
    float south_angle  = lerp_angle_shortest((float)M_PI / 2.0f,  g_vis.aim_pose_angle[SEAT_SOUTH], g_vis.reach[SEAT_SOUTH]);
    float east_angle   = lerp_angle_shortest(0.0f,                g_vis.aim_pose_angle[SEAT_EAST],  g_vis.reach[SEAT_EAST]);
    float west_angle   = lerp_angle_shortest((float)M_PI,         g_vis.aim_pose_angle[SEAT_WEST],  g_vis.reach[SEAT_WEST]);

    // Draw human figures for all four seats
    draw_human_figure(vp, L, north_world, north_angle, TEAM_WHITE, current_turn_seat == SEAT_NORTH, figure_alpha, SEAT_NORTH, current_time, g_vis.reach[SEAT_NORTH], g_vis.aim_pose_striker[SEAT_NORTH], g_vis.aim_pose_side[SEAT_NORTH], g_vis.arm_spin[SEAT_NORTH], g_vis.aim_pose_pos[SEAT_NORTH], g_vis.aim_pose_angle[SEAT_NORTH]);
    draw_human_figure(vp, L, south_world, south_angle, TEAM_WHITE, current_turn_seat == SEAT_SOUTH, figure_alpha, SEAT_SOUTH, current_time, g_vis.reach[SEAT_SOUTH], g_vis.aim_pose_striker[SEAT_SOUTH], g_vis.aim_pose_side[SEAT_SOUTH], g_vis.arm_spin[SEAT_SOUTH], g_vis.aim_pose_pos[SEAT_SOUTH], g_vis.aim_pose_angle[SEAT_SOUTH]);
    draw_human_figure(vp, L, east_world, east_angle, TEAM_BLACK, current_turn_seat == SEAT_EAST, figure_alpha, SEAT_EAST, current_time, g_vis.reach[SEAT_EAST], g_vis.aim_pose_striker[SEAT_EAST], g_vis.aim_pose_side[SEAT_EAST], g_vis.arm_spin[SEAT_EAST], g_vis.aim_pose_pos[SEAT_EAST], g_vis.aim_pose_angle[SEAT_EAST]);
    draw_human_figure(vp, L, west_world, west_angle, TEAM_BLACK, current_turn_seat == SEAT_WEST, figure_alpha, SEAT_WEST, current_time, g_vis.reach[SEAT_WEST], g_vis.aim_pose_striker[SEAT_WEST], g_vis.aim_pose_side[SEAT_WEST], g_vis.arm_spin[SEAT_WEST], g_vis.aim_pose_pos[SEAT_WEST], g_vis.aim_pose_angle[SEAT_WEST]);
    
    // Pockets
    float pocket_r = math_world_to_screen_dist(vp, POCKET_RADIUS_NORM);
    for (int i = 0; i < 4; i++) {
        Vec2 p = math_world_to_screen(vp, POCKET_CENTERS[i]);
        DrawCircle((int)p.x, (int)p.y, pocket_r, COLOR_POCKET);
    }
    
    // Pieces (interpolated from physics for smooth animation, fall back to board state)
    for (int i = 0; i < MAX_PIECES; i++) {
        Vec2 pos;
        if (effects_piece_returning(i)) continue;   /* it is sliding back from its pocket: effects.c draws it */
        bool phys_pocketed = use_physics && physics_is_piece_pocketed(physics, i);
        if (!board_view_piece_draw_pos(board->pieces[i].on_board, phys_pocketed, use_physics,
                                       board->pieces[i].position,
                                       prev_positions[i], curr_positions[i], alpha, &pos)) {
            continue;
        }
        if (use_physics) g_dbg.drawn_from_physics_mask |= (1u << i);
        
        Vec2 screen = math_world_to_screen(vp, pos);
        float piece_r = math_world_to_screen_dist(vp, PIECE_RADIUS_NORM);
        
        Color c;
        if (board->pieces[i].color == PIECE_WHITE) c = COLOR_WHITE_PIECE;
        else if (board->pieces[i].color == PIECE_BLACK) c = COLOR_BLACK_PIECE;
        else c = COLOR_QUEEN;
        
        DrawCircle((int)screen.x, (int)screen.y, piece_r, c);
        draw_coin_rim(screen, piece_r, board->pieces[i].color);
    }
    
    // Pocketed pieces (drawn at their pocketed positions near corners)
    for (int i = 0; i < board->pocketed_count; i++) {
        if (!board->pocketed_pieces[i].pocketed) continue;
        /* still sliding/sinking into the pocket: the slot copy appears when that ends */
        if (effects_piece_falling(board->pocketed_pieces[i].id)) continue;
        
        Vec2 pos = board->pocketed_pieces[i].pocketed_position;
        Vec2 screen = math_world_to_screen(vp, pos);
        float piece_r = math_world_to_screen_dist(vp, PIECE_RADIUS_NORM);
        
        Color c;
        if (board->pocketed_pieces[i].color == PIECE_WHITE) c = COLOR_WHITE_PIECE;
        else if (board->pocketed_pieces[i].color == PIECE_BLACK) c = COLOR_BLACK_PIECE;
        else c = COLOR_QUEEN;
        
        DrawCircle((int)screen.x, (int)screen.y, piece_r, c);
        draw_coin_rim(screen, piece_r, board->pocketed_pieces[i].color);
    }
    
    // Striker: drawn at its tracked visual position (glides between turns, follows physics during a shot)
    if (board->striker.on_baseline && !board->striker.pocketed && g_vis.striker_valid && !effects_striker_recovering() &&
        !(physics && physics_is_striker_pocketed(physics) && !(is_thinking || is_placement || is_aim_preview))) {
        Vec2 screen = math_world_to_screen(vp, g_vis.striker);
        float striker_r = math_world_to_screen_dist(vp, STRIKER_RADIUS_NORM);
        if (is_thinking) {
            float fa = compute_flash_alpha(wall_time);
            DrawCircle((int)screen.x, (int)screen.y, striker_r, (Color){ 255, 215, 0, (unsigned char)(fa * g_vis.appear * 255) });
            DrawCircleLines((int)screen.x, (int)screen.y, striker_r, (Color){ 255, 255, 255, (unsigned char)(fa * g_vis.appear * 100) });
        } else {
            draw_striker_polished((Vector2){ screen.x, screen.y }, striker_r, (unsigned char)(255.0f * g_vis.appear));
        }
    }

    // AIM_PREVIEW phase: draw aim preview line (INSIDE camera, AFTER striker draw)
    // Only draw when in AIM_PREVIEW phase AND computed shot is valid (phase gate)
    // Also ensure striker is stationary (velocity near zero) to prevent aim line during movement
    if (is_aim_preview && game && game->computed_shot_valid) {
        // Check striker velocity is near zero (aim line should not show during movement)
        float striker_speed = math_sqrtf(game->board.striker.velocity.x * game->board.striker.velocity.x + 
                                         game->board.striker.velocity.y * game->board.striker.velocity.y);
        if (striker_speed <= SETTLE_SPEED_EPS) {
            draw_aim_preview_line(vp, game, L, g_vis.striker);
        }
    }
}
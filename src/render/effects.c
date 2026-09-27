#include <math.h>
#include "effects.h"
#include "theme.h"
#include "common/types.h"
#define __USE_MINGW_ANSI_STDIO 1
#include <raylib.h>
#include <stdio.h>
#include "common/vecmath.h"

#define MAX_POCKET_FADE_TIME 0.4f  // pocket flash: 400 ms (twice the original 200 ms)
#define POCKET_SINK_TIME 0.4f      // sim seconds a coin takes to sink out of sight in the hole
#define STRIKER_SINK_TIME 1.1f     // the striker is heavier and drops to the floor: a longer fall
#define PLACEMENT_HOLD_TIME 1.0f   // 1 second at 1x playback

typedef struct {
    Vec2 pocket_center;
    float timer;
    float duration;
    bool active;
} PocketFadeEffect;

static PocketFadeEffect pocket_fades[4] = {0};

typedef struct {
    bool active;
    PieceColor color;
    Vec2 from, pocket;
    float slide_time;   /* sim seconds to slide from the capture point to the pocket centre */
    float sink_time;    /* sim seconds spent sinking in the hole */
    float t;            /* sim seconds since the piece fell in */
} FallEffect;

static FallEffect falls[MAX_PIECES + 1];

#define RETURN_SLIDE_TIME EFFECTS_RETURN_SLIDE_TIME   /* seconds a returned coin takes to slide from the pocket to its place */
typedef struct { bool active; PieceColor color; Vec2 from, to; float t, dur, bend; bool grow; } ReturnEffect;   /* t < 0: still waiting at `from` */
static ReturnEffect returns[MAX_PIECES];

void effects_trigger_return(int id, PieceColor color, int from_pocket, Vec2 to) {
    if (id < 0 || id >= MAX_PIECES || from_pocket < 0 || from_pocket > 3) return;
    returns[id] = (ReturnEffect){ true, color, POCKET_CENTERS[from_pocket], to, 0.0f, RETURN_SLIDE_TIME, 0.0f, true };
}

void effects_trigger_slide(int id, PieceColor color, Vec2 from, Vec2 to, float delay, float duration, float bend) {
    if (id < 0 || id >= MAX_PIECES) return;
    returns[id] = (ReturnEffect){ true, color, from, to, -delay, duration, bend, false };
}

#define STRIKER_SLIDE_MAX_POINTS 40
typedef struct {
    bool active, done;
    Vec2 pts[STRIKER_SLIDE_MAX_POINTS];
    int n;
    float total, t, dur;
} StrikerSlide;
static StrikerSlide striker_slide;

float effects_trigger_striker_slide(const Vec2* pts, int n, float delay, float speed) {
    if (n < 2) return 0.0f;
    if (n > STRIKER_SLIDE_MAX_POINTS) n = STRIKER_SLIDE_MAX_POINTS;
    StrikerSlide* ss = &striker_slide;
    ss->active = true;
    ss->done = false;
    ss->n = n;
    ss->total = 0.0f;
    for (int i = 0; i < n; i++) {
        ss->pts[i] = pts[i];
        if (i > 0) { float dx = pts[i].x - pts[i - 1].x, dy = pts[i].y - pts[i - 1].y; ss->total += sqrtf(dx * dx + dy * dy); }
    }
    ss->dur = ss->total / (speed > 0.05f ? speed : 0.05f);
    if (ss->dur < 0.5f) ss->dur = 0.5f;
    if (ss->dur > 2.5f) ss->dur = 2.5f;
    ss->t = -delay;
    return ss->dur;
}

bool effects_striker_recovering(void) { return striker_slide.active; }

bool effects_take_striker_slide_done(Vec2* end_pos) {
    if (!striker_slide.done) return false;
    striker_slide.done = false;
    if (end_pos) *end_pos = striker_slide.pts[striker_slide.n - 1];
    return true;
}

bool effects_piece_returning(int id) { return id >= 0 && id < MAX_PIECES && returns[id].active; }

void effects_trigger_pocket_fall(int id, PieceColor color, Vec2 from, Vec2 vel, int pocket_index) {
    if (id < 0 || id > MAX_PIECES || pocket_index < 0 || pocket_index > 3) return;
    Vec2 pc = POCKET_CENTERS[pocket_index];
    float dx = pc.x - from.x, dy = pc.y - from.y;
    float dist = sqrtf(dx * dx + dy * dy);
    float speed = sqrtf(vel.x * vel.x + vel.y * vel.y);
    if (speed < 0.3f) speed = 0.3f;
    float slide = dist / speed;   /* keeps the speed the piece had, so it does not seem to accelerate */
    if (slide < 0.04f) slide = 0.04f;
    if (slide > 0.4f) slide = 0.4f;
    falls[id] = (FallEffect){ true, color, from, pc, slide, (id == EFFECTS_STRIKER_ID) ? STRIKER_SINK_TIME : POCKET_SINK_TIME, 0.0f };
}

void effects_update(float sim_dt) {
    if (striker_slide.active) {
        striker_slide.t += sim_dt;
        if (striker_slide.t >= striker_slide.dur) { striker_slide.active = false; striker_slide.done = true; }
    }
    for (int i = 0; i < MAX_PIECES; i++) {
        if (!returns[i].active) continue;
        returns[i].t += sim_dt;
        if (returns[i].t >= returns[i].dur) returns[i].active = false;
    }
    for (int i = 0; i <= MAX_PIECES; i++) {
        if (!falls[i].active) continue;
        falls[i].t += sim_dt;
        if (falls[i].t >= falls[i].slide_time + falls[i].sink_time) falls[i].active = false;
    }
}

bool effects_piece_falling(int id) {
    return id >= 0 && id <= MAX_PIECES && falls[id].active;
}

unsigned int effects_falling_mask(void) {
    unsigned int mask = 0;
    for (int i = 0; i <= MAX_PIECES; i++) if (falls[i].active) mask |= (1u << i);
    return mask;
}

void effects_reset(void) {
    striker_slide.active = false;
    striker_slide.done = false;
    for (int i = 0; i < MAX_PIECES; i++) returns[i].active = false;
    for (int i = 0; i <= MAX_PIECES; i++) falls[i].active = false;
}


void effects_draw(Viewport vp, const GameState* game, double placement_timer, const Layout* L) {
    double wall_time = GetTime();  // Wall time for animations
    
    (void)L;
    (void)wall_time;

    // Aim line and power bar during aiming/placement phase (when not in placement hold)
    if (game->phase == PHASE_AIMING) {
        Vec2 striker_pos = game->board.striker.position;
        Vec2 screen = math_world_to_screen(vp, striker_pos);
        
        float aim_angle = 0.0f;
        if (game->turn_seat == SEAT_NORTH) aim_angle = -M_PI / 2.0f;
        else if (game->turn_seat == SEAT_SOUTH) aim_angle = M_PI / 2.0f;
        else if (game->turn_seat == SEAT_EAST) aim_angle = M_PI;
        else aim_angle = 0.0f;
        
        float line_len = math_world_to_screen_dist(vp, 0.5f);
        Vec2 end = {
            screen.x + cosf(aim_angle) * line_len,
            screen.y + sinf(aim_angle) * line_len
        };
        DrawLine((int)screen.x, (int)screen.y, (int)end.x, (int)end.y, (Color){255, 255, 0, 150});
        
        float power = 0.5f;
        float bar_w = math_world_to_screen_dist(vp, 0.2f);
        float bar_h = math_world_to_screen_dist(vp, 0.02f);
        Vec2 bar_pos = { screen.x - bar_w * 0.5f, screen.y - bar_h - math_world_to_screen_dist(vp, 0.03f) };
        DrawRectangle((int)bar_pos.x, (int)bar_pos.y, (int)(bar_w * power), (int)bar_h, (Color){0, 255, 0, 200});
        DrawRectangleLines((int)bar_pos.x, (int)bar_pos.y, (int)bar_w, (int)bar_h, WHITE);
    }
    
    // Pieces / striker falling into a pocket
    for (int i = 0; i <= MAX_PIECES; i++) {
        const FallEffect* f = &falls[i];
        if (!f->active) continue;
        Vec2 pos;
        float scale = 1.0f, alpha = 1.0f;
        if (f->t < f->slide_time) {
            float u = f->t / f->slide_time;
            pos = (Vec2){ f->from.x + (f->pocket.x - f->from.x) * u, f->from.y + (f->pocket.y - f->from.y) * u };
        } else {
            float u = (f->t - f->slide_time) / f->sink_time;
            if (u > 1.0f) u = 1.0f;
            pos = f->pocket;
            if (i == EFFECTS_STRIKER_ID) {
                scale = 1.0f - 0.8f * u;                       /* a slow drop down the hole... */
                alpha = (u < 0.55f) ? 1.0f : 1.0f - (u - 0.55f) / 0.45f;   /* ...then out of sight */
            } else {
                scale = 1.0f - 0.7f * u;      /* coins shrink as they drop into the hole */
                alpha = 1.0f - u;
            }
        }
        Vec2 sp = math_world_to_screen(vp, pos);
        float r = ((i == EFFECTS_STRIKER_ID) ? L->striker_r_px : L->piece_r_px) * scale;
        Color col = (f->color == PIECE_WHITE) ? (Color){ 240, 240, 240, 255 }
                  : (f->color == PIECE_BLACK) ? (Color){ 62, 64, 74, 255 }
                  : (f->color == PIECE_QUEEN) ? (Color){ 220, 30, 30, 255 }
                  : (Color){ 255, 215, 0, 255 };
        col.a = (unsigned char)(255.0f * alpha);
        DrawCircle((int)sp.x, (int)sp.y, r, col);
        DrawCircleLines((int)sp.x, (int)sp.y, r, (f->color == PIECE_BLACK) ? (Color){ 200, 205, 215, col.a } : (Color){ 20, 20, 20, col.a });
    }

    // The pocketed striker sliding back to the next player, around the coins (ease in and out along its route)
    if (striker_slide.active && striker_slide.t >= 0.0f) {
        const StrikerSlide* ss = &striker_slide;
        float u = ss->t / ss->dur;
        if (u > 1.0f) u = 1.0f;
        float d = u * u * (3.0f - 2.0f * u) * ss->total;          /* distance travelled along the route */
        Vec2 pos = ss->pts[ss->n - 1];
        for (int i = 1; i < ss->n; i++) {
            float dx = ss->pts[i].x - ss->pts[i - 1].x, dy = ss->pts[i].y - ss->pts[i - 1].y;
            float seg = sqrtf(dx * dx + dy * dy);
            if (d <= seg || i == ss->n - 1) {
                float k = seg > 1e-6f ? (d > seg ? 1.0f : d / seg) : 1.0f;
                pos = (Vec2){ ss->pts[i - 1].x + dx * k, ss->pts[i - 1].y + dy * k };
                break;
            }
            d -= seg;
        }
        float fade = ss->t / 0.25f;                                /* it climbs out of the pocket */
        if (fade > 1.0f) fade = 1.0f;
        Vec2 sp = math_world_to_screen(vp, pos);
        DrawCircle((int)sp.x, (int)sp.y, L->striker_r_px, (Color){ 255, 215, 0, (unsigned char)(255.0f * fade) });
        DrawCircleLines((int)sp.x, (int)sp.y, L->striker_r_px, (Color){ 255, 255, 255, (unsigned char)(100.0f * fade) });
    }

    // Coins sliding back onto the board
    for (int i = 0; i < MAX_PIECES; i++) {
        const ReturnEffect* re = &returns[i];
        if (!re->active) continue;
        float u = (re->t < 0.0f ? 0.0f : re->t) / re->dur;
        if (u > 1.0f) u = 1.0f;
        float s = u * u * (3.0f - 2.0f * u);                     /* ease in and out */
        Vec2 pos = { re->from.x + (re->to.x - re->from.x) * s, re->from.y + (re->to.y - re->from.y) * s };
        if (re->bend != 0.0f) {                                  /* a curved path: sideways by bend x the distance at mid-way */
            float side = re->bend * sinf((float)M_PI * s);
            pos.x += -(re->to.y - re->from.y) * side;
            pos.y += (re->to.x - re->from.x) * side;
        }
        Vec2 sp = math_world_to_screen(vp, pos);
        float r = L->piece_r_px * (re->grow ? 0.7f + 0.3f * s : 1.0f);
        Color col = (re->color == PIECE_WHITE) ? (Color){ 240, 240, 240, 255 }
                  : (re->color == PIECE_BLACK) ? (Color){ 62, 64, 74, 255 }
                  : (Color){ 220, 30, 30, 255 };
        DrawCircle((int)sp.x, (int)sp.y, r, col);
        DrawCircleLines((int)sp.x, (int)sp.y, r, (re->color == PIECE_BLACK) ? (Color){ 200, 205, 215, 255 } : (Color){ 50, 50, 50, 230 });
    }

    // Pocket fade effects for recently pocketed pieces
    for (int i = 0; i < 4; i++) {
        if (pocket_fades[i].active) {
            pocket_fades[i].timer -= GetFrameTime();
            if (pocket_fades[i].timer <= 0) {
                pocket_fades[i].active = false;
            } else {
                float alpha = pocket_fades[i].timer / pocket_fades[i].duration;
                float r = math_world_to_screen_dist(vp, POCKET_RADIUS_NORM * 1.8f);
                Vec2 p = math_world_to_screen(vp, POCKET_CENTERS[i]);
                DrawCircle((int)p.x, (int)p.y, r, (Color){255, 255, 0, (unsigned char)(alpha * 170)});
            }
        }
    }
}

void effects_trigger_pocket_fade_long(int pocket_index, float seconds) {
    if (pocket_index >= 0 && pocket_index < 4) {
        pocket_fades[pocket_index].pocket_center = POCKET_CENTERS[pocket_index];
        pocket_fades[pocket_index].timer = seconds;
        pocket_fades[pocket_index].duration = seconds;
        pocket_fades[pocket_index].active = true;
    }
}

void effects_trigger_pocket_fade(int pocket_index) {
    if (pocket_index >= 0 && pocket_index < 4) {
        pocket_fades[pocket_index].pocket_center = POCKET_CENTERS[pocket_index];
        pocket_fades[pocket_index].timer = MAX_POCKET_FADE_TIME;
        pocket_fades[pocket_index].duration = MAX_POCKET_FADE_TIME;
        pocket_fades[pocket_index].active = true;
    }
}

/* flight_dump: decode a Carrom Arena flight-recorder file (flight_<seed>.bin) into text.
 *   flight_dump FILE                 events, notes and one summary line per frame
 *   flight_dump FILE --events        events and notes only
 *   flight_dump FILE --frame N       every field of frame number N (all pieces, striker, players, aim line, layout)
 *   flight_dump FILE --csv           one CSV row per frame: time, phase, striker, then x,y,vx,vy,flags of every piece
 *   flight_dump FILE --from T --to T restrict to wall-clock seconds
 */
#include "telemetry/flight.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    int mode;            /* 0 all, 1 events only, 2 single frame, 3 csv */
    uint64_t want_frame;
    double from, to;
    int frames, events, texts;
    bool header_done;
} Opts;

static void print_frame_summary(const FlightFrame* f) {
    int pocketed = 0, falling = 0;
    for (int i = 0; i < FLIGHT_PIECES; i++) {
        if (f->piece[i].flags & 2) pocketed++;
        if (f->piece[i].flags & 8) falling++;
    }
    printf("F %8llu t=%9.3f sim=%7.3f dt=%5.1fms %-14s seat=%d spd=%.2f a=%.2f  striker(%.3f,%.3f v=%.2f,%.2f vis=%.3f,%.3f)%s%s  pocketed=%d falling=%d\n",
           (unsigned long long)f->frame, f->wall, f->sim_time, f->frame_dt * 1000.0f, flight_phase_name(f->phase), f->turn_seat,
           f->playback_speed, f->alpha, f->striker_pos[0], f->striker_pos[1], f->striker_vel[0], f->striker_vel[1],
           f->striker_vis[0], f->striker_vis[1], (f->flags & 1) ? " PAUSED" : "", (f->flags & 2) ? " AIM" : "", pocketed, falling);
}

static void print_frame_full(const FlightFrame* f) {
    print_frame_summary(f);
    printf("  timers: placement=%.3f thinking=%.3f aim=%.3f  score W=%u B=%u  flags=0x%02x n_falling=%u\n",
           f->placement_timer, f->thinking_timer, f->aim_timer, f->score_white, f->score_black, f->flags, f->n_falling);
    printf("  players (baseline coord N,E,S,W as drawn): %.4f %.4f %.4f %.4f\n", f->figures[0], f->figures[1], f->figures[2], f->figures[3]);
    printf("  aim: angle=%.4f power=%.3f  drawn line (%.4f,%.4f) -> (%.4f,%.4f)\n", f->aim_angle, f->aim_power,
           f->aim_line[0], f->aim_line[1], f->aim_line[2], f->aim_line[3]);
    printf("  layout: board_x=%.0f board_y=%.0f board_size=%.0f window=%.0fx%.0f\n", f->layout[0], f->layout[1], f->layout[2], f->layout[3], f->layout[4]);
    for (int i = 0; i < FLIGHT_PIECES; i++) {
        const FlightPiece* p = &f->piece[i];
        printf("  piece %2d (%s) pos=(%.4f,%.4f) vel=(%.4f,%.4f) game[on_board=%d pocketed=%d] physics_alive=%d falling=%d drawn_from_physics=%d\n",
               i, i < 9 ? "white" : (i < 18 ? "black" : "queen"), p->x, p->y, p->vx, p->vy,
               p->flags & 1, (p->flags >> 1) & 1, (p->flags >> 2) & 1, (p->flags >> 3) & 1, (p->flags >> 4) & 1);
    }
}

static void visit(void* user, int type, const void* payload, size_t len) {
    Opts* o = (Opts*)user;
    if (type == FLIGHT_REC_FRAME && len == sizeof(FlightFrame)) {
        FlightFrame f;
        memcpy(&f, payload, sizeof(f));
        if (f.wall < o->from || f.wall > o->to) return;
        o->frames++;
        if (o->mode == 0) print_frame_summary(&f);
        else if (o->mode == 2 && f.frame == o->want_frame) print_frame_full(&f);
        else if (o->mode == 3) {
            if (!o->header_done) {
                printf("wall,frame,sim,phase,seat,speed,striker_x,striker_y,striker_vx,striker_vy,vis_x,vis_y");
                for (int i = 0; i < FLIGHT_PIECES; i++) printf(",p%d_x,p%d_y,p%d_vx,p%d_vy,p%d_flags", i, i, i, i, i);
                printf("\n");
                o->header_done = true;
            }
            printf("%.4f,%llu,%.4f,%d,%d,%.2f,%.5f,%.5f,%.5f,%.5f,%.5f,%.5f", f.wall, (unsigned long long)f.frame, f.sim_time,
                   f.phase, f.turn_seat, f.playback_speed, f.striker_pos[0], f.striker_pos[1], f.striker_vel[0], f.striker_vel[1],
                   f.striker_vis[0], f.striker_vis[1]);
            for (int i = 0; i < FLIGHT_PIECES; i++)
                printf(",%.5f,%.5f,%.5f,%.5f,%u", f.piece[i].x, f.piece[i].y, f.piece[i].vx, f.piece[i].vy, f.piece[i].flags);
            printf("\n");
        }
    } else if (type == FLIGHT_REC_EVENT && len == sizeof(FlightEvent)) {
        FlightEvent e;
        memcpy(&e, payload, sizeof(e));
        if (e.wall < o->from || e.wall > o->to) return;
        o->events++;
        if (o->mode == 0 || o->mode == 1) {
            printf("E t=%9.3f sim=%7.3f %-14s a=%.4f b=%.4f c=%.4f d=%.4f", e.wall, e.sim_time, flight_event_name(e.kind), e.a, e.b, e.c, e.d);
            if (e.kind == FLIGHT_EV_PHASE) printf("   (%s -> %s)", flight_phase_name((int)e.a), flight_phase_name((int)e.b));
            printf("\n");
        }
    } else if (type == FLIGHT_REC_TEXT && len > sizeof(double)) {
        double w;
        memcpy(&w, payload, sizeof(w));
        if (w < o->from || w > o->to) return;
        o->texts++;
        if (o->mode == 0 || o->mode == 1) printf("T t=%9.3f %s\n", w, (const char*)payload + sizeof(double));
    }
}

int main(int argc, char** argv) {
    if (argc < 2) {
        fprintf(stderr, "usage: %s FILE [--events | --frame N | --csv] [--from T] [--to T]\n", argv[0]);
        return 2;
    }
    Opts o = { 0, 0, -1e300, 1e300, 0, 0, 0, false };
    int i = 2;
    while (i < argc) {
        const char* arg_cur = argv[i++];
        if (!strcmp(arg_cur, "--events")) o.mode = 1;
        else if (!strcmp(arg_cur, "--csv")) o.mode = 3;
        else if (!strcmp(arg_cur, "--frame") && i < argc) { o.mode = 2; o.want_frame = strtoull(argv[i++], NULL, 10); }
        else if (!strcmp(arg_cur, "--from") && i < argc) o.from = atof(argv[i++]);
        else if (!strcmp(arg_cur, "--to") && i < argc) o.to = atof(argv[i++]);
    }
    uint64_t total = 0, seed = 0;
    int n = flight_read(argv[1], visit, &o, &total, &seed);
    if (n < 0) { fprintf(stderr, "cannot read %s (not a flight recorder file?)\n", argv[1]); return 1; }
    fprintf(stderr, "flight file: seed=%llu, %llu bytes written (%s), %d records read: %d frames, %d events, %d notes\n",
            (unsigned long long)seed, (unsigned long long)total, total > FLIGHT_RING_SIZE ? "ring wrapped, oldest data overwritten" : "complete",
            n, o.frames, o.events, o.texts);
    return 0;
}

#include "unity.h"
#include "telemetry/flight.h"
#include "render/piece_draw.h"
#include <stdio.h>
#include <string.h>
#include <math.h>

void setUp(void) {}
void tearDown(void) {}

typedef struct { int frames, events, texts; uint64_t first_frame, last_frame, prev_frame; bool ordered; float last_x; } Tally;

static void visit(void* user, int type, const void* payload, size_t len) {
    Tally* t = (Tally*)user;
    if (type == FLIGHT_REC_FRAME) {
        TEST_ASSERT_EQUAL_UINT((unsigned)sizeof(FlightFrame), (unsigned)len);
        FlightFrame f;
        memcpy(&f, payload, sizeof(f));
        if (t->frames == 0) t->first_frame = f.frame;
        else if (f.frame != t->prev_frame + 1) t->ordered = false;
        t->prev_frame = f.frame;
        t->last_frame = f.frame;
        t->last_x = f.piece[3].x;
        t->frames++;
    } else if (type == FLIGHT_REC_EVENT) t->events++;
    else if (type == FLIGHT_REC_TEXT) t->texts++;
}

static void fill(FlightFrame* f, uint64_t n) {
    memset(f, 0, sizeof(*f));
    f->frame = n;
    f->wall = (double)n / 60.0;
    f->sim_time = (float)n / 60.0f;
    f->piece[3].x = (float)n;
    f->piece[3].y = -(float)n;
    f->phase = (uint8_t)(n % 8);
}

void test_flight_roundtrip_without_wrap(void) {
    const char* path = "test_flight_small.bin";
    FlightRecorder* fr = flight_open(path, 4242);
    TEST_ASSERT_NOT_NULL(fr);
    FlightFrame f;
    for (uint64_t i = 0; i < 100; i++) {
        fill(&f, i);
        flight_write_frame(fr, &f);
        if (i == 10) flight_write_event(fr, 0.2, 0.1f, FLIGHT_EV_POCKET, 5, 2, 1.5f, 3.0f);
        if (i == 20) flight_write_text(fr, 0.4, "hello flight");
    }
    flight_close(fr);
    Tally t = {0, 0, 0, 0, 0, 0, true, 0};
    uint64_t total = 0, seed = 0;
    int n = flight_read(path, visit, &t, &total, &seed);
    remove(path);
    TEST_ASSERT_EQUAL_INT(102, n);
    TEST_ASSERT_EQUAL_INT(100, t.frames);
    TEST_ASSERT_EQUAL_INT(1, t.events);
    TEST_ASSERT_EQUAL_INT(1, t.texts);
    TEST_ASSERT_TRUE(t.ordered);
    TEST_ASSERT_EQUAL_UINT64(0, t.first_frame);
    TEST_ASSERT_EQUAL_UINT64(99, t.last_frame);
    TEST_ASSERT_EQUAL_UINT64(4242, seed);
    TEST_ASSERT_FLOAT_WITHIN(1e-3f, 99.0f, t.last_x);
}

void test_flight_ring_wraps_and_keeps_the_newest(void) {
    const char* path = "test_flight_wrap.bin";
    FlightRecorder* fr = flight_open(path, 1);
    TEST_ASSERT_NOT_NULL(fr);
    FlightFrame f;
    const uint64_t N = 30000;   /* about 14 MB of frames: the 8 MiB ring must wrap */
    for (uint64_t i = 0; i < N; i++) {
        fill(&f, i);
        flight_write_frame(fr, &f);
        if (i % 1000 == 0) flight_write_event(fr, f.wall, f.sim_time, FLIGHT_EV_PHASE, 1, 2, 0, 0);
    }
    flight_close(fr);
    Tally t = {0, 0, 0, 0, 0, 0, true, 0};
    uint64_t total = 0;
    int n = flight_read(path, visit, &t, &total, NULL);
    remove(path);
    TEST_ASSERT_TRUE(total > FLIGHT_RING_SIZE);
    TEST_ASSERT_TRUE(n > 1000);
    TEST_ASSERT_TRUE(t.first_frame > 0);                 /* oldest frames were overwritten */
    TEST_ASSERT_EQUAL_UINT64(N - 1, t.last_frame);       /* newest frame survived */
    TEST_ASSERT_TRUE(t.ordered);                          /* consecutive, no corrupt record in between */
    TEST_ASSERT_TRUE((uint64_t)t.frames * sizeof(FlightFrame) <= FLIGHT_RING_SIZE);
}

void test_stash_coins_never_overlap_and_sit_outside_the_board(void) {
    for (int p = 0; p < 4; p++) {
        for (int i = 0; i < 9; i++) {
            Vec2 a = pocket_stash_position(p, i);
            TEST_ASSERT_TRUE(fabsf(a.x) > 0.5f && fabsf(a.y) > 0.5f);            /* outside the board corner */
            TEST_ASSERT_TRUE((POCKET_CENTERS[p].x < 0) == (a.x < 0));             /* on the pocket's own side */
            TEST_ASSERT_TRUE((POCKET_CENTERS[p].y < 0) == (a.y < 0));
            for (int j = i + 1; j < 9; j++) {
                Vec2 b = pocket_stash_position(p, j);
                TEST_ASSERT_TRUE(hypotf(a.x - b.x, a.y - b.y) >= 2.0f * PIECE_RADIUS_NORM - 1e-5f);
            }
        }
    }
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_flight_roundtrip_without_wrap);
    RUN_TEST(test_flight_ring_wraps_and_keeps_the_newest);
    RUN_TEST(test_stash_coins_never_overlap_and_sit_outside_the_board);
    return UNITY_END();
}

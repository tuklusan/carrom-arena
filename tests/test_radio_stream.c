/* The radio's reconnect behaviour, against a fake network source (no sockets, no audio device). */
#include "unity.h"
#include "audio/radio_stream.h"
#include "platform/platform.h"

void setUp(void) {}
void tearDown(void) {}

static volatile int g_opens;
static volatile int g_fail_all;      /* every open fails */
static volatile int g_reads_ok;      /* reads that deliver data before the connection breaks */

static void* fake_open(void* user, const char* url) {
    (void)user; (void)url;
    g_opens++;
    if (g_fail_all) return NULL;
    static int handle;
    return &handle;
}
static int fake_read(void* h, uint8_t* buf, int max) {
    (void)h;
    static volatile int calls;
    if (calls++ % (g_reads_ok + 1) == g_reads_ok) return -1;    /* the connection breaks */
    int n = (max < 600) ? max : 600;
    for (int i = 0; i < n; i++) buf[i] = (uint8_t)(i * 7);      /* not an MP3: decodes to nothing */
    platform_sleep_ms(2);
    return n;
}
static void fake_close(void* h) { (void)h; }

static bool wait_for(int (*pred)(RadioStream*), RadioStream* rs, int ms) {
    for (int t = 0; t < ms; t += 5) { if (pred(rs)) return true; platform_sleep_ms(5); }
    return false;
}
static int is_playing(RadioStream* rs) { return rs_status(rs) == RS_PLAYING; }
static int is_failed(RadioStream* rs) { return rs_status(rs) == RS_FAILED; }
static int is_idle(RadioStream* rs) { return rs_status(rs) == RS_IDLE; }
static int reopened(RadioStream* rs) { (void)rs; return g_opens >= 3; }

void test_failure_is_retried_until_paused(void) {
    g_opens = 0; g_fail_all = 0; g_reads_ok = 3;
    const char* urls[] = { "fake://a", "fake://b" };
    RsSource src = { fake_open, fake_read, fake_close, NULL };
    RadioStream* rs = rs_create(urls, 2, &src, 30);
    TEST_ASSERT_NOT_NULL(rs);
    TEST_ASSERT_EQUAL_INT(RS_IDLE, rs_status(rs));           /* nothing happens until it is switched on */
    platform_sleep_ms(100);
    TEST_ASSERT_EQUAL_INT(0, g_opens);

    rs_set_active(rs, true);
    TEST_ASSERT_TRUE(wait_for(is_playing, rs, 2000));        /* data flows */
    TEST_ASSERT_TRUE(wait_for(is_failed, rs, 2000));         /* the connection breaks: failed... */
    TEST_ASSERT_TRUE(wait_for(reopened, rs, 3000));          /* ...and it reconnects by itself, again and again */

    rs_set_active(rs, false);                                 /* the player pauses */
    TEST_ASSERT_TRUE(wait_for(is_idle, rs, 1000));
    platform_sleep_ms(150);                                   /* let a running attempt end */
    int opens_at_pause = g_opens;
    platform_sleep_ms(300);
    TEST_ASSERT_EQUAL_INT(opens_at_pause, g_opens);          /* no reconnect attempts while paused */
    rs_destroy(rs);
}

void test_unreachable_stream_keeps_trying(void) {
    g_opens = 0; g_fail_all = 1; g_reads_ok = 3;
    const char* urls[] = { "fake://a", "fake://b", "fake://c" };
    RsSource src = { fake_open, fake_read, fake_close, NULL };
    RadioStream* rs = rs_create(urls, 3, &src, 20);
    rs_set_active(rs, true);
    TEST_ASSERT_TRUE(wait_for(reopened, rs, 3000));
    TEST_ASSERT_TRUE(rs_status(rs) == RS_FAILED || rs_status(rs) == RS_CONNECTING);
    g_fail_all = 0;                                           /* the network comes back: it plays again, unprompted */
    TEST_ASSERT_TRUE(wait_for(is_playing, rs, 3000));
    rs_destroy(rs);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_failure_is_retried_until_paused);
    RUN_TEST(test_unreachable_stream_keeps_trying);
    return UNITY_END();
}

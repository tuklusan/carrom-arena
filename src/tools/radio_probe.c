/* radio_probe: connect to the AH.FM stream for a few seconds, decode it, and report what arrived (network + MP3 check).
 *   radio_probe [seconds] */
#include "audio/radio_stream.h"
#include "platform/platform.h"
#include <stdio.h>
#include <stdlib.h>

int main(int argc, char** argv) {
    int seconds = (argc > 1) ? atoi(argv[1]) : 8;
    const char* urls[] = { "https://eu.ah.fm/live", "https://us.ah.fm/live", "https://fr.ah.fm/live" };
    RadioStream* rs = rs_create(urls, 3, NULL, 3000);
    if (!rs) { printf("cannot start the stream worker\n"); return 1; }
    rs_set_active(rs, true);
    static int16_t buf[4096 * 2];
    size_t total = 0;
    long long peak = 0;
    for (int t = 0; t < seconds * 20; t++) {
        platform_sleep_ms(50);
        size_t n;
        while ((n = rs_read(rs, buf, 4096)) > 0) {
            total += n;
            for (size_t i = 0; i < n * 2; i++) { long long v = buf[i] < 0 ? -buf[i] : buf[i]; if (v > peak) peak = v; }
        }
    }
    printf("status=%d hz=%d frames_decoded=%zu (%.1f s of audio) peak=%lld\n", (int)rs_status(rs), rs_sample_rate(rs), total,
           rs_sample_rate(rs) ? (double)total / rs_sample_rate(rs) : 0.0, peak);
    rs_destroy(rs);
    return total > 0 ? 0 : 2;
}

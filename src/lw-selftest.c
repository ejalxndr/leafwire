#define LW_LOG_PREFIX "lw-selftest"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "capture.h"
#include "log.h"
#include "leafwire.h"
#include "zone.h"

static void sleep_ms(unsigned ms)
{
    struct timespec ts;
    ts.tv_sec = (time_t)(ms / 1000u);
    ts.tv_nsec = (long)(ms % 1000u) * 1000000L;
    while (nanosleep(&ts, &ts) == -1)
        ;
}

static int run_capture(void)
{
    char errbuf[LW_ERRBUF];
    struct lw_capture *cap = NULL;
    lw_status st;
    int frame;

    st = lw_capture_new(&cap, getenv("DISPLAY"), errbuf);
    if (st != LW_OK) {
        lw_err("capture new: %s", errbuf);
        return 1;
    }

    for (frame = 0; frame < 3; frame++) {
        struct lw_zone_color zc[256];
        const uint8_t *data;
        size_t len = 0;
        size_t n;
        size_t i;

        if (!lw_capture_grab(cap, 0.9f)) {
            lw_err("grab failed");
            lw_capture_free(cap);
            return 1;
        }
        data = lw_capture_data(cap, &len);
        printf("frame %d: %dx%d bpp=%d bytes=%zu\n", frame, lw_capture_width(cap),
               lw_capture_height(cap), lw_capture_bpp(cap), len);

        n = lw_zone_analyze(10, data, len, lw_capture_width(cap), lw_capture_height(cap),
                            lw_capture_bpp(cap), 10, 10, 10, 10, zc, 256);
        printf("  %zu zones; first 3:", n);
        for (i = 0; i < n && i < 3; i++)
            printf(" (%.2f,%.2f,%.2f)", (double)zc[i].r, (double)zc[i].g,
                   (double)zc[i].b);
        printf("\n");
        sleep_ms(100);
    }

    lw_capture_free(cap);
    return 0;
}

static int run_anim(struct lw_device *dev, struct lw_anim *anim, unsigned seconds)
{
    char errbuf[LW_ERRBUF];
    unsigned elapsed = 0;

    if (!anim) {
        lw_err("could not create animation");
        return 1;
    }

    while (elapsed < seconds * 1000u) {
        uint32_t delay = 50;
        lw_status st = anim->run(anim, dev, &delay, errbuf);
        if (st != LW_OK) {
            lw_err("frame failed: %s", errbuf);
            anim->destroy(anim);
            return 1;
        }
        if (delay == 0)
            delay = 1;
        sleep_ms(delay);
        elapsed += delay;
    }
    anim->destroy(anim);
    return 0;
}

int main(int argc, char **argv)
{
    char errbuf[LW_ERRBUF];
    struct lw_device *dev = NULL;
    lw_status st;
    int rc = 0;

    if (argc >= 2 && strcmp(argv[1], "capture") == 0)
        return run_capture();

    st = lw_device_open(&dev, errbuf);
    if (st != LW_OK) {
        lw_err("open: %s", errbuf);
        return 1;
    }
    printf("zone count: %zu\n", lw_device_zone_count(dev));

    st = lw_device_initialize(dev, errbuf);
    if (st != LW_OK) {
        lw_err("initialize: %s", errbuf);
        lw_device_close(dev);
        return 1;
    }

    if (argc >= 3 && strcmp(argv[1], "anim") == 0) {
        unsigned seconds = (argc >= 4) ? (unsigned)strtoul(argv[3], NULL, 10) : 5;
        struct lw_anim *anim = NULL;
        struct lw_color white = { 255, 255, 255 };

        if (strcmp(argv[2], "solid") == 0)
            anim = lw_anim_solid(white);
        else if (strcmp(argv[2], "breathing") == 0)
            anim = lw_anim_breathing(white, 3000, 500);
        else if (strcmp(argv[2], "wave") == 0)
            anim = lw_anim_wave(white, 2000, 50);
        else if (strcmp(argv[2], "rainbow") == 0)
            anim = lw_anim_rainbow(5000, 100);
        else {
            lw_err("unknown anim '%s'", argv[2]);
            lw_device_close(dev);
            return 2;
        }
        rc = run_anim(dev, anim, seconds);
    } else {
        struct lw_color buf[256];
        struct lw_color c = { 255, 255, 255 };
        size_t n = lw_device_zone_count(dev);
        size_t i;

        if (argc >= 4) {
            c.r = (uint8_t)strtoul(argv[1], NULL, 10);
            c.g = (uint8_t)strtoul(argv[2], NULL, 10);
            c.b = (uint8_t)strtoul(argv[3], NULL, 10);
        }
        if (n > 256)
            n = 256;
        for (i = 0; i < n; i++)
            buf[i] = c;

        st = lw_device_set_colors(dev, buf, n, errbuf);
        if (st != LW_OK) {
            lw_err("set_colors: %s", errbuf);
            rc = 1;
        } else {
            printf("holding color for 2s...\n");
            sleep_ms(2000);
        }
        memset(buf, 0, sizeof(buf));
        lw_device_set_colors(dev, buf, n, errbuf);
    }

    lw_device_close(dev);
    lw_lib_shutdown();
    return rc;
}

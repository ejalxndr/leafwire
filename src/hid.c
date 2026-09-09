#include "leafwire.h"

#include <hidapi.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <wchar.h>

#include "log.h"

#define LW_VENDOR_ID  0x37FAu
#define LW_PRODUCT_ID 0x8202u

#define LW_REPORT_SIZE 65
#define LW_READ_SIZE   64

#define LW_CMD_ZONE_COUNT 0x03
#define LW_CMD_RGB_DATA   0x02
#define LW_CMD_INIT_MODE  0x07
#define LW_CMD_BRIGHTNESS 0x09

#define LW_GRB_ORDER_LIMIT 20

#define LW_RGB_PKT1_MAX 60
#define LW_RGB_PKT2_MAX 64
#define LW_RGB_PKT3_MAX 38
#define LW_RGB_WIRE_MAX (124 + LW_RGB_PKT3_MAX)

#define LW_WRITE_SETTLE_MS 100
#define LW_READ_TIMEOUT_MS 200

struct lw_device {
    hid_device *dev;
    size_t zone_count;
};

static int hid_ready;

static void sleep_ms(unsigned ms)
{
    struct timespec ts;
    ts.tv_sec = (time_t)(ms / 1000u);
    ts.tv_nsec = (long)(ms % 1000u) * 1000000L;
    while (nanosleep(&ts, &ts) == -1)
        ;
}

static void copy_hid_error(hid_device *dev, char errbuf[LW_ERRBUF])
{
    const wchar_t *w = hid_error(dev);
    if (w)
        snprintf(errbuf, LW_ERRBUF, "%ls", w);
    else
        snprintf(errbuf, LW_ERRBUF, "unknown HID error");
}

static lw_status send_command(struct lw_device *d, unsigned char cmd,
                              const unsigned char *data, size_t dlen,
                              unsigned char *resp, size_t resp_cap,
                              char errbuf[LW_ERRBUF])
{
    unsigned char buf[LW_REPORT_SIZE];
    unsigned char rbuf[LW_READ_SIZE];
    int n;

    if (dlen > LW_REPORT_SIZE - 4) {
        snprintf(errbuf, LW_ERRBUF, "command payload too large (%zu)", dlen);
        return LW_ERR_PROTO;
    }

    memset(buf, 0, sizeof(buf));
    buf[1] = cmd;
    buf[2] = (unsigned char)((dlen >> 8) & 0xFFu);
    buf[3] = (unsigned char)(dlen & 0xFFu);
    if (dlen > 0)
        memcpy(buf + 4, data, dlen);

    if (hid_write(d->dev, buf, sizeof(buf)) < 0) {
        copy_hid_error(d->dev, errbuf);
        return LW_ERR_HID_IO;
    }

    sleep_ms(LW_WRITE_SETTLE_MS);

    n = hid_read_timeout(d->dev, rbuf, sizeof(rbuf), LW_READ_TIMEOUT_MS);
    if (n < 0) {
        copy_hid_error(d->dev, errbuf);
        return LW_ERR_HID_IO;
    }

    if (resp && resp_cap > 0 && n > 0) {
        size_t len = (size_t)n;
        if (len > resp_cap)
            len = resp_cap;
        if (len > sizeof(rbuf))
            len = sizeof(rbuf);
        memcpy(resp, rbuf, len);
    }
    return LW_OK;
}

static lw_status query_zone_count(struct lw_device *d, char errbuf[LW_ERRBUF])
{
    unsigned char resp[LW_READ_SIZE];
    lw_status st;

    memset(resp, 0, sizeof(resp));
    st = send_command(d, LW_CMD_ZONE_COUNT, NULL, 0, resp, sizeof(resp), errbuf);
    if (st != LW_OK)
        return st;

    d->zone_count = resp[4];
    return LW_OK;
}

static lw_status write_rgb_data(struct lw_device *d, const unsigned char *rgb,
                                size_t len, char errbuf[LW_ERRBUF])
{
    unsigned char buf[LW_REPORT_SIZE];
    size_t n;

    memset(buf, 0, sizeof(buf));
    buf[1] = LW_CMD_RGB_DATA;
    buf[2] = (unsigned char)((len >> 8) & 0xFFu);
    buf[3] = (unsigned char)(len & 0xFFu);
    n = len < LW_RGB_PKT1_MAX ? len : LW_RGB_PKT1_MAX;
    memcpy(buf + 4, rgb, n);
    if (hid_write(d->dev, buf, sizeof(buf)) < 0) {
        copy_hid_error(d->dev, errbuf);
        return LW_ERR_HID_IO;
    }

    memset(buf, 0, sizeof(buf));
    if (len > 60) {
        n = len - 60;
        if (n > LW_RGB_PKT2_MAX)
            n = LW_RGB_PKT2_MAX;
        memcpy(buf + 1, rgb + 60, n);
    }
    if (hid_write(d->dev, buf, sizeof(buf)) < 0) {
        copy_hid_error(d->dev, errbuf);
        return LW_ERR_HID_IO;
    }

    memset(buf, 0, sizeof(buf));
    if (len > 124) {
        n = len - 124;
        if (n > LW_RGB_PKT3_MAX)
            n = LW_RGB_PKT3_MAX;
        memcpy(buf + 1, rgb + 124, n);
    }
    if (hid_write(d->dev, buf, sizeof(buf)) < 0) {
        copy_hid_error(d->dev, errbuf);
        return LW_ERR_HID_IO;
    }

    return LW_OK;
}

lw_status lw_device_open(struct lw_device **out, char errbuf[LW_ERRBUF])
{
    struct lw_device *d;
    lw_status st;

    *out = NULL;

    if (!hid_ready) {
        if (hid_init() != 0) {
            snprintf(errbuf, LW_ERRBUF, "hid_init failed");
            return LW_ERR_HID_INIT;
        }
        hid_ready = 1;
    }

    d = calloc(1, sizeof(*d));
    if (!d) {
        snprintf(errbuf, LW_ERRBUF, "out of memory");
        return LW_ERR_NOMEM;
    }

    d->dev = hid_open(LW_VENDOR_ID, LW_PRODUCT_ID, NULL);
    if (!d->dev) {
        snprintf(errbuf, LW_ERRBUF,
                 "could not open HID device %04x:%04x (permissions? not connected?)",
                 LW_VENDOR_ID, LW_PRODUCT_ID);
        free(d);
        return LW_ERR_HID_OPEN;
    }

    st = query_zone_count(d, errbuf);
    if (st != LW_OK) {
        hid_close(d->dev);
        free(d);
        return st;
    }

    *out = d;
    return LW_OK;
}

void lw_device_close(struct lw_device *d)
{
    if (!d)
        return;
    if (d->dev)
        hid_close(d->dev);
    free(d);
}

size_t lw_device_zone_count(const struct lw_device *d)
{
    return d->zone_count;
}

lw_status lw_device_initialize(struct lw_device *d, char errbuf[LW_ERRBUF])
{
    static const unsigned char on = 1;
    static const unsigned char full = 255;
    lw_status st;

    st = send_command(d, LW_CMD_INIT_MODE, &on, 1, NULL, 0, errbuf);
    if (st != LW_OK)
        return st;
    return send_command(d, LW_CMD_BRIGHTNESS, &full, 1, NULL, 0, errbuf);
}

lw_status lw_device_set_colors(struct lw_device *d, const struct lw_color *colors,
                               size_t n, char errbuf[LW_ERRBUF])
{
    unsigned char rgb[LW_RGB_WIRE_MAX];
    size_t i;
    size_t wire;
    static int overflow_warned;

    if (n > sizeof(rgb) / 3) {
        if (!overflow_warned) {
            lw_warn("zone count %zu exceeds wire capacity %d; extra zones ignored",
                    n, LW_RGB_WIRE_MAX / 3);
            overflow_warned = 1;
        }
        n = sizeof(rgb) / 3;
    }

    for (i = 0; i < n; i++) {
        struct lw_color c = colors[i];
        if (i < LW_GRB_ORDER_LIMIT) {
            rgb[i * 3 + 0] = c.g;
            rgb[i * 3 + 1] = c.r;
            rgb[i * 3 + 2] = c.b;
        } else {
            rgb[i * 3 + 0] = c.r;
            rgb[i * 3 + 1] = c.b;
            rgb[i * 3 + 2] = c.g;
        }
    }

    wire = n * 3;
    return write_rgb_data(d, rgb, wire, errbuf);
}

void lw_lib_shutdown(void)
{
    if (hid_ready) {
        hid_exit();
        hid_ready = 0;
    }
}

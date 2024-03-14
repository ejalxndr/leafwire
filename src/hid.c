#include "nlctl.h"

#include <hidapi.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <wchar.h>

#include "log.h"

#define NL_VENDOR_ID  0x37FAu
#define NL_PRODUCT_ID 0x8202u

#define NL_REPORT_SIZE 65
#define NL_READ_SIZE   64

#define NL_CMD_ZONE_COUNT 0x03
#define NL_CMD_RGB_DATA   0x02
#define NL_CMD_INIT_MODE  0x07
#define NL_CMD_BRIGHTNESS 0x09

#define NL_GRB_ORDER_LIMIT 20

#define NL_RGB_PKT1_MAX 60
#define NL_RGB_PKT2_MAX 64
#define NL_RGB_PKT3_MAX 38
#define NL_RGB_WIRE_MAX (124 + NL_RGB_PKT3_MAX)

#define NL_WRITE_SETTLE_MS 100
#define NL_READ_TIMEOUT_MS 200

struct nl_device {
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

static void copy_hid_error(hid_device *dev, char errbuf[NL_ERRBUF])
{
    const wchar_t *w = hid_error(dev);
    if (w)
        snprintf(errbuf, NL_ERRBUF, "%ls", w);
    else
        snprintf(errbuf, NL_ERRBUF, "unknown HID error");
}

static nl_status send_command(struct nl_device *d, unsigned char cmd,
                              const unsigned char *data, size_t dlen,
                              unsigned char *resp, size_t resp_cap,
                              char errbuf[NL_ERRBUF])
{
    unsigned char buf[NL_REPORT_SIZE];
    unsigned char rbuf[NL_READ_SIZE];
    int n;

    if (dlen > NL_REPORT_SIZE - 4) {
        snprintf(errbuf, NL_ERRBUF, "command payload too large (%zu)", dlen);
        return NL_ERR_PROTO;
    }

    memset(buf, 0, sizeof(buf));
    buf[1] = cmd;
    buf[2] = (unsigned char)((dlen >> 8) & 0xFFu);
    buf[3] = (unsigned char)(dlen & 0xFFu);
    if (dlen > 0)
        memcpy(buf + 4, data, dlen);

    if (hid_write(d->dev, buf, sizeof(buf)) < 0) {
        copy_hid_error(d->dev, errbuf);
        return NL_ERR_HID_IO;
    }

    sleep_ms(NL_WRITE_SETTLE_MS);

    n = hid_read_timeout(d->dev, rbuf, sizeof(rbuf), NL_READ_TIMEOUT_MS);
    if (n < 0) {
        copy_hid_error(d->dev, errbuf);
        return NL_ERR_HID_IO;
    }

    if (resp && resp_cap > 0 && n > 0) {
        size_t len = (size_t)n;
        if (len > resp_cap)
            len = resp_cap;
        if (len > sizeof(rbuf))
            len = sizeof(rbuf);
        memcpy(resp, rbuf, len);
    }
    return NL_OK;
}

static nl_status query_zone_count(struct nl_device *d, char errbuf[NL_ERRBUF])
{
    unsigned char resp[NL_READ_SIZE];
    nl_status st;

    memset(resp, 0, sizeof(resp));
    st = send_command(d, NL_CMD_ZONE_COUNT, NULL, 0, resp, sizeof(resp), errbuf);
    if (st != NL_OK)
        return st;

    d->zone_count = resp[4];
    return NL_OK;
}

static nl_status write_rgb_data(struct nl_device *d, const unsigned char *rgb,
                                size_t len, char errbuf[NL_ERRBUF])
{
    unsigned char buf[NL_REPORT_SIZE];
    size_t n;

    memset(buf, 0, sizeof(buf));
    buf[1] = NL_CMD_RGB_DATA;
    buf[2] = (unsigned char)((len >> 8) & 0xFFu);
    buf[3] = (unsigned char)(len & 0xFFu);
    n = len < NL_RGB_PKT1_MAX ? len : NL_RGB_PKT1_MAX;
    memcpy(buf + 4, rgb, n);
    if (hid_write(d->dev, buf, sizeof(buf)) < 0) {
        copy_hid_error(d->dev, errbuf);
        return NL_ERR_HID_IO;
    }

    memset(buf, 0, sizeof(buf));
    if (len > 60) {
        n = len - 60;
        if (n > NL_RGB_PKT2_MAX)
            n = NL_RGB_PKT2_MAX;
        memcpy(buf + 1, rgb + 60, n);
    }
    if (hid_write(d->dev, buf, sizeof(buf)) < 0) {
        copy_hid_error(d->dev, errbuf);
        return NL_ERR_HID_IO;
    }

    memset(buf, 0, sizeof(buf));
    if (len > 124) {
        n = len - 124;
        if (n > NL_RGB_PKT3_MAX)
            n = NL_RGB_PKT3_MAX;
        memcpy(buf + 1, rgb + 124, n);
    }
    if (hid_write(d->dev, buf, sizeof(buf)) < 0) {
        copy_hid_error(d->dev, errbuf);
        return NL_ERR_HID_IO;
    }

    return NL_OK;
}

nl_status nl_device_open(struct nl_device **out, char errbuf[NL_ERRBUF])
{
    struct nl_device *d;
    nl_status st;

    *out = NULL;

    if (!hid_ready) {
        if (hid_init() != 0) {
            snprintf(errbuf, NL_ERRBUF, "hid_init failed");
            return NL_ERR_HID_INIT;
        }
        hid_ready = 1;
    }

    d = calloc(1, sizeof(*d));
    if (!d) {
        snprintf(errbuf, NL_ERRBUF, "out of memory");
        return NL_ERR_NOMEM;
    }

    d->dev = hid_open(NL_VENDOR_ID, NL_PRODUCT_ID, NULL);
    if (!d->dev) {
        snprintf(errbuf, NL_ERRBUF,
                 "could not open HID device %04x:%04x (permissions? not connected?)",
                 NL_VENDOR_ID, NL_PRODUCT_ID);
        free(d);
        return NL_ERR_HID_OPEN;
    }

    st = query_zone_count(d, errbuf);
    if (st != NL_OK) {
        hid_close(d->dev);
        free(d);
        return st;
    }

    *out = d;
    return NL_OK;
}

void nl_device_close(struct nl_device *d)
{
    if (!d)
        return;
    if (d->dev)
        hid_close(d->dev);
    free(d);
}

size_t nl_device_zone_count(const struct nl_device *d)
{
    return d->zone_count;
}

nl_status nl_device_initialize(struct nl_device *d, char errbuf[NL_ERRBUF])
{
    static const unsigned char on = 1;
    static const unsigned char full = 255;
    nl_status st;

    st = send_command(d, NL_CMD_INIT_MODE, &on, 1, NULL, 0, errbuf);
    if (st != NL_OK)
        return st;
    return send_command(d, NL_CMD_BRIGHTNESS, &full, 1, NULL, 0, errbuf);
}

nl_status nl_device_set_colors(struct nl_device *d, const struct nl_color *colors,
                               size_t n, char errbuf[NL_ERRBUF])
{
    unsigned char rgb[NL_RGB_WIRE_MAX];
    size_t i;
    size_t wire;
    static int overflow_warned;

    if (n > sizeof(rgb) / 3) {
        if (!overflow_warned) {
            nl_warn("zone count %zu exceeds wire capacity %d; extra zones ignored",
                    n, NL_RGB_WIRE_MAX / 3);
            overflow_warned = 1;
        }
        n = sizeof(rgb) / 3;
    }

    for (i = 0; i < n; i++) {
        struct nl_color c = colors[i];
        if (i < NL_GRB_ORDER_LIMIT) {
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

void nl_lib_shutdown(void)
{
    if (hid_ready) {
        hid_exit();
        hid_ready = 0;
    }
}

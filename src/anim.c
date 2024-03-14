#include "nlctl.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#include "anim_internal.h"
#include "capture.h"
#include "log.h"
#include "zone.h"

#define NL_TWO_PI 6.28318530717958647692
#define NL_ZONE_BUF 256

static struct nl_color scale_uniform_buf[NL_ZONE_BUF];

static size_t clamp_zone_count(size_t n)
{
    return n > NL_ZONE_BUF ? NL_ZONE_BUF : n;
}

static nl_status push_uniform(struct nl_device *dev, struct nl_color c,
                              char errbuf[NL_ERRBUF])
{
    size_t n = clamp_zone_count(nl_device_zone_count(dev));
    size_t i;
    for (i = 0; i < n; i++)
        scale_uniform_buf[i] = c;
    return nl_device_set_colors(dev, scale_uniform_buf, n, errbuf);
}

/* solid ------------------------------------------------------------------ */

struct anim_solid {
    struct nl_anim base;
    struct nl_color color;
    int applied;
};

static nl_status solid_run(struct nl_anim *self, struct nl_device *dev,
                           uint32_t *next_delay_ms, char errbuf[NL_ERRBUF])
{
    struct anim_solid *a = (struct anim_solid *)self;
    nl_status st;

    *next_delay_ms = 50;
    if (a->applied)
        return NL_OK;

    st = push_uniform(dev, a->color, errbuf);
    if (st == NL_OK)
        a->applied = 1;
    return st;
}

static void anim_free(struct nl_anim *self)
{
    free(self);
}

struct nl_anim *nl_anim_solid(struct nl_color c)
{
    struct anim_solid *a = calloc(1, sizeof(*a));
    if (!a)
        return NULL;
    a->base.run = solid_run;
    a->base.destroy = anim_free;
    a->color = c;
    return &a->base;
}

/* breathing ------------------------------------------------------------- */

struct anim_breathing {
    struct nl_anim base;
    struct nl_color color;
    uint32_t duration_ms;
    unsigned steps;
    unsigned frame;
};

static nl_status breathing_run(struct nl_anim *self, struct nl_device *dev,
                               uint32_t *next_delay_ms, char errbuf[NL_ERRBUF])
{
    struct anim_breathing *a = (struct anim_breathing *)self;
    double t = (double)a->frame * NL_TWO_PI / (double)a->steps;
    double brightness = (sin(t) + 1.0) / 2.0;
    nl_status st;

    *next_delay_ms = a->duration_ms / a->steps;
    st = push_uniform(dev, nl_color_scaled(a->color, brightness), errbuf);
    a->frame = (a->frame + 1) % a->steps;
    return st;
}

struct nl_anim *nl_anim_breathing(struct nl_color c, uint32_t duration_ms, unsigned steps)
{
    struct anim_breathing *a;
    if (steps == 0)
        steps = 1;
    a = calloc(1, sizeof(*a));
    if (!a)
        return NULL;
    a->base.run = breathing_run;
    a->base.destroy = anim_free;
    a->color = c;
    a->duration_ms = duration_ms;
    a->steps = steps;
    return &a->base;
}

/* wave ---------------------------------------------------------------- */

struct anim_wave {
    struct nl_anim base;
    struct nl_color color;
    uint32_t duration_ms;
    unsigned frames;
    unsigned frame;
};

static nl_status wave_run(struct nl_anim *self, struct nl_device *dev,
                          uint32_t *next_delay_ms, char errbuf[NL_ERRBUF])
{
    struct anim_wave *a = (struct anim_wave *)self;
    size_t zones = clamp_zone_count(nl_device_zone_count(dev));
    size_t i;

    *next_delay_ms = a->duration_ms / a->frames;
    if (zones == 0) {
        a->frame = (a->frame + 1) % a->frames;
        return NL_OK;
    }

    for (i = 0; i < zones; i++) {
        double offset = (double)(i + a->frame) * NL_TWO_PI / (double)zones;
        double brightness = (sin(offset) + 1.0) / 2.0;
        scale_uniform_buf[i] = nl_color_scaled(a->color, brightness);
    }
    a->frame = (a->frame + 1) % a->frames;
    return nl_device_set_colors(dev, scale_uniform_buf, zones, errbuf);
}

struct nl_anim *nl_anim_wave(struct nl_color c, uint32_t duration_ms, unsigned frames)
{
    struct anim_wave *a;
    if (frames == 0)
        frames = 1;
    a = calloc(1, sizeof(*a));
    if (!a)
        return NULL;
    a->base.run = wave_run;
    a->base.destroy = anim_free;
    a->color = c;
    a->duration_ms = duration_ms;
    a->frames = frames;
    return &a->base;
}

/* rainbow ----------------------------------------------------------- */

struct anim_rainbow {
    struct nl_anim base;
    uint32_t duration_ms;
    unsigned frames;
    unsigned frame;
};

struct nl_color nl_hsv_to_rgb(double h, double s, double v)
{
    double c = v * s;
    double x = c * (1.0 - fabs(fmod(h / 60.0, 2.0) - 1.0));
    double m = v - c;
    double r, g, b;
    struct nl_color out;

    if (h < 60.0) {
        r = c; g = x; b = 0.0;
    } else if (h < 120.0) {
        r = x; g = c; b = 0.0;
    } else if (h < 180.0) {
        r = 0.0; g = c; b = x;
    } else if (h < 240.0) {
        r = 0.0; g = x; b = c;
    } else if (h < 300.0) {
        r = x; g = 0.0; b = c;
    } else {
        r = c; g = 0.0; b = x;
    }

    out.r = (uint8_t)((r + m) * 255.0);
    out.g = (uint8_t)((g + m) * 255.0);
    out.b = (uint8_t)((b + m) * 255.0);
    return out;
}

static nl_status rainbow_run(struct nl_anim *self, struct nl_device *dev,
                             uint32_t *next_delay_ms, char errbuf[NL_ERRBUF])
{
    struct anim_rainbow *a = (struct anim_rainbow *)self;
    size_t zones = clamp_zone_count(nl_device_zone_count(dev));
    size_t i;

    *next_delay_ms = a->duration_ms / a->frames;
    if (zones == 0) {
        a->frame = (a->frame + 1) % a->frames;
        return NL_OK;
    }

    for (i = 0; i < zones; i++) {
        size_t hue = ((i * 360 / zones) + ((size_t)a->frame * 360 / a->frames)) % 360;
        scale_uniform_buf[i] = nl_hsv_to_rgb((double)hue, 1.0, 1.0);
    }
    a->frame = (a->frame + 1) % a->frames;
    return nl_device_set_colors(dev, scale_uniform_buf, zones, errbuf);
}

struct nl_anim *nl_anim_rainbow(uint32_t duration_ms, unsigned frames)
{
    struct anim_rainbow *a;
    if (frames == 0)
        frames = 1;
    a = calloc(1, sizeof(*a));
    if (!a)
        return NULL;
    a->base.run = rainbow_run;
    a->base.destroy = anim_free;
    a->duration_ms = duration_ms;
    a->frames = frames;
    return &a->base;
}

/* reactive -------------------------------------------------------- */

struct anim_reactive {
    struct nl_anim base;
    struct nl_capture *cap;
    size_t bottom;
    size_t left;
    size_t top;
    size_t right;
    float capture_percent;
    int zone_depth;
    unsigned fps;
};

static nl_status reactive_run(struct nl_anim *self, struct nl_device *dev,
                              uint32_t *next_delay_ms, char errbuf[NL_ERRBUF])
{
    struct anim_reactive *a = (struct anim_reactive *)self;
    struct nl_zone_color zc[NL_ZONE_BUF];
    const uint8_t *data;
    size_t data_len;
    size_t nz, i, dev_zones;

    *next_delay_ms = 1000u / a->fps;

    if (!nl_capture_grab(a->cap, a->capture_percent))
        return NL_OK;

    data = nl_capture_data(a->cap, &data_len);
    nz = nl_zone_analyze(a->zone_depth, data, data_len, nl_capture_width(a->cap),
                         nl_capture_height(a->cap), nl_capture_bpp(a->cap), a->bottom,
                         a->left, a->top, a->right, zc, NL_ZONE_BUF);

    dev_zones = nl_device_zone_count(dev);
    if (nz != dev_zones)
        nl_warn("zone count (%zu) doesn't match LED count (%zu)", nz, dev_zones);

    for (i = 0; i < nz; i++) {
        scale_uniform_buf[i].r = (uint8_t)(zc[i].r * 255.0f);
        scale_uniform_buf[i].g = (uint8_t)(zc[i].g * 255.0f);
        scale_uniform_buf[i].b = (uint8_t)(zc[i].b * 255.0f);
    }
    return nl_device_set_colors(dev, scale_uniform_buf, nz, errbuf);
}

static void reactive_destroy(struct nl_anim *self)
{
    struct anim_reactive *a = (struct anim_reactive *)self;
    nl_capture_free(a->cap);
    free(a);
}

struct nl_anim *nl_anim_reactive(const uint8_t zones[4], float capture_percent,
                                 int zone_depth, unsigned fps, const char *display,
                                 const char *xauthority, char errbuf[NL_ERRBUF])
{
    struct anim_reactive *a;
    struct nl_capture *cap;
    nl_status st;

    if (xauthority && xauthority[0])
        setenv("XAUTHORITY", xauthority, 1);

    st = nl_capture_new(&cap, display, errbuf);
    if (st != NL_OK)
        return NULL;

    a = calloc(1, sizeof(*a));
    if (!a) {
        nl_capture_free(cap);
        snprintf(errbuf, NL_ERRBUF, "out of memory");
        return NULL;
    }

    a->base.run = reactive_run;
    a->base.destroy = reactive_destroy;
    a->cap = cap;
    a->bottom = zones[0];
    a->left = zones[1];
    a->top = zones[2];
    a->right = zones[3];
    a->capture_percent = capture_percent;
    a->zone_depth = zone_depth;
    a->fps = fps ? fps : 1;
    return &a->base;
}

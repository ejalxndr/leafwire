#ifndef LEAFWIRE_H
#define LEAFWIRE_H

#include <stddef.h>
#include <stdint.h>

#define LW_ERRBUF 256

typedef enum {
    LW_OK = 0,
    LW_ERR_HID_INIT,
    LW_ERR_HID_OPEN,
    LW_ERR_HID_IO,
    LW_ERR_X_OPEN,
    LW_ERR_X_SHM,
    LW_ERR_SOCKET,
    LW_ERR_PROTO,
    LW_ERR_STATE_IO,
    LW_ERR_ARGS,
    LW_ERR_NODEV,
    LW_ERR_NOMEM,
    LW_ERR_TIMEOUT
} lw_status;

const char *lw_strerror(lw_status s);

struct lw_color {
    uint8_t r;
    uint8_t g;
    uint8_t b;
};

struct lw_color lw_color_scaled(struct lw_color c, double brightness);

struct lw_device;

lw_status lw_device_open(struct lw_device **out, char errbuf[LW_ERRBUF]);
void lw_device_close(struct lw_device *d);
size_t lw_device_zone_count(const struct lw_device *d);
lw_status lw_device_initialize(struct lw_device *d, char errbuf[LW_ERRBUF]);
lw_status lw_device_set_colors(struct lw_device *d, const struct lw_color *colors,
                               size_t n, char errbuf[LW_ERRBUF]);
void lw_lib_shutdown(void);

struct lw_anim {
    lw_status (*run)(struct lw_anim *self, struct lw_device *dev,
                     uint32_t *next_delay_ms, char errbuf[LW_ERRBUF]);
    void (*destroy)(struct lw_anim *self);
};

struct lw_anim *lw_anim_solid(struct lw_color c);
struct lw_anim *lw_anim_breathing(struct lw_color c, uint32_t duration_ms, unsigned steps);
struct lw_anim *lw_anim_wave(struct lw_color c, uint32_t duration_ms, unsigned frames);
struct lw_anim *lw_anim_rainbow(uint32_t duration_ms, unsigned frames);
struct lw_anim *lw_anim_reactive(const uint8_t zones[4], float capture_percent,
                                 int zone_depth, unsigned fps, const char *display,
                                 const char *xauthority, char errbuf[LW_ERRBUF]);

#endif

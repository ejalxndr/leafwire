#ifndef NLCTL_H
#define NLCTL_H

#include <stddef.h>
#include <stdint.h>

#define NL_ERRBUF 256

typedef enum {
    NL_OK = 0,
    NL_ERR_HID_INIT,
    NL_ERR_HID_OPEN,
    NL_ERR_HID_IO,
    NL_ERR_X_OPEN,
    NL_ERR_X_SHM,
    NL_ERR_SOCKET,
    NL_ERR_PROTO,
    NL_ERR_STATE_IO,
    NL_ERR_ARGS,
    NL_ERR_NODEV,
    NL_ERR_NOMEM,
    NL_ERR_TIMEOUT
} nl_status;

const char *nl_strerror(nl_status s);

struct nl_color {
    uint8_t r;
    uint8_t g;
    uint8_t b;
};

struct nl_color nl_color_scaled(struct nl_color c, double brightness);

struct nl_device;

nl_status nl_device_open(struct nl_device **out, char errbuf[NL_ERRBUF]);
void nl_device_close(struct nl_device *d);
size_t nl_device_zone_count(const struct nl_device *d);
nl_status nl_device_initialize(struct nl_device *d, char errbuf[NL_ERRBUF]);
nl_status nl_device_set_colors(struct nl_device *d, const struct nl_color *colors,
                               size_t n, char errbuf[NL_ERRBUF]);
void nl_lib_shutdown(void);

struct nl_anim {
    nl_status (*run)(struct nl_anim *self, struct nl_device *dev,
                     uint32_t *next_delay_ms, char errbuf[NL_ERRBUF]);
    void (*destroy)(struct nl_anim *self);
};

struct nl_anim *nl_anim_solid(struct nl_color c);
struct nl_anim *nl_anim_breathing(struct nl_color c, uint32_t duration_ms, unsigned steps);
struct nl_anim *nl_anim_wave(struct nl_color c, uint32_t duration_ms, unsigned frames);
struct nl_anim *nl_anim_rainbow(uint32_t duration_ms, unsigned frames);
struct nl_anim *nl_anim_reactive(const uint8_t zones[4], float capture_percent,
                                 int zone_depth, unsigned fps, const char *display,
                                 const char *xauthority, char errbuf[NL_ERRBUF]);

#endif

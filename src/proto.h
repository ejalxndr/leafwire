#ifndef LW_PROTO_H
#define LW_PROTO_H

#include <stdint.h>

#define LW_SOCKET_PATH "/run/leafwire/leafwire.sock"
#define LW_STATE_PATH "/var/lib/leafwire/state.bin"

#define LW_PROTO_VERSION 1u

#define LW_DISPLAY_MAX 128
#define LW_XAUTH_MAX 256
#define LW_TEXT_MAX 256

typedef enum {
    LW_MODE_SOLID = 0,
    LW_MODE_BREATHING = 1,
    LW_MODE_WAVE = 2,
    LW_MODE_RAINBOW = 3,
    LW_MODE_REACTIVE = 4,
    LW_MODE_OFF = 5,
    LW_MODE_STATUS = 6
} lw_mode;

#define LW_MODE_MAX LW_MODE_STATUS

struct lw_request {
    uint8_t version;
    uint8_t mode;
    uint8_t has_color;
    uint8_t color[3];
    uint8_t zones[4];
    char display[LW_DISPLAY_MAX];
    char xauthority[LW_XAUTH_MAX];
};

struct lw_response {
    uint8_t version;
    uint8_t ok;
    char text[LW_TEXT_MAX];
};

#endif

#ifndef NL_PROTO_H
#define NL_PROTO_H

#include <stdint.h>

#define NL_SOCKET_PATH "/run/nlctl/nlctl.sock"
#define NL_STATE_PATH "/var/lib/nlctl/state.bin"

#define NL_PROTO_VERSION 1u

#define NL_DISPLAY_MAX 128
#define NL_XAUTH_MAX 256
#define NL_TEXT_MAX 256

typedef enum {
    NL_MODE_SOLID = 0,
    NL_MODE_BREATHING = 1,
    NL_MODE_WAVE = 2,
    NL_MODE_RAINBOW = 3,
    NL_MODE_REACTIVE = 4,
    NL_MODE_OFF = 5,
    NL_MODE_STATUS = 6
} nl_mode;

#define NL_MODE_MAX NL_MODE_STATUS

struct nl_request {
    uint8_t version;
    uint8_t mode;
    uint8_t has_color;
    uint8_t color[3];
    uint8_t zones[4];
    char display[NL_DISPLAY_MAX];
    char xauthority[NL_XAUTH_MAX];
};

struct nl_response {
    uint8_t version;
    uint8_t ok;
    char text[NL_TEXT_MAX];
};

#endif

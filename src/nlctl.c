#define NL_LOG_PREFIX "nlctl"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "ipc.h"
#include "nlctl.h"
#include "proto.h"

struct subcmd {
    const char *name;
    nl_mode mode;
    int takes_color;
    int takes_zones;
};

static const struct subcmd SUBCMDS[] = {
    { "solid",     NL_MODE_SOLID,     1, 0 },
    { "breathing", NL_MODE_BREATHING, 1, 0 },
    { "wave",      NL_MODE_WAVE,      1, 0 },
    { "rainbow",   NL_MODE_RAINBOW,   0, 0 },
    { "reactive",  NL_MODE_REACTIVE,  0, 1 },
    { "off",       NL_MODE_OFF,       0, 0 },
    { "status",    NL_MODE_STATUS,    0, 0 },
};

static void usage(FILE *f)
{
    fprintf(f,
            "usage: nlctl <command> [options]\n"
            "\n"
            "commands:\n"
            "  solid      [--color R,G,B]\n"
            "  breathing  [--color R,G,B]\n"
            "  wave       [--color R,G,B]\n"
            "  rainbow\n"
            "  reactive   [--zones B,L,T,R]\n"
            "  off\n"
            "  status\n");
}

static int parse_triplet(const char *s, long *out, int count, long lo, long hi)
{
    int i;
    for (i = 0; i < count; i++) {
        char *end;
        long v;
        while (*s == ' ')
            s++;
        v = strtol(s, &end, 10);
        if (end == s || v < lo || v > hi)
            return -1;
        out[i] = v;
        s = end;
        while (*s == ' ')
            s++;
        if (i + 1 < count) {
            if (*s != ',')
                return -1;
            s++;
        }
    }
    if (*s != '\0')
        return -1;
    return 0;
}

int main(int argc, char **argv)
{
    const struct subcmd *sc = NULL;
    struct nl_request req;
    struct nl_response resp;
    const char *color_arg = NULL;
    const char *zones_arg = NULL;
    char errbuf[NL_ERRBUF];
    size_t i;
    int a;
    int fd;
    nl_status st;

    if (argc >= 2 && (strcmp(argv[1], "-h") == 0 || strcmp(argv[1], "--help") == 0)) {
        usage(stdout);
        return 0;
    }
    if (argc < 2) {
        usage(stderr);
        return 2;
    }

    for (i = 0; i < sizeof(SUBCMDS) / sizeof(SUBCMDS[0]); i++) {
        if (strcmp(argv[1], SUBCMDS[i].name) == 0) {
            sc = &SUBCMDS[i];
            break;
        }
    }
    if (!sc) {
        fprintf(stderr, "nlctl: unknown command '%s'\n", argv[1]);
        usage(stderr);
        return 2;
    }

    for (a = 2; a < argc; a++) {
        if (sc->takes_color && strcmp(argv[a], "--color") == 0 && a + 1 < argc) {
            color_arg = argv[++a];
        } else if (sc->takes_zones && strcmp(argv[a], "--zones") == 0 && a + 1 < argc) {
            zones_arg = argv[++a];
        } else {
            fprintf(stderr, "nlctl: unexpected argument '%s'\n", argv[a]);
            usage(stderr);
            return 2;
        }
    }

    memset(&req, 0, sizeof(req));
    req.version = NL_PROTO_VERSION;
    req.mode = (uint8_t)sc->mode;

    if (color_arg) {
        long v[3];
        if (parse_triplet(color_arg, v, 3, 0, 255) != 0) {
            fprintf(stderr, "nlctl: --color must be R,G,B (e.g. 255,0,128)\n");
            return 2;
        }
        req.has_color = 1;
        req.color[0] = (uint8_t)v[0];
        req.color[1] = (uint8_t)v[1];
        req.color[2] = (uint8_t)v[2];
    }

    if (zones_arg) {
        long v[4];
        if (parse_triplet(zones_arg, v, 4, 0, 255) != 0) {
            fprintf(stderr, "nlctl: --zones must be B,L,T,R (e.g. 10,10,10,10)\n");
            return 2;
        }
        req.zones[0] = (uint8_t)v[0];
        req.zones[1] = (uint8_t)v[1];
        req.zones[2] = (uint8_t)v[2];
        req.zones[3] = (uint8_t)v[3];
    }

    if (sc->mode == NL_MODE_REACTIVE) {
        const char *disp = getenv("DISPLAY");
        const char *xauth = getenv("XAUTHORITY");
        if (disp)
            snprintf(req.display, sizeof(req.display), "%s", disp);
        if (xauth)
            snprintf(req.xauthority, sizeof(req.xauthority), "%s", xauth);
    }

    {
        const char *sock = getenv("NLCTL_SOCKET");
        if (!sock || !sock[0])
            sock = NL_SOCKET_PATH;
        fd = ipc_connect_unix(sock, errbuf);
    }
    if (fd < 0) {
        fprintf(stderr, "nlctl: %s - is nlctld running?\n", errbuf);
        return 1;
    }

    st = ipc_write_full(fd, &req, sizeof(req));
    if (st != NL_OK) {
        fprintf(stderr, "nlctl: send failed: %s\n", nl_strerror(st));
        close(fd);
        return 1;
    }

    st = ipc_read_full(fd, &resp, sizeof(resp));
    close(fd);
    if (st != NL_OK) {
        fprintf(stderr, "nlctl: no valid response: %s\n", nl_strerror(st));
        return 1;
    }

    if (resp.version != NL_PROTO_VERSION) {
        fprintf(stderr, "nlctl: response protocol mismatch\n");
        return 1;
    }

    resp.text[sizeof(resp.text) - 1] = '\0';
    if (resp.ok) {
        if (sc->mode == NL_MODE_STATUS)
            printf("Current mode: %s\n", resp.text);
        else
            printf("OK\n");
        return 0;
    }

    fprintf(stderr, "nlctl: %s\n", resp.text[0] ? resp.text : "unknown error");
    return 1;
}

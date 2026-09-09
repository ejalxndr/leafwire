#define LW_LOG_PREFIX "lwd"

#include <errno.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/epoll.h>
#include <sys/signalfd.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/timerfd.h>
#include <sys/types.h>
#include <unistd.h>

#include "ipc.h"
#include "log.h"
#include "leafwire.h"
#include "proto.h"

#define RECONNECT_INTERVAL_MS 2000
#define REQUEST_TIMEOUT_MS 1000
#define RENDER_SOON_MS 1

enum dev_state {
    DEV_DOWN,
    DEV_UP
};

struct daemon {
    enum dev_state state;
    struct lw_device *dev;
    struct lw_anim *anim;
    char mode_name[16];
    const char *sock_path;
    int first_connect;

    int listen_fd;
    int frame_fd;
    int reconnect_fd;
    int sig_fd;
    int epoll_fd;
};

static volatile sig_atomic_t g_stop;

static const char *mode_name(lw_mode m)
{
    switch (m) {
    case LW_MODE_SOLID:     return "solid";
    case LW_MODE_BREATHING: return "breathing";
    case LW_MODE_WAVE:      return "wave";
    case LW_MODE_RAINBOW:   return "rainbow";
    case LW_MODE_REACTIVE:  return "reactive";
    case LW_MODE_OFF:       return "off";
    case LW_MODE_STATUS:    return "status";
    }
    return "unknown";
}

static void timer_set(int fd, unsigned value_ms, unsigned interval_ms)
{
    struct itimerspec its;
    its.it_value.tv_sec = (time_t)(value_ms / 1000u);
    its.it_value.tv_nsec = (long)(value_ms % 1000u) * 1000000L;
    its.it_interval.tv_sec = (time_t)(interval_ms / 1000u);
    its.it_interval.tv_nsec = (long)(interval_ms % 1000u) * 1000000L;
    timerfd_settime(fd, 0, &its, NULL);
}

static void timer_disarm(int fd)
{
    struct itimerspec its;
    memset(&its, 0, sizeof(its));
    timerfd_settime(fd, 0, &its, NULL);
}

static void drain_timer(int fd)
{
    uint64_t ticks;
    while (read(fd, &ticks, sizeof(ticks)) == (ssize_t)sizeof(ticks))
        ;
}

static int mkdir_p(const char *path)
{
    char tmp[256];
    char *p;
    size_t len;

    len = strlen(path);
    if (len == 0 || len >= sizeof(tmp))
        return -1;
    memcpy(tmp, path, len + 1);
    if (tmp[len - 1] == '/')
        tmp[len - 1] = '\0';

    for (p = tmp + 1; *p; p++) {
        if (*p == '/') {
            *p = '\0';
            if (mkdir(tmp, 0755) != 0 && errno != EEXIST)
                return -1;
            *p = '/';
        }
    }
    if (mkdir(tmp, 0755) != 0 && errno != EEXIST)
        return -1;
    return 0;
}

static void parent_dir(const char *path, char *out, size_t out_sz)
{
    const char *slash = strrchr(path, '/');
    size_t n;
    if (!slash || slash == path) {
        snprintf(out, out_sz, ".");
        return;
    }
    n = (size_t)(slash - path);
    if (n >= out_sz)
        n = out_sz - 1;
    memcpy(out, path, n);
    out[n] = '\0';
}

static struct lw_anim *build_animation(const struct lw_request *req, char errbuf[LW_ERRBUF])
{
    struct lw_color white = { 255, 255, 255 };
    struct lw_color c = white;

    if (req->has_color) {
        c.r = req->color[0];
        c.g = req->color[1];
        c.b = req->color[2];
    }

    switch ((lw_mode)req->mode) {
    case LW_MODE_SOLID:
        return lw_anim_solid(c);
    case LW_MODE_BREATHING:
        return lw_anim_breathing(c, 3000, 500);
    case LW_MODE_WAVE:
        return lw_anim_wave(c, 2000, 50);
    case LW_MODE_RAINBOW:
        return lw_anim_rainbow(5000, 100);
    case LW_MODE_OFF: {
        struct lw_color black = { 0, 0, 0 };
        return lw_anim_solid(black);
    }
    case LW_MODE_REACTIVE: {
        uint8_t zones[4];
        int i;
        for (i = 0; i < 4; i++)
            zones[i] = req->zones[i] ? req->zones[i] : (uint8_t)10;
        return lw_anim_reactive(zones, 0.9f, 10, 60,
                                req->display[0] ? req->display : NULL,
                                req->xauthority[0] ? req->xauthority : NULL, errbuf);
    }
    case LW_MODE_STATUS:
        break;
    }
    snprintf(errbuf, LW_ERRBUF, "invalid mode %u", req->mode);
    return NULL;
}

static void save_state(const struct lw_request *req)
{
    char dir[256];
    struct lw_request persisted = *req;
    FILE *f;

    memset(persisted.display, 0, sizeof(persisted.display));
    memset(persisted.xauthority, 0, sizeof(persisted.xauthority));

    parent_dir(LW_STATE_PATH, dir, sizeof(dir));
    mkdir_p(dir);

    f = fopen(LW_STATE_PATH, "wb");
    if (!f) {
        lw_warn("could not save state: %s", strerror(errno));
        return;
    }
    if (fwrite(&persisted, sizeof(persisted), 1, f) != 1)
        lw_warn("could not write state file");
    fclose(f);
}

static int load_state(struct lw_request *out)
{
    FILE *f = fopen(LW_STATE_PATH, "rb");
    size_t n;

    if (!f)
        return 0;
    n = fread(out, sizeof(*out), 1, f);
    fclose(f);

    if (n != 1) {
        lw_warn("ignoring unreadable state file");
        return 0;
    }
    if (out->version != LW_PROTO_VERSION || out->mode > LW_MODE_MAX) {
        lw_warn("ignoring state file with incompatible version/mode");
        return 0;
    }
    out->display[0] = '\0';
    out->xauthority[0] = '\0';
    return 1;
}

static void set_mode_name(struct daemon *d, lw_mode m)
{
    snprintf(d->mode_name, sizeof(d->mode_name), "%s", mode_name(m));
}

static void go_offline(struct daemon *d)
{
    if (d->dev) {
        lw_device_close(d->dev);
        d->dev = NULL;
    }
    d->state = DEV_DOWN;
    timer_disarm(d->frame_fd);
    timer_set(d->reconnect_fd, RECONNECT_INTERVAL_MS, RECONNECT_INTERVAL_MS);
}

static void try_connect(struct daemon *d)
{
    char errbuf[LW_ERRBUF];
    struct lw_device *dev = NULL;
    lw_status st;

    st = lw_device_open(&dev, errbuf);
    if (st != LW_OK) {
        lw_info("waiting for device: %s", errbuf);
        return;
    }
    st = lw_device_initialize(dev, errbuf);
    if (st != LW_OK) {
        lw_err("device init failed: %s", errbuf);
        lw_device_close(dev);
        return;
    }

    d->dev = dev;
    d->state = DEV_UP;
    timer_disarm(d->reconnect_fd);
    lw_info("device connected - %zu LEDs", lw_device_zone_count(dev));

    if (d->first_connect) {
        struct lw_request saved;
        d->first_connect = 0;
        if (load_state(&saved)) {
            struct lw_anim *restored = build_animation(&saved, errbuf);
            if (restored) {
                if (d->anim)
                    d->anim->destroy(d->anim);
                d->anim = restored;
                set_mode_name(d, (lw_mode)saved.mode);
                lw_info("restored mode '%s' from state file", d->mode_name);
            } else {
                lw_err("could not restore saved state: %s", errbuf);
            }
        }
    }

    timer_set(d->frame_fd, RENDER_SOON_MS, 0);
}

static void handle_frame(struct daemon *d)
{
    char errbuf[LW_ERRBUF];
    uint32_t delay_ms = 50;
    lw_status st;

    drain_timer(d->frame_fd);
    if (d->state != DEV_UP || !d->anim)
        return;

    st = d->anim->run(d->anim, d->dev, &delay_ms, errbuf);
    if (st == LW_ERR_HID_IO) {
        lw_err("device error (%s), attempting reconnect...", errbuf);
        go_offline(d);
        return;
    }
    if (st != LW_OK)
        lw_warn("animation frame failed: %s", errbuf);

    if (delay_ms == 0)
        delay_ms = 1;
    timer_set(d->frame_fd, delay_ms, 0);
}

static void handle_client(struct daemon *d)
{
    char errbuf[LW_ERRBUF];
    struct lw_request req;
    struct lw_response resp;
    struct timeval tv;
    int cfd;
    lw_status st;

    cfd = accept(d->listen_fd, NULL, NULL);
    if (cfd < 0)
        return;

    tv.tv_sec = REQUEST_TIMEOUT_MS / 1000;
    tv.tv_usec = (REQUEST_TIMEOUT_MS % 1000) * 1000;
    setsockopt(cfd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    setsockopt(cfd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));

    memset(&resp, 0, sizeof(resp));
    resp.version = LW_PROTO_VERSION;

    st = ipc_read_full(cfd, &req, sizeof(req));
    if (st != LW_OK) {
        resp.ok = 0;
        snprintf(resp.text, sizeof(resp.text), "bad request: %s", lw_strerror(st));
        ipc_write_full(cfd, &resp, sizeof(resp));
        close(cfd);
        return;
    }

    if (req.version != LW_PROTO_VERSION || req.mode > LW_MODE_MAX) {
        resp.ok = 0;
        snprintf(resp.text, sizeof(resp.text), "unsupported protocol version/mode");
        ipc_write_full(cfd, &resp, sizeof(resp));
        close(cfd);
        return;
    }
    req.display[LW_DISPLAY_MAX - 1] = '\0';
    req.xauthority[LW_XAUTH_MAX - 1] = '\0';

    if (req.mode == LW_MODE_STATUS) {
        resp.ok = 1;
        snprintf(resp.text, sizeof(resp.text), "%s", d->mode_name);
        ipc_write_full(cfd, &resp, sizeof(resp));
        close(cfd);
        return;
    }

    if (d->state != DEV_UP) {
        resp.ok = 0;
        snprintf(resp.text, sizeof(resp.text), "Device not connected");
        ipc_write_full(cfd, &resp, sizeof(resp));
        close(cfd);
        return;
    }

    errbuf[0] = '\0';
    struct lw_anim *new_anim = build_animation(&req, errbuf);
    if (!new_anim) {
        lw_err("failed to build animation: %s", errbuf);
        resp.ok = 0;
        snprintf(resp.text, sizeof(resp.text), "%s",
                 errbuf[0] ? errbuf : "could not build animation");
        ipc_write_full(cfd, &resp, sizeof(resp));
        close(cfd);
        return;
    }

    if (d->anim)
        d->anim->destroy(d->anim);
    d->anim = new_anim;
    set_mode_name(d, (lw_mode)req.mode);
    lw_info("switched to %s", d->mode_name);

    if (req.mode != LW_MODE_REACTIVE)
        save_state(&req);

    timer_set(d->frame_fd, RENDER_SOON_MS, 0);

    resp.ok = 1;
    ipc_write_full(cfd, &resp, sizeof(resp));
    close(cfd);
}

static void on_signal(int sig)
{
    (void)sig;
    g_stop = 1;
}

static int add_epoll(int ep, int fd)
{
    struct epoll_event ev;
    memset(&ev, 0, sizeof(ev));
    ev.events = EPOLLIN;
    ev.data.fd = fd;
    return epoll_ctl(ep, EPOLL_CTL_ADD, fd, &ev);
}

int main(void)
{
    char errbuf[LW_ERRBUF];
    char dir[256];
    struct daemon d;
    sigset_t mask;
    struct lw_color black = { 0, 0, 0 };

    memset(&d, 0, sizeof(d));
    d.state = DEV_DOWN;
    d.first_connect = 1;
    d.listen_fd = d.frame_fd = d.reconnect_fd = d.sig_fd = d.epoll_fd = -1;
    snprintf(d.mode_name, sizeof(d.mode_name), "off");
    d.sock_path = getenv("LEAFWIRE_SOCKET");
    if (!d.sock_path || !d.sock_path[0])
        d.sock_path = LW_SOCKET_PATH;

    signal(SIGPIPE, SIG_IGN);

    d.anim = lw_anim_solid(black);
    if (!d.anim) {
        lw_err("out of memory");
        return 1;
    }

    parent_dir(d.sock_path, dir, sizeof(dir));
    if (mkdir_p(dir) != 0) {
        lw_err("could not create socket directory %s: %s", dir, strerror(errno));
        return 1;
    }

    d.listen_fd = ipc_listen_unix(d.sock_path, errbuf);
    if (d.listen_fd < 0) {
        lw_err("%s", errbuf);
        return 1;
    }
    lw_info("socket at %s", d.sock_path);

    d.frame_fd = timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK | TFD_CLOEXEC);
    d.reconnect_fd = timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK | TFD_CLOEXEC);
    if (d.frame_fd < 0 || d.reconnect_fd < 0) {
        lw_err("timerfd_create: %s", strerror(errno));
        return 1;
    }

    sigemptyset(&mask);
    sigaddset(&mask, SIGTERM);
    sigaddset(&mask, SIGINT);
    if (sigprocmask(SIG_BLOCK, &mask, NULL) != 0) {
        lw_err("sigprocmask: %s", strerror(errno));
        return 1;
    }
    d.sig_fd = signalfd(-1, &mask, SFD_NONBLOCK | SFD_CLOEXEC);
    if (d.sig_fd < 0) {
        lw_err("signalfd: %s", strerror(errno));
        return 1;
    }
    signal(SIGTERM, on_signal);
    signal(SIGINT, on_signal);

    d.epoll_fd = epoll_create1(EPOLL_CLOEXEC);
    if (d.epoll_fd < 0 || add_epoll(d.epoll_fd, d.listen_fd) != 0 ||
        add_epoll(d.epoll_fd, d.frame_fd) != 0 ||
        add_epoll(d.epoll_fd, d.reconnect_fd) != 0 ||
        add_epoll(d.epoll_fd, d.sig_fd) != 0) {
        lw_err("epoll setup failed: %s", strerror(errno));
        return 1;
    }

    timer_set(d.reconnect_fd, RENDER_SOON_MS, RECONNECT_INTERVAL_MS);

    while (!g_stop) {
        struct epoll_event events[8];
        int n = epoll_wait(d.epoll_fd, events, 8, -1);
        int i;

        if (n < 0) {
            if (errno == EINTR)
                continue;
            lw_err("epoll_wait: %s", strerror(errno));
            break;
        }

        for (i = 0; i < n; i++) {
            int fd = events[i].data.fd;
            if (fd == d.sig_fd) {
                g_stop = 1;
            } else if (fd == d.listen_fd) {
                handle_client(&d);
            } else if (fd == d.reconnect_fd) {
                drain_timer(d.reconnect_fd);
                if (d.state == DEV_DOWN)
                    try_connect(&d);
            } else if (fd == d.frame_fd) {
                handle_frame(&d);
            }
        }
    }

    lw_info("shutting down");
    if (d.anim)
        d.anim->destroy(d.anim);
    if (d.dev)
        lw_device_close(d.dev);
    lw_lib_shutdown();
    if (d.epoll_fd >= 0)
        close(d.epoll_fd);
    if (d.sig_fd >= 0)
        close(d.sig_fd);
    if (d.frame_fd >= 0)
        close(d.frame_fd);
    if (d.reconnect_fd >= 0)
        close(d.reconnect_fd);
    if (d.listen_fd >= 0)
        close(d.listen_fd);
    unlink(d.sock_path);
    return 0;
}

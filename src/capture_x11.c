#include "capture.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ipc.h>
#include <sys/shm.h>

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/extensions/XShm.h>

struct nl_capture {
    Display *dpy;
    Window root;
    int screen_w;
    int screen_h;
    int capture_w;
    int capture_h;
    XImage *ximg;
    int use_shm;
    XShmSegmentInfo shminfo;
    float last_percent;
};

static void teardown_image(struct nl_capture *c)
{
    if (!c->ximg)
        return;

    if (c->use_shm) {
        XShmDetach(c->dpy, &c->shminfo);
        XDestroyImage(c->ximg);
        c->ximg = NULL;
        if (c->shminfo.shmaddr && c->shminfo.shmaddr != (char *)-1)
            shmdt(c->shminfo.shmaddr);
        c->shminfo.shmaddr = NULL;
        c->shminfo.shmid = -1;
    } else {
        XDestroyImage(c->ximg);
        c->ximg = NULL;
    }
}

static void reallocate_shm(struct nl_capture *c, int w, int h)
{
    int screen;
    Visual *visual;
    unsigned int depth;
    size_t size;
    char *addr;

    teardown_image(c);

    screen = DefaultScreen(c->dpy);
    visual = DefaultVisual(c->dpy, screen);
    depth = (unsigned int)DefaultDepth(c->dpy, screen);

    c->ximg = XShmCreateImage(c->dpy, visual, depth, ZPixmap, NULL, &c->shminfo,
                              (unsigned int)w, (unsigned int)h);
    if (!c->ximg)
        return;

    size = (size_t)c->ximg->bytes_per_line * (size_t)c->ximg->height;
    c->shminfo.shmid = shmget(IPC_PRIVATE, size, IPC_CREAT | 0777);
    if (c->shminfo.shmid == -1) {
        XDestroyImage(c->ximg);
        c->ximg = NULL;
        return;
    }

    addr = shmat(c->shminfo.shmid, NULL, 0);
    if (addr == (char *)-1) {
        shmctl(c->shminfo.shmid, IPC_RMID, NULL);
        c->shminfo.shmid = -1;
        XDestroyImage(c->ximg);
        c->ximg = NULL;
        return;
    }

    c->shminfo.shmaddr = addr;
    c->ximg->data = addr;
    c->shminfo.readOnly = False;

    if (!XShmAttach(c->dpy, &c->shminfo)) {
        shmdt(addr);
        shmctl(c->shminfo.shmid, IPC_RMID, NULL);
        c->shminfo.shmid = -1;
        c->shminfo.shmaddr = NULL;
        XDestroyImage(c->ximg);
        c->ximg = NULL;
        c->use_shm = 0;
        return;
    }

    XSync(c->dpy, False);
    shmctl(c->shminfo.shmid, IPC_RMID, NULL);
}

nl_status nl_capture_new(struct nl_capture **out, const char *display,
                         char errbuf[NL_ERRBUF])
{
    struct nl_capture *c;
    XWindowAttributes attrs;

    *out = NULL;

    c = calloc(1, sizeof(*c));
    if (!c) {
        snprintf(errbuf, NL_ERRBUF, "out of memory");
        return NL_ERR_NOMEM;
    }
    c->shminfo.shmid = -1;

    c->dpy = XOpenDisplay((display && display[0]) ? display : NULL);
    if (!c->dpy) {
        snprintf(errbuf, NL_ERRBUF, "could not open X display '%s'",
                 (display && display[0]) ? display : "(default)");
        free(c);
        return NL_ERR_X_OPEN;
    }

    c->root = DefaultRootWindow(c->dpy);
    memset(&attrs, 0, sizeof(attrs));
    XGetWindowAttributes(c->dpy, c->root, &attrs);
    c->screen_w = attrs.width;
    c->screen_h = attrs.height;
    c->use_shm = XShmQueryExtension(c->dpy) ? 1 : 0;

    *out = c;
    return NL_OK;
}

void nl_capture_free(struct nl_capture *c)
{
    if (!c)
        return;
    teardown_image(c);
    if (c->dpy)
        XCloseDisplay(c->dpy);
    free(c);
}

int nl_capture_grab(struct nl_capture *c, float percent)
{
    int cx, cy;
    unsigned long all_planes = AllPlanes;

    if (percent < 0.01f)
        percent = 0.01f;
    if (percent > 1.0f)
        percent = 1.0f;

    c->capture_w = (int)((float)c->screen_w * percent);
    c->capture_h = (int)((float)c->screen_h * percent);
    cx = (c->screen_w - c->capture_w) / 2;
    cy = (c->screen_h - c->capture_h) / 2;

    if (c->use_shm) {
        if (percent != c->last_percent) {
            reallocate_shm(c, c->capture_w, c->capture_h);
            c->last_percent = percent;
        }
        if (!c->ximg)
            return 0;
        return XShmGetImage(c->dpy, c->root, c->ximg, cx, cy, all_planes) ? 1 : 0;
    }

    teardown_image(c);
    c->ximg = XGetImage(c->dpy, c->root, cx, cy, (unsigned int)c->capture_w,
                        (unsigned int)c->capture_h, all_planes, ZPixmap);
    c->last_percent = percent;
    return c->ximg ? 1 : 0;
}

const uint8_t *nl_capture_data(const struct nl_capture *c, size_t *len)
{
    if (!c->ximg) {
        if (len)
            *len = 0;
        return NULL;
    }
    if (len)
        *len = (size_t)c->ximg->bytes_per_line * (size_t)c->ximg->height;
    return (const uint8_t *)c->ximg->data;
}

int nl_capture_width(const struct nl_capture *c)
{
    return c->capture_w;
}

int nl_capture_height(const struct nl_capture *c)
{
    return c->capture_h;
}

int nl_capture_bpp(const struct nl_capture *c)
{
    if (!c->ximg)
        return 4;
    return c->ximg->bits_per_pixel / 8;
}

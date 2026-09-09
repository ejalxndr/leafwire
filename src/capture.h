#ifndef LW_CAPTURE_H
#define LW_CAPTURE_H

#include <stddef.h>
#include <stdint.h>

#include "leafwire.h"

struct lw_capture;

lw_status lw_capture_new(struct lw_capture **out, const char *display,
                         char errbuf[LW_ERRBUF]);
void lw_capture_free(struct lw_capture *c);
int lw_capture_grab(struct lw_capture *c, float percent);
const uint8_t *lw_capture_data(const struct lw_capture *c, size_t *len);
int lw_capture_width(const struct lw_capture *c);
int lw_capture_height(const struct lw_capture *c);
int lw_capture_bpp(const struct lw_capture *c);

#endif

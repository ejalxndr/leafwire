#ifndef NL_CAPTURE_H
#define NL_CAPTURE_H

#include <stddef.h>
#include <stdint.h>

#include "nlctl.h"

struct nl_capture;

nl_status nl_capture_new(struct nl_capture **out, const char *display,
                         char errbuf[NL_ERRBUF]);
void nl_capture_free(struct nl_capture *c);
int nl_capture_grab(struct nl_capture *c, float percent);
const uint8_t *nl_capture_data(const struct nl_capture *c, size_t *len);
int nl_capture_width(const struct nl_capture *c);
int nl_capture_height(const struct nl_capture *c);
int nl_capture_bpp(const struct nl_capture *c);

#endif

#include "nlctl.h"

struct nl_color nl_color_scaled(struct nl_color c, double brightness)
{
    struct nl_color out;
    out.r = (uint8_t)((double)c.r * brightness);
    out.g = (uint8_t)((double)c.g * brightness);
    out.b = (uint8_t)((double)c.b * brightness);
    return out;
}

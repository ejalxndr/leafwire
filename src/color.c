#include "leafwire.h"

struct lw_color lw_color_scaled(struct lw_color c, double brightness)
{
    struct lw_color out;
    out.r = (uint8_t)((double)c.r * brightness);
    out.g = (uint8_t)((double)c.g * brightness);
    out.b = (uint8_t)((double)c.b * brightness);
    return out;
}

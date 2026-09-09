#ifndef LW_ZONE_H
#define LW_ZONE_H

#include <stddef.h>
#include <stdint.h>

struct lw_zone_color {
    float r;
    float g;
    float b;
};

size_t lw_zone_expected_count(size_t bottom, size_t left, size_t top, size_t right);

size_t lw_zone_analyze(int zone_depth, const uint8_t *data, size_t data_len, int width,
                       int height, int bpp, size_t bottom, size_t left, size_t top,
                       size_t right, struct lw_zone_color *out, size_t out_cap);

#endif

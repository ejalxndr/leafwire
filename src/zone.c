#include "zone.h"

#include <string.h>

static const struct lw_zone_color LW_ZONE_ZERO = { 0.0f, 0.0f, 0.0f };

size_t lw_zone_expected_count(size_t bottom, size_t left, size_t top, size_t right)
{
    if (left < 1 || right < 1 || top < 2)
        return 0;
    return bottom + (left - 1) + (top - 2) + (right - 1);
}

static struct lw_zone_color average_region(const uint8_t *data, size_t data_len,
                                           int width, int bpp, int x0, int y0, int w,
                                           int h)
{
    unsigned long long r = 0, g = 0, b = 0, n = 0;
    struct lw_zone_color out;
    int x, y;

    for (y = y0; y < y0 + h; y++) {
        for (x = x0; x < x0 + w; x++) {
            size_t i = (size_t)((long long)(y * width + x) * bpp);
            if (i + 2 >= data_len)
                continue;
            b += data[i];
            g += data[i + 1];
            r += data[i + 2];
            n++;
        }
    }

    if (n == 0)
        return LW_ZONE_ZERO;

    out.r = (float)((double)r / (double)n / 255.0);
    out.g = (float)((double)g / (double)n / 255.0);
    out.b = (float)((double)b / (double)n / 255.0);
    return out;
}

size_t lw_zone_analyze(int zone_depth, const uint8_t *data, size_t data_len, int width,
                       int height, int bpp, size_t bottom, size_t left, size_t top,
                       size_t right, struct lw_zone_color *out, size_t out_cap)
{
    size_t total = lw_zone_expected_count(bottom, left, top, right);
    size_t idx = 0;
    size_t i;
    int depth;
    int limit;

    if (total == 0 || width <= 0 || height <= 0 || bpp <= 0 || data == NULL)
        return 0;

    limit = (width < height ? width : height) / 2;
    depth = zone_depth < limit ? zone_depth : limit;
    if (depth <= 0)
        return 0;

    for (i = 0; i < bottom && idx < out_cap; i++) {
        size_t rev = bottom - 1 - i;
        int x = (int)(rev * ((size_t)width / bottom));
        int w = (rev == bottom - 1) ? width - x : width / (int)bottom;
        out[idx++] = average_region(data, data_len, width, bpp, x, height - depth, w,
                                    depth);
    }

    for (i = 0; i + 1 < left && idx < out_cap; i++) {
        size_t rev = left - 2 - i;
        int y = (int)(rev * ((size_t)height / left));
        int h = (rev == left - 1) ? height - y : height / (int)left;
        out[idx++] = average_region(data, data_len, width, bpp, 0, y, depth, h);
    }

    for (i = 0; i + 2 < top && idx < out_cap; i++) {
        int x = (int)((i + 1) * ((size_t)width / top));
        out[idx++] = average_region(data, data_len, width, bpp, x, 0, width / (int)top,
                                    depth);
    }

    for (i = 0; i + 1 < right && idx < out_cap; i++) {
        int y = (int)(i * ((size_t)height / right));
        out[idx++] = average_region(data, data_len, width, bpp, width - depth, y, depth,
                                    height / (int)right);
    }

    (void)total;
    return idx;
}

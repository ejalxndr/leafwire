#ifndef LW_LOG_H
#define LW_LOG_H

#include <stdio.h>

#ifndef LW_LOG_PREFIX
#define LW_LOG_PREFIX "lwctl"
#endif

#define lw_info(...)                                     \
    do {                                                 \
        fprintf(stderr, LW_LOG_PREFIX ": ");             \
        fprintf(stderr, __VA_ARGS__);                    \
        fputc('\n', stderr);                             \
    } while (0)

#define lw_warn(...)                                     \
    do {                                                 \
        fprintf(stderr, LW_LOG_PREFIX ": warning: ");    \
        fprintf(stderr, __VA_ARGS__);                    \
        fputc('\n', stderr);                             \
    } while (0)

#define lw_err(...)                                      \
    do {                                                 \
        fprintf(stderr, LW_LOG_PREFIX ": error: ");      \
        fprintf(stderr, __VA_ARGS__);                    \
        fputc('\n', stderr);                             \
    } while (0)

#endif

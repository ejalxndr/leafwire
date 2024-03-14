#ifndef NL_LOG_H
#define NL_LOG_H

#include <stdio.h>

#ifndef NL_LOG_PREFIX
#define NL_LOG_PREFIX "nlctl"
#endif

#define nl_info(...)                                     \
    do {                                                 \
        fprintf(stderr, NL_LOG_PREFIX ": ");             \
        fprintf(stderr, __VA_ARGS__);                    \
        fputc('\n', stderr);                             \
    } while (0)

#define nl_warn(...)                                     \
    do {                                                 \
        fprintf(stderr, NL_LOG_PREFIX ": warning: ");    \
        fprintf(stderr, __VA_ARGS__);                    \
        fputc('\n', stderr);                             \
    } while (0)

#define nl_err(...)                                      \
    do {                                                 \
        fprintf(stderr, NL_LOG_PREFIX ": error: ");      \
        fprintf(stderr, __VA_ARGS__);                    \
        fputc('\n', stderr);                             \
    } while (0)

#endif

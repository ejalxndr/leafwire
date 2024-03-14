#ifndef NL_IPC_H
#define NL_IPC_H

#include <stddef.h>

#include "nlctl.h"

nl_status ipc_read_full(int fd, void *buf, size_t n);
nl_status ipc_write_full(int fd, const void *buf, size_t n);
int ipc_listen_unix(const char *path, char errbuf[NL_ERRBUF]);
int ipc_connect_unix(const char *path, char errbuf[NL_ERRBUF]);

#endif

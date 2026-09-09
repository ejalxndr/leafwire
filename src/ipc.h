#ifndef LW_IPC_H
#define LW_IPC_H

#include <stddef.h>

#include "leafwire.h"

lw_status ipc_read_full(int fd, void *buf, size_t n);
lw_status ipc_write_full(int fd, const void *buf, size_t n);
int ipc_listen_unix(const char *path, char errbuf[LW_ERRBUF]);
int ipc_connect_unix(const char *path, char errbuf[LW_ERRBUF]);

#endif

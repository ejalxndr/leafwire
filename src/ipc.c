#include "ipc.h"

#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <unistd.h>

lw_status ipc_read_full(int fd, void *buf, size_t n)
{
    unsigned char *p = buf;
    size_t got = 0;

    while (got < n) {
        ssize_t r = read(fd, p + got, n - got);
        if (r == 0)
            return LW_ERR_PROTO;
        if (r < 0) {
            if (errno == EINTR)
                continue;
            if (errno == EAGAIN || errno == EWOULDBLOCK)
                return LW_ERR_TIMEOUT;
            return LW_ERR_SOCKET;
        }
        got += (size_t)r;
    }
    return LW_OK;
}

lw_status ipc_write_full(int fd, const void *buf, size_t n)
{
    const unsigned char *p = buf;
    size_t sent = 0;

    while (sent < n) {
        ssize_t w = write(fd, p + sent, n - sent);
        if (w < 0) {
            if (errno == EINTR)
                continue;
            if (errno == EAGAIN || errno == EWOULDBLOCK)
                return LW_ERR_TIMEOUT;
            return LW_ERR_SOCKET;
        }
        sent += (size_t)w;
    }
    return LW_OK;
}

static int fill_addr(struct sockaddr_un *addr, const char *path, char errbuf[LW_ERRBUF])
{
    memset(addr, 0, sizeof(*addr));
    addr->sun_family = AF_UNIX;
    if (strlen(path) >= sizeof(addr->sun_path)) {
        snprintf(errbuf, LW_ERRBUF, "socket path too long: %s", path);
        return -1;
    }
    strcpy(addr->sun_path, path);
    return 0;
}

int ipc_listen_unix(const char *path, char errbuf[LW_ERRBUF])
{
    struct sockaddr_un addr;
    int fd;

    if (fill_addr(&addr, path, errbuf) != 0)
        return -1;

    unlink(path);

    fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd < 0) {
        snprintf(errbuf, LW_ERRBUF, "socket: %s", strerror(errno));
        return -1;
    }

    if (bind(fd, (struct sockaddr *)&addr, sizeof(addr)) != 0) {
        snprintf(errbuf, LW_ERRBUF, "bind %s: %s", path, strerror(errno));
        close(fd);
        return -1;
    }

    if (chmod(path, 0666) != 0) {
        snprintf(errbuf, LW_ERRBUF, "chmod %s: %s", path, strerror(errno));
        close(fd);
        return -1;
    }

    if (listen(fd, 8) != 0) {
        snprintf(errbuf, LW_ERRBUF, "listen %s: %s", path, strerror(errno));
        close(fd);
        return -1;
    }

    return fd;
}

int ipc_connect_unix(const char *path, char errbuf[LW_ERRBUF])
{
    struct sockaddr_un addr;
    int fd;

    if (fill_addr(&addr, path, errbuf) != 0)
        return -1;

    fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd < 0) {
        snprintf(errbuf, LW_ERRBUF, "socket: %s", strerror(errno));
        return -1;
    }

    if (connect(fd, (struct sockaddr *)&addr, sizeof(addr)) != 0) {
        snprintf(errbuf, LW_ERRBUF, "connect %s: %s", path, strerror(errno));
        close(fd);
        return -1;
    }

    return fd;
}

/**
 * @file stub_syscalls.c
 * @brief --wrap で差し替えられるシステムコールスタブの実装
 */
#include <string.h>
#include <sys/socket.h>
#include <poll.h>
#include "stub_syscalls.h"

SyscallStubs g_stubs;

void stubs_reset(void)
{
    memset(&g_stubs, 0, sizeof(g_stubs));
}

int __wrap_socket(int d, int t, int p)
{
    (void)d; (void)t; (void)p;
    g_stubs.socket_calls++;
    return g_stubs.socket_ret;
}

int __wrap_bind(int fd, const struct sockaddr *a, socklen_t l)
{
    (void)fd; (void)a; (void)l;
    g_stubs.bind_calls++;
    return g_stubs.bind_ret;
}

ssize_t __wrap_sendto(int fd, const void *buf, size_t len, int flags,
                      const struct sockaddr *dst, socklen_t dl)
{
    (void)fd; (void)buf; (void)len; (void)flags; (void)dst; (void)dl;
    g_stubs.sendto_calls++;
    return g_stubs.sendto_ret;
}

int __wrap_poll(struct pollfd *fds, nfds_t n, int t)
{
    (void)fds; (void)n; (void)t;
    g_stubs.poll_calls++;
    return g_stubs.poll_ret;
}

ssize_t __wrap_recv(int fd, void *buf, size_t len, int flags)
{
    (void)fd; (void)len; (void)flags;
    g_stubs.recv_calls++;
    if (g_stubs.recv_ret > 0 && buf)
        memset(buf, 0, (size_t)g_stubs.recv_ret);
    return g_stubs.recv_ret;
}

int __wrap_close(int fd)
{
    (void)fd;
    g_stubs.close_calls++;
    return g_stubs.close_ret;
}

int __wrap_unlink(const char *p)
{
    (void)p;
    g_stubs.unlink_calls++;
    return g_stubs.unlink_ret;
}

/**
 * @file socket_ops.h
 * @brief システムコール抽象レイヤー（CMock デモ用）
 */
#ifndef SOCKET_OPS_H
#define SOCKET_OPS_H

#include <sys/types.h>
#include <sys/socket.h>
#include <poll.h>

int     socket_create(int domain, int type, int protocol);
int     socket_bind(int fd, const struct sockaddr *addr, socklen_t len);
ssize_t socket_sendto(int fd, const void *buf, size_t len, int flags,
                      const struct sockaddr *dst, socklen_t dstlen);
int     socket_poll(struct pollfd *fds, nfds_t nfds, int timeout_ms);
ssize_t socket_recv(int fd, void *buf, size_t len, int flags);
int     socket_close(int fd);
int     socket_unlink(const char *path);

#endif /* SOCKET_OPS_H */

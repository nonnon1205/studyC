/**
 * @file mgmt_send.c
 * @brief Client-side send/receive for mgmt commands over UNIX datagram socket
 */
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <poll.h>
#include "mgmt_send.h"

int mgmt_send_command(const char* socket_path,
                      const MgmtCommandRequest* req,
                      MgmtCommandResponse* resp,
                      int timeout_ms)
{
    if (!socket_path || !req || !resp) return -1;

    int fd = socket(AF_UNIX, SOCK_DGRAM, 0);
    if (fd < 0) return -1;

    int ret = -1;

    /* 応答の返送先として一時パスにバインド */
    struct sockaddr_un cli;
    memset(&cli, 0, sizeof(cli));
    cli.sun_family = AF_UNIX;
    snprintf(cli.sun_path, sizeof(cli.sun_path),
             "/tmp/studyc_mgmtctl_%d.sock", (int)getpid());
    unlink(cli.sun_path);

    if (bind(fd, (struct sockaddr*)&cli, sizeof(cli)) < 0)
        goto cleanup;

    struct sockaddr_un srv;
    memset(&srv, 0, sizeof(srv));
    srv.sun_family = AF_UNIX;
    strncpy(srv.sun_path, socket_path, sizeof(srv.sun_path) - 1);

    if (sendto(fd, req, sizeof(MgmtCommandRequest), 0,
               (struct sockaddr*)&srv, sizeof(srv)) < 0)
        goto cleanup;

    struct pollfd pfd = { .fd = fd, .events = POLLIN };
    if (poll(&pfd, 1, timeout_ms) <= 0)
        goto cleanup;

    ssize_t n = recv(fd, resp, sizeof(MgmtCommandResponse), 0);
    if (n == (ssize_t)sizeof(MgmtCommandResponse))
        ret = 0;

cleanup:
    unlink(cli.sun_path);
    close(fd);
    return ret;
}

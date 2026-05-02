#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <pthread.h>
#include <signal.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/time.h>
#include "unified_logger.h"
#define MODULE_NAME "UDP"
#include "debug_log.h"
#include "msg_common.h"

// --- 1. UDPスレッド (正規化してキューへ) ---
void* udp_worker(void* arg) {
    int msqid = *(int*)arg;
    int fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (fd == -1) {
        GLOG_ERR("[UDP] socket: %s", strerror(errno));
        return NULL;
    }

    struct sockaddr_in addr = {0};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(UDP_PORT);
    addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(fd, (struct sockaddr *)&addr, sizeof(addr)) != 0) {
        GLOG_ERR("[UDP] bind: %s", strerror(errno));
        close(fd);
        return NULL;
    }

    char buf[256];
    GLOG_INFO("[UDP] 待機中 (Port: %d)", UDP_PORT);

    int stop_fd = g_shutdown_pipe[0];
    if (stop_fd == -1) {
        GLOG_ERR("[UDP] no shutdown pipe available");
        close(fd);
        return NULL;
    }

    while (atomic_load_explicit(&g_keep_running, memory_order_acquire)) {
        fd_set readfds;
        FD_ZERO(&readfds);
        FD_SET(fd, &readfds);
        FD_SET(stop_fd, &readfds);
        int nfds = (fd > stop_fd ? fd : stop_fd) + 1;

        int ready = select(nfds, &readfds, NULL, NULL, NULL);
        if (ready < 0) {
            if (errno == EINTR) continue;
            GLOG_ERR("[UDP] select: %s", strerror(errno));
            break;
        }
        if (FD_ISSET(stop_fd, &readfds)) {
            GLOG_INFO("[UDP] シャットダウンシグナルを受信しました。ループを抜けます。");
            break;
        }
        if (FD_ISSET(fd, &readfds)) {
            int n = recvfrom(fd, buf, sizeof(buf) - 1, 0, NULL, NULL);
            if (n < 0) {
                if (errno == EINTR) continue;
                GLOG_ERR("[UDP] recvfrom: %s", strerror(errno));
                break;
            }
            if (n == 0) continue;

            buf[n] = '\0';
            DBG("UDP受信: %d bytes, data=\"%s\"", n, buf);
            if (strcmp(buf, UDP_QUIT_CMD) == 0) {
                GLOG_INFO("[UDP] 外部 QUIT を受信しました。終了処理はシグナルを待ちます。");
                continue;
            }
            send_udp_event(msqid, buf);
            DBG("UDPイベントをメインキューへ送信");
        }
    }

    close(fd);
    return NULL;
}

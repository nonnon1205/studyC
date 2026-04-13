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
#include "msg_common.h"

// --- 1. UDPスレッド (正規化してキューへ) ---
void* udp_worker(void* arg) {
    int msqid = *(int*)arg;
    int fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (fd == -1) {
        perror("socket");
        return NULL;
    }

    struct sockaddr_in addr = {0};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(UDP_PORT);
    addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(fd, (struct sockaddr *)&addr, sizeof(addr)) != 0) {
        perror("bind");
        close(fd);
        return NULL;
    }

    char buf[256];
    printf("[UDP] 待機中 (Port: %d)\n", UDP_PORT);
    while (1) {
        int n = recvfrom(fd, buf, sizeof(buf) - 1, 0, NULL, NULL);
        if (n < 0) {
            if (errno == EINTR) continue;
            perror("recvfrom");
            break;
        }
        if (n == 0) continue;

        buf[n] = '\0';
        if (strncmp(buf, "QUIT", 4) == 0) {
            send_quit_event(msqid);
            break;
        }
        send_udp_event(msqid, buf);
    }

    close(fd);
    return NULL;
}

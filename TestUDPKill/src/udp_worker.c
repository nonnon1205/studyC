#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <signal.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/msg.h>
#include <sys/ipc.h>
#include "common.h"

// --- 1. UDPスレッド ---
void* udp_worker(void* arg) {
    AppContext *ctx = (AppContext *)arg;
    struct sockaddr_in servaddr, cliaddr;
    char buffer[1024];
    socklen_t len;

    ctx->udp_fd = socket(AF_INET, SOCK_DGRAM, 0);
    memset(&servaddr, 0, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = INADDR_ANY;
    servaddr.sin_port = htons(UDP_PORT);

    if (bind(ctx->udp_fd, (const struct sockaddr *)&servaddr, sizeof(servaddr)) < 0) {
        perror("bind failed");
        return NULL;
    }

    printf("[UDP] 待機中...\n");
    while (1) {
        len = sizeof(cliaddr);
        int n = recvfrom(ctx->udp_fd, buffer, sizeof(buffer)-1, 0, (struct sockaddr *)&cliaddr, &len);
        if (n < 0) break;
        buffer[n] = '\0';

        if (strncmp(buffer, "QUIT", 4) == 0) {
            printf("[UDP] 終了メッセージを受信。ループを抜けます。\n");
            break;
        }
    }
    return NULL;
}
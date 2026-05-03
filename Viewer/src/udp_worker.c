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
#include "udp_common.h"
#define MODULE_NAME "UDP"
#include "debug_log.h"

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

    // src/udp_worker.c の while ループ内を以下のように修正

    printf("[UDP] 待機中...\n");
    while (1) {
        len = sizeof(cliaddr);
        ssize_t n = recvfrom(ctx->udp_fd, buffer, sizeof(buffer)-1, 0, (struct sockaddr *)&cliaddr, &len);
        if (n < 0) break;
        buffer[n] = '\0';
        DBG("UDP受信: %zd bytes, data=\"%s\"", n, buffer);

        // パターンA: 外部からの終了「要求」
        if (strncmp(buffer, "QUIT", 4) == 0) {
            DBG("QUIT受信");
            printf("[UDP] 外部から終了要求(QUIT)を受信。メインに報告します。\n");
            pthread_mutex_lock(&ctx->mtx);
            ctx->shutdown_requested = 1;
            pthread_cond_signal(&ctx->cond);
            pthread_mutex_unlock(&ctx->mtx);
            // ★超重要: ここで break しない！次の recvfrom に戻る
            continue; 
        }

        // パターンB: メインスレッドからの致死命令（Poison Pill）
        if (strncmp(buffer, "POISON_PILL", 11) == 0) {
            DBG("POISON_PILL受信");
            printf("[UDP] メインから致死命令(POISON_PILL)を受信。自害します。\n");
            break;
        }
        // （その他の通常のUDPパケット処理があればここに書く）
    }

    return NULL;
}
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
#include <arpa/inet.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include "common.h"

static void send_local_quit(void) {
    int fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (fd < 0) {
        perror("socket");
        return;
    }

    struct sockaddr_in dest = {0};
    dest.sin_family = AF_INET;
    dest.sin_port = htons(UDP_PORT);
    dest.sin_addr.s_addr = htonl(INADDR_LOOPBACK);

    if (sendto(fd, "QUIT", 4, 0, (struct sockaddr *)&dest, sizeof(dest)) < 0) {
        perror("sendto");
    }
    close(fd);
}

int main() {
    int msqid = msgget(MSG_KEY, 0666 | IPC_CREAT);
    if (msqid == -1) {
        perror("msgget");
        return EXIT_FAILURE;
    }

    pthread_t t1, t2;
    sigset_t set;

    // シグナルブロック
    sigemptyset(&set);
    sigaddset(&set, SIGINT);
    sigaddset(&set, SIGTERM);
    if (pthread_sigmask(SIG_BLOCK, &set, NULL) != 0) {
        perror("pthread_sigmask");
        msgctl(msqid, IPC_RMID, NULL);
        return EXIT_FAILURE;
    }

    // スレッド起動
    if (pthread_create(&t1, NULL, udp_worker, &msqid) != 0) {
        perror("pthread_create udp_worker");
        msgctl(msqid, IPC_RMID, NULL);
        return EXIT_FAILURE;
    }
    if (pthread_create(&t2, NULL, signal_worker, &msqid) != 0) {
        perror("pthread_create signal_worker");
        pthread_cancel(t1);
        pthread_join(t1, NULL);
        msgctl(msqid, IPC_RMID, NULL);
        return EXIT_FAILURE;
    }

    printf("[Main] 指揮官、msgrcvにてイベント待機を開始します。\n");

    InternalMsg rx_msg;
    int keep_running = 1;

    while (keep_running) {
        // ここで全イベントを一本化して待機（CPU負荷 0）
        if (msgrcv(msqid, &rx_msg, sizeof(InternalMsg) - sizeof(long), 0, 0) == -1) {
            if (errno == EINTR) continue;
            perror("msgrcv");
            break;
        }

        switch (rx_msg.event) {
            case EV_QUIT:
                printf("[Main] 終了命令を受信。システムを停止します。\n");
                keep_running = 0;
                if (pthread_kill(t2, SIGINT) != 0) {
                    perror("pthread_kill");
                }
                break;
            case EV_UDP:
                printf("[Main] UDPデータ受信: %s\n", rx_msg.data.udp_payload);
                break;
            case EV_SIGNAL:
                printf("[Main] シグナル(%d)を検知しました。\n", rx_msg.data.sig_num);
                if (rx_msg.data.sig_num == SIGINT) {
                    keep_running = 0;
                    send_local_quit();
                }
                break;
            default:
                break;
        }
    }

    // 終了シーケンス
    printf("[Main] 各スレッドを回収中...\n");
    pthread_join(t1, NULL);
    pthread_join(t2, NULL);
    if (msgctl(msqid, IPC_RMID, NULL) == -1) {
        perror("msgctl(IPC_RMID)");
    }
    printf("[Main] リソース解放完了。正常終了。\n");

    return 0;
}

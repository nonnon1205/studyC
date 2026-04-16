#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "log_wrapper.h"
#include "router_worker.h"
#include "msg_common.h"
#define DEST_TCP_PORT 7777 // 最終目的地(TestUDPKill)のポート

// このスレッドに渡す引数用の構造体（mainでセットして渡す想定）


void* router_worker(void* arg) {
    RouterContext* ctx = (RouterContext*)arg;
    IpcNotifyMessage notify;
    
    printf("[Router] MQの監視、およびTCPゲートウェイを開始します...\n");

    // 1. TCPクライアントソケットの準備
    int tcp_sock = socket(AF_INET, SOCK_STREAM, 0);
    int tcp_connected = 0;
    struct sockaddr_in dest_addr = {0};
    dest_addr.sin_family = AF_INET;
    dest_addr.sin_port = htons(DEST_TCP_PORT);
    inet_pton(AF_INET, "127.0.0.1", &dest_addr.sin_addr);

    // ※簡単のため、起動時に1回だけ接続を試みます
    // 本格的にやるなら、ここで接続失敗してもリトライするループを入れます
    if (tcp_sock < 0) {
        log_err("[Router] socket: %s", strerror(errno));
        send_fatal_event(ctx->main_msqid, "router_worker: socket open failure");
        return NULL;
    }
    if (connect(tcp_sock, (struct sockaddr*)&dest_addr, sizeof(dest_addr)) < 0) {
        perror("[Router] TCPサーバー(TestUDPKill)への接続に失敗しました。プロセスを終了します");
        close(tcp_sock);
        send_fatal_event(ctx->main_msqid, "router_worker: TCP connect failure");
        return NULL;
    }
    tcp_connected = 1;
    printf("[Router] TestUDPKill (Port: %d) とTCP接続確立！\n", DEST_TCP_PORT);

    // 2. Control Plane (MQ) の監視ループ
    while (1) {
        // MSG_TYPE_SHM_NOTIFY (1) の通知が来るまで深くブロックして待つ！
        if (msgrcv(ctx->ipc_msqid, &notify, sizeof(IpcNotifyMessage) - sizeof(long), MSG_TYPE_SHM_NOTIFY, 0) != -1) {
            if (notify.shm_status_id == MSG_TYPE_SHM_QUIT) {
                printf("[Router] SHM終了通知を受信しました。ループを抜けます。\n");
                break;
            }
            
            // 3. 通知を受けたら Data Plane (SHM) から重いデータを回収
            int status_read;
            char payload[1024] = {0};
            
            if (shm_api_read(ctx->shm_handle, &status_read, payload)) {
                printf("  -> [Router] SHM(status=%d)からデータ読出成功: %s\n", status_read, payload);
                
                // 4. TCPに乗せ換えて TestUDPKill へ発射！
                if (tcp_connected && tcp_sock >= 0) {
                    // TCP通信なので、パケットの区切り(改行など)を付けて送るのが親切です
                    char tcp_buf[1100];
                    snprintf(tcp_buf, sizeof(tcp_buf), "[TCP-RELAY] %s\n", payload);
                    
                    if (send(tcp_sock, tcp_buf, strlen(tcp_buf), 0) > 0) {
                        printf("  -> [Router] TCPでTestUDPKillへ転送完了\n");
                    } else {
                        perror("  -> [Router] TCP送信エラー");
                    }
                }
            } else {
                printf("  -> [Router] SHMの読み取り失敗\n");
            }
        }
    }
    
    close(tcp_sock);
    return NULL;
}
#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "unified_logger.h"
#define MODULE_NAME "Router"
#include "debug_log.h"
#include "router_worker.h"
#include "msg_common.h"
#define DEST_TCP_PORT 7777
#define CONNECT_MAX_RETRIES 5
#define CONNECT_RETRY_DELAY_S 3


void* router_worker(void* arg) {
    RouterContext* ctx = (RouterContext*)arg;
    IpcNotifyMessage notify;

    GLOG_INFO("[Router] MQの監視、およびTCPゲートウェイを開始します...");

    // 1. TCPクライアントソケットの準備
    int tcp_sock = socket(AF_INET, SOCK_STREAM, 0);
    int tcp_connected = 0;
    struct sockaddr_in dest_addr = {0};
    dest_addr.sin_family = AF_INET;
    dest_addr.sin_port = htons(DEST_TCP_PORT);
    inet_pton(AF_INET, "127.0.0.1", &dest_addr.sin_addr);

    if (tcp_sock < 0) {
        GLOG_ERR("[Router] socket: %s", strerror(errno));
        send_fatal_event(ctx->main_msqid, "router_worker: socket open failure");
        return NULL;
    }

    // TCPサーバーへの接続をリトライする
    for (int i = 0; i < CONNECT_MAX_RETRIES; i++) {
        DBG("TCP接続試行 #%d to 127.0.0.1:%d", i + 1, DEST_TCP_PORT);
        if (connect(tcp_sock, (struct sockaddr*)&dest_addr, sizeof(dest_addr)) == 0) {
            tcp_connected = 1;
            break;
        }
        GLOG_WARN("[Router] TCP接続試行 #%d 失敗: %s. %d秒後にリトライします...", i + 1, strerror(errno), CONNECT_RETRY_DELAY_S);

        int stop_fd = g_shutdown_pipe[0];
        fd_set readfds;
        FD_ZERO(&readfds);
        FD_SET(stop_fd, &readfds);
        struct timeval tv = {CONNECT_RETRY_DELAY_S, 0};

        int ready = select(stop_fd + 1, &readfds, NULL, NULL, &tv);
        if (ready > 0) {
            GLOG_INFO("[Router] リトライ待機中にシャットダウン要求を受信。処理を中断します。");
            close(tcp_sock);
            return NULL;
        }
    }

    if (!tcp_connected) {
        GLOG_ERR("[Router] TCPサーバー(Viewer)への接続に複数回失敗しました。プロセスを終了します。");
        close(tcp_sock);
        send_fatal_event(ctx->main_msqid, "router_worker: TCP connect failure after retries");
        return NULL;
    }
    tcp_connected = 1;
    GLOG_INFO("[Router] Viewer (Port: %d) とTCP接続確立！", DEST_TCP_PORT);

    // 2. Control Plane (MQ) の監視ループ
    while (1) {
        if (msgrcv(ctx->ipc_msqid, &notify, sizeof(IpcNotifyMessage) - sizeof(long), MSG_TYPE_SHM_NOTIFY, 0) != -1) {
            DBG("MQ通知受信: shm_status_id=%d", notify.shm_status_id);
            if (notify.shm_status_id == MSG_TYPE_SHM_QUIT) {
                GLOG_INFO("[Router] SHM終了通知を受信しました。ループを抜けます。");
                break;
            }

            // 3. 通知を受けたら Data Plane (SHM) から重いデータを回収
            int status_read;
            char payload[1024] = {0};

            if (shm_api_read(ctx->shm_handle, &status_read, payload)) {
                DBG("SHM読出し成功: status=%d, payload=\"%s\"", status_read, payload);
                GLOG_INFO("  -> [Router] SHM(status=%d)からデータ読出成功: %s", status_read, payload);

                // 4. TCPに乗せ換えて Viewer へ転送
                if (tcp_connected && tcp_sock >= 0) {
                    char tcp_buf[1100];
                    snprintf(tcp_buf, sizeof(tcp_buf), "[TCP-RELAY] %s\n", payload);

                    if (send(tcp_sock, tcp_buf, strlen(tcp_buf), 0) > 0) {
                        DBG("TCP送信完了: %zu bytes", strlen(tcp_buf));
                        GLOG_INFO("  -> [Router] TCPでViewerへ転送完了");
                    } else {
                        GLOG_ERR("  -> [Router] TCP送信エラー: %s", strerror(errno));
                    }
                }
            } else {
                GLOG_ERR("  -> [Router] SHMの読み取り失敗");
            }
        }
    }

    close(tcp_sock);
    return NULL;
}

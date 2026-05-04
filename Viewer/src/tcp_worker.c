#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <poll.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "udp_common.h"
#define MODULE_NAME "TCP"
#include "debug_log.h"
#include "network_config.h"
#include "unified_logger.h"

#define TCP_RECV_BUF_SIZE 2048

void* tcp_worker(void* arg) {
    AppContext* ctx = (AppContext*)arg;
    
    int server_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (server_sock < 0) return NULL;

    int opt = 1;
    setsockopt(server_sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in server_addr = {0};
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    int listen_port = get_network_tcp_port();
    server_addr.sin_port = htons(listen_port);

    if (bind(server_sock, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("[TCP] bind");
        close(server_sock);
        return NULL;
    }

    if (listen(server_sock, 5) < 0) {
        close(server_sock);
        return NULL;
    }

    GLOG_INFO("[TCP View] ポート %d でクライアントからの接続を待機中...", listen_port);

    int client_sock = -1;
    struct pollfd fds[2];

    // CondVar で管理されているフラグを監視
    while (ctx->shutdown_requested == 0) {
        // 1. メインからの終了パイプを監視
        fds[0].fd = ctx->shutdown_pipe[0];
        fds[0].events = POLLIN;

        // 2. ソケットの状態を監視
        if (client_sock < 0) {
            fds[1].fd = server_sock;
            fds[1].events = POLLIN;
        } else {
            fds[1].fd = client_sock;
            fds[1].events = POLLIN;
        }

        int ret = poll(fds, 2, -1);
        if (ret < 0) {
            if (errno == EINTR) continue;
            break;
        }

        // --- メインから終了通知(パイプ書き込み)が来た場合 ---
        if (fds[0].revents & POLLIN) {
            GLOG_INFO("[TCP View] 終了通知を受信しました。ループを抜けます。");
            break; 
        }

        // --- ネットワークイベント ---
        if (fds[1].revents & POLLIN) {
            if (client_sock < 0) {
                struct sockaddr_in client_addr = {0};
                socklen_t client_len = sizeof(client_addr);
                client_sock = accept(server_sock, (struct sockaddr*)&client_addr, &client_len);
                if (client_sock >= 0) {
                    DBG("TCP新規接続受付: client_sock=%d", client_sock);
                    GLOG_INFO("[TCP View] 🟢 クライアントが接続しました！");
                }
            } else {
                char buffer[TCP_RECV_BUF_SIZE];
                ssize_t bytes_read = recv(client_sock, buffer, sizeof(buffer) - 1, 0);
                if (bytes_read > 0) {
                    buffer[bytes_read] = '\0';
                    DBG("TCP受信: %zd bytes", bytes_read);
                    printf("----------------------------------------\n");
                    printf("📥 【TCP受信データ】\n%s", buffer);
                    printf("----------------------------------------\n\n");
                } else {
                    DBG("TCP切断検知: recv=%zd", bytes_read);
                    GLOG_INFO("[TCP View] 🔴 クライアントが切断しました。");
                    close(client_sock);
                    client_sock = -1;
                }
            }
        }
    }

    // クリーンアップ
    if (client_sock >= 0) close(client_sock);
    close(server_sock);
    GLOG_INFO("[TCP View] リソースを解放して終了します。");
    
    return NULL;
}
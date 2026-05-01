/**
 * @file main.c
 * @brief Collector エントリポイント：UDP 受信 → SHM 書込 + MQ 通知
 */
#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include "pollIo_common.h"
#include "shared_ipc.h"
#include "shm_api.h"
#include "unified_logger.h"
#include "event_handler.h"
#include "mgmt_socket.h"
#include "mgmt_paths.h"
#include "mgmt_handlers.h"

volatile sig_atomic_t g_keep_running = 1;

void sig_handler(int signum) {
    (void)signum;
    g_keep_running = 0;
}

int main(void) {
    struct sigaction sa;
    sa.sa_handler = sig_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(SIGINT,  &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);

    log_init("Collector");

    int ipc_msqid = msgget(SYSTEM_IPC_KEY, 0666 | IPC_CREAT);
    ShmHandle shm_handle = shm_api_init();
    if (!shm_handle) {
        GLOG_FATAL("共有メモリの初期化に失敗しました。");
        log_close();
        return EXIT_FAILURE;
    }

    /* mgmt: ハンドラレジストリ初期化 → ハンドラ登録 */
    if (handler_registry_init() < 0) {
        GLOG_WARN("[Mgmt] ハンドラレジストリの初期化に失敗");
    } else {
        CollectorMgmtCtx mgmt_ctx = {
            .start_time   = time(NULL),
            .keep_running = &g_keep_running
        };
        if (collector_mgmt_register(&mgmt_ctx) < 0)
            GLOG_WARN("[Mgmt] ハンドラ登録に失敗");
    }

    /* mgmt: ソケット作成（失敗しても本体は動作継続） */
    MgmtSocketHandle mgmt_sock =
        mgmt_socket_create(MGMT_SOCKET_PATH_COLLECTOR);
    if (!mgmt_sock)
        GLOG_WARN("[Mgmt] 管理ソケットの作成に失敗: %s",
                  MGMT_SOCKET_PATH_COLLECTOR);

    GLOG_INFO("==========================================");
    GLOG_INFO(" Collector 起動");
    GLOG_INFO("==========================================");

    int udp_fd = setup_udp_socket(UDP_PORT);
    if (udp_fd < 0) {
        if (mgmt_sock) mgmt_socket_destroy(mgmt_sock);
        handler_registry_destroy();
        shm_api_close(shm_handle);
        log_close();
        return EXIT_FAILURE;
    }

    run_event_loop(udp_fd, ipc_msqid, shm_handle, mgmt_sock);

    /* 終了処理 */
    shm_api_close(shm_handle);
    close_udp_socket(udp_fd);
    if (mgmt_sock) mgmt_socket_destroy(mgmt_sock);
    handler_registry_destroy();

    GLOG_INFO("[Main] リソースを解放し、システムをクリーンに終了しました。");
    log_close();
    return 0;
}

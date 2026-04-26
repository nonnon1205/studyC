#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include "pollIo_common.h"
#include "shared_ipc.h"
#include "shm_api.h"

// シングルスレッド特権：シグナルで書き換えるだけのシンプルな終了フラグ
volatile sig_atomic_t g_keep_running = 1;

void sig_handler(int signum) {
    (void)signum;
    g_keep_running = 0; // ループを止めるフラグを立てる
}

int main() {
    // 1. シグナルハンドラの登録 (Ctrl+C など)
    struct sigaction sa;
    sa.sa_handler = sig_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);

    // --- 追加: MQとSHMの初期化 ---
    int ipc_msqid = msgget(SYSTEM_IPC_KEY, 0666 | IPC_CREAT);
    ShmHandle shm_handle = shm_api_init();
    if (!shm_handle) {
        fprintf(stderr, "共有メモリの初期化に失敗しました。\n");
        return EXIT_FAILURE;
    }

    printf("==========================================\n");
    printf(" Collector 起動\n");
    printf("==========================================\n");

    // 2. 外部からUDPを受け取るための自局ソケット設定（例: 9999番ポート）
    int udp_fd = setup_udp_socket(UDP_PORT);
    if (udp_fd < 0) {
        return EXIT_FAILURE;
    }

    // run_event_loop の引数を増やして呼び出す
    run_event_loop(udp_fd, ipc_msqid, shm_handle);

    // 終了処理に追加
    shm_api_close(shm_handle);

    // 4. クリーンアップ
    close_udp_socket(udp_fd);
    printf("[Main] リソースを解放し、システムをクリーンに終了しました。\n");

    return 0;
}
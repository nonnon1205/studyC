#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include "pollIo_common.h"

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

    printf("==========================================\n");
    printf(" PollIO 統合コントローラー起動\n");
    printf("==========================================\n");

    // 2. 外部からUDPを受け取るための自局ソケット設定（例: 9999番ポート）
    int udp_fd = setup_udp_socket(UDP_PORT);
    if (udp_fd < 0) {
        return EXIT_FAILURE;
    }

    // 3. 全てを poll() に委ねる
    run_event_loop(udp_fd);

    // 4. クリーンアップ
    close_udp_socket(udp_fd);
    printf("[Main] リソースを解放し、システムをクリーンに終了しました。\n");

    return 0;
}
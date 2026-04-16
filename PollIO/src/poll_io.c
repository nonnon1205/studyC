#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <poll.h>
#include <errno.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include "pollIo_common.h"
#include <signal.h>
#include "shared_ipc.h"

extern volatile sig_atomic_t g_keep_running;

// --- 1. 初期化系 ---
int setup_udp_socket(uint16_t port) {
    int fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (fd < 0) {
        perror("socket");
        return -1;
    }
    struct sockaddr_in addr = {0};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port);

    if (bind(fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("bind");
        close(fd);
        return -1;
    }
    printf("[Setup] 監視用UDPポート(%d) バインド完了\n", port);
    return fd;
}

void close_udp_socket(int fd) {
    if (fd >= 0) close(fd);
}

// --- 2. イベントハンドラ系 ---
bool handle_stdin_read(int udp_fd) {
    char input[256];
    if (!fgets(input, sizeof(input), stdin)) return false;
    input[strcspn(input, "\n")] = '\0'; // 改行削除

    if (strlen(input) == 0) return true;

    if (strcmp(input, "quit") == 0) {
        return false; // ループ終了の合図
    }
    else if (strncmp(input, "udp ", 4) == 0) {
        // TestMsgRcv (8888番ポート) にパケットを撃ち込む
        struct sockaddr_in dest = {0};
        dest.sin_family = AF_INET;
        dest.sin_port = htons(UDP_SEND_PORT);
        inet_pton(AF_INET, "127.0.0.1", &dest.sin_addr);

        const char* payload = input + 4;
        sendto(udp_fd, payload, strlen(payload), 0, (struct sockaddr*)&dest, sizeof(dest));
        printf("  -> [UDP送信] 8888番へ: %s\n", payload);
    }
    else {
        printf("  コマンド: quit, udp <msg>\n");
    }
    return true;
}

void handle_udp_read(int udp_fd) {
    char buffer[1024];
    struct sockaddr_in cliaddr;
    socklen_t len = sizeof(cliaddr);

    int n = recvfrom(udp_fd, buffer, sizeof(buffer)-1, 0, (struct sockaddr *)&cliaddr, &len);
    if (n > 0) {
        buffer[n] = '\0';
        printf("\n  <- [UDP受信] %s:%d より: %s\n", 
               inet_ntoa(cliaddr.sin_addr), ntohs(cliaddr.sin_port), buffer);
        
        // ※将来、ここに「受信したバッファをIPC(メッセージキュー)へ流し込む」
        // 処理を1行追加するだけで、完璧な中継プロキシが完成します。
    }
}

// --- 3. メインループ (Reactorコア) ---
void run_event_loop(int udp_fd) {
    struct pollfd fds[2];

    // 監視対象1: キーボード(標準入力)
    fds[0].fd = STDIN_FILENO;
    fds[0].events = POLLIN;

    // 監視対象2: UDPソケット
    fds[1].fd = udp_fd;
    fds[1].events = POLLIN;

    printf("[PollIO] イベントループ開始 (Ctrl+C または 'quit' で終了)\n");
    printf("> ");
    fflush(stdout);

    while (g_keep_running) {
        // ここでブロック！キーボードかUDP、どちらかにデータが来るまでOSが寝かせてくれる
        int ret = poll(fds, 2, -1);

        if (ret < 0) {
            if (errno == EINTR) {
                // シグナル(Ctrl+C)で割り込まれた場合は、安全にループを抜ける
                break;
            }
            perror("[PollIO] poll error");
            break;
        }

        // キーボードから入力があった場合
        if (fds[0].revents & POLLIN) {
            if (!handle_stdin_read(udp_fd)) {
                break; // quitが入力された
            }
            printf("> ");
            fflush(stdout);
        }

        // UDPパケットが届いた場合
        if (fds[1].revents & POLLIN) {
            handle_udp_read(udp_fd);
            printf("> ");
            fflush(stdout);
        }
    }
}
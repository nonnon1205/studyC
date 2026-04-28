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
#include "shm_api.h"
#include "unified_logger.h"

extern volatile sig_atomic_t g_keep_running;

// --- 1. 初期化系 ---
int setup_udp_socket(uint16_t port) {
    int fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (fd < 0) {
        GLOG_ERR("[Setup] UDPソケット作成失敗: %s", strerror(errno));
        return -1;
    }
    struct sockaddr_in addr = {0};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port);

    if (bind(fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        GLOG_ERR("[Setup] UDPポート(%d) バインド失敗: %s", port, strerror(errno));
        close(fd);
        return -1;
    }
    GLOG_INFO("[Setup] 監視用UDPポート(%d) バインド完了", port);
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
        // Router (8888番ポート) にパケットを撃ち込む
        struct sockaddr_in dest = {0};
        dest.sin_family = AF_INET;
        dest.sin_port = htons(UDP_SEND_PORT);
        inet_pton(AF_INET, "127.0.0.1", &dest.sin_addr);

        const char* payload = input + 4;
        sendto(udp_fd, payload, strlen(payload), 0, (struct sockaddr*)&dest, sizeof(dest));
        GLOG_DEBUG("[UDP送信] Router(8888)へ: %s", payload);
    }
    else {
        GLOG_INFO("コマンド: quit, udp <msg>");
    }
    return true;
}

// ※ main関数などで shm_handle と ipc_msqid を初期化し、引数として渡してくる想定です
void handle_udp_read(int udp_fd, int ipc_msqid, ShmHandle shm_handle) {
    char buffer[1024];
    struct sockaddr_in cliaddr;
    socklen_t len = sizeof(cliaddr);

    int n = recvfrom(udp_fd, buffer, sizeof(buffer)-1, 0, (struct sockaddr *)&cliaddr, &len);
    if (n > 0) {
        buffer[n] = '\0';
        char src_ip[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &cliaddr.sin_addr, src_ip, sizeof(src_ip));
        GLOG_INFO("[Collector] %s:%d からUDP受信: %s", src_ip, ntohs(cliaddr.sin_port), buffer);

        int status_id = 100;

        if (shm_api_write(shm_handle, status_id, buffer)) {
            GLOG_DEBUG("[SHM] ペイロード書き込み成功 (StatusID: %d)", status_id);

            IpcNotifyMessage notify;
            notify.mtype = MSG_TYPE_SHM_NOTIFY;
            notify.shm_status_id = status_id;

            if (msgsnd(ipc_msqid, &notify, sizeof(IpcNotifyMessage) - sizeof(long), IPC_NOWAIT) == 0) {
                GLOG_DEBUG("[MQ] Routerへ通知完了");
            } else {
                GLOG_ERR("[MQ] 通知失敗: %s", strerror(errno));
            }
        } else {
            GLOG_WARN("[SHM] 書き込み失敗 (Mutexロック等による)");
        }
    }
}

// --- 3. メインループ (Reactorコア) ---
void run_event_loop(int udp_fd,int ipc_msqid, ShmHandle shm_handle) {
    struct pollfd fds[2];

    // 監視対象1: キーボード(標準入力)
    fds[0].fd = STDIN_FILENO;
    fds[0].events = POLLIN;

    // 監視対象2: UDPソケット
    fds[1].fd = udp_fd;
    fds[1].events = POLLIN;

    GLOG_INFO("[Collector] イベントループ開始 (Ctrl+C または 'quit' で終了)");

    while (g_keep_running) {
        // ここでブロック！キーボードかUDP、どちらかにデータが来るまでOSが寝かせてくれる
        int ret = poll(fds, 2, -1);

        if (ret < 0) {
            if (errno == EINTR) {
                // シグナル(Ctrl+C)で割り込まれた場合は、安全にループを抜ける
                break;
            }
            GLOG_ERR("[Collector] poll error: %s", strerror(errno));
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
            handle_udp_read(udp_fd, ipc_msqid, shm_handle);
            printf("> ");
            fflush(stdout);
        }
    }
}
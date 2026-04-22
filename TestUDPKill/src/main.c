#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <signal.h>
#include <unistd.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "udp_common.h"

int main() {
    pthread_t t_udp, t_ipc, t_sig, t_tcp;
    AppContext ctx;
    sigset_t set;

    // 前準備
    sigemptyset(&set);
    sigaddset(&set, SIGTERM);
    sigaddset(&set, SIGINT);
    sigaddset(&set, SIGUSR1);
    pthread_sigmask(SIG_BLOCK, &set, NULL);

    pthread_mutex_init(&ctx.mtx, NULL);
    pthread_cond_init(&ctx.cond, NULL);
    ctx.shutdown_requested = 0;
    ctx.signal_thread_ready = false;

    // ★追加: TCPスレッドを叩き起こす用のパイプを作成
    if (pipe(ctx.shutdown_pipe) < 0) {
        perror("pipe");
        return EXIT_FAILURE;
    }

    pthread_create(&t_sig, NULL, signal_worker, (void *)&ctx);

    pthread_mutex_lock(&ctx.mtx);
    while (!ctx.signal_thread_ready) {
        pthread_cond_wait(&ctx.cond, &ctx.mtx);
    }
    pthread_mutex_unlock(&ctx.mtx);

    // /* UDP worker disabled
    // pthread_create(&t_udp, NULL, udp_worker, (void *)&ctx);
    // */
    pthread_create(&t_ipc, NULL, ipc_worker, (void *)&ctx);
    pthread_create(&t_tcp, NULL, tcp_worker, (void *)&ctx);

    // いずれかのルートで終了フラグが立つのを待つ
    pthread_mutex_lock(&ctx.mtx);
    while (ctx.shutdown_requested == 0) {
        pthread_cond_wait(&ctx.cond, &ctx.mtx);
    }
    pthread_mutex_unlock(&ctx.mtx);

    printf("\n[Main] --- 終了通知シーケンス開始 ---\n");

    // /* UDP worker disabled - Poison Pill
    // 1. UDPスレッドに致死命令（Poison Pill）を送る
    // struct sockaddr_in dest;
    // dest.sin_family = AF_INET;
    // dest.sin_port = htons(UDP_PORT); // 既存のUDPポート定義
    // inet_pton(AF_INET, "127.0.0.1", &dest.sin_addr);
    // int tmp_fd = socket(AF_INET, SOCK_DGRAM, 0);
    // sendto(tmp_fd, "POISON_PILL", 11, 0, (struct sockaddr *)&dest, sizeof(dest));
    // close(tmp_fd);
    // */

    // 2. IPCスレッドを叩き起こす
    struct msg_buffer stop_msg = {1, "EXIT"};
    msgsnd(ctx.msqid, &stop_msg, sizeof(stop_msg.msg_text), 0);

    // 3. Signalスレッドを叩き起こす
    pthread_kill(t_sig, SIGUSR1);

    // ★追加: 4. TCPスレッド(View)をパイプで叩き起こす
    char dummy = 'X';
    if (write(ctx.shutdown_pipe[1], &dummy, 1) < 0) {
        perror("write to shutdown_pipe");
    }
    
    // 各スレッドの合流を待つ
    // /* UDP worker disabled
    // pthread_join(t_udp, NULL);
    // */
    pthread_join(t_ipc, NULL);
    pthread_join(t_sig, NULL);
    pthread_join(t_tcp, NULL);

    // 最終後片付け
    msgctl(ctx.msqid, IPC_RMID, NULL);
    close(ctx.udp_fd);
    close(ctx.shutdown_pipe[0]); // ★パイプの片付け
    close(ctx.shutdown_pipe[1]); // ★パイプの片付け
    printf("[Main] 全てのスレッドを回収。リソースを解放しました。\n");

    return 0;
}
#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <signal.h>
#include <pthread.h>
#include "udp_common.h"

void* signal_worker(void* arg) {
    // 修正: 必要な情報をすべて持った ctx を受け取る
    AppContext *ctx = (AppContext *)arg; 
    
    // sigset_t はメインスレッドと同じものをここで作ればOKです
    sigset_t set;
    sigemptyset(&set);
    sigaddset(&set, SIGTERM);
    sigaddset(&set, SIGINT);
    sigaddset(&set, SIGUSR1);
    
    int sig;
    printf("[Signal] 待機中...\n");

    pthread_mutex_lock(&ctx->mtx);
    ctx->signal_thread_ready = true;
    pthread_cond_signal(&ctx->cond);
    pthread_mutex_unlock(&ctx->mtx);

    while (1) {
        sigwait(&set, &sig);
        
        // パターンA: 外部からの終了要求 (Ctrl+C など)
        if (sig == SIGINT || sig == SIGTERM) {
            printf("[Signal] 外部から %s を受信。メインに報告します。\n", 
                   sig == SIGINT ? "SIGINT" : "SIGTERM");
            pthread_mutex_lock(&ctx->mtx);
            ctx->shutdown_requested = 1;
            pthread_cond_signal(&ctx->cond);
            pthread_mutex_unlock(&ctx->mtx);
            // ※ここでは break しない（まだメインから殺されていないため）
        }
        // パターンB: メインスレッドからの致死命令 (Poison Pill)
        else if (sig == SIGUSR1) {
            printf("[Signal] メインから致死命令(SIGUSR1)を受信。自害します。\n");
            break;
        }
    }
    return NULL;
}
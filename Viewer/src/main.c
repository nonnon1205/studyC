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
#include <errno.h>
#include "udp_common.h"
#include "unified_logger.h"
#include "event_handler.h"
#include "mgmt_worker.h"
#include "mgmt_paths.h"
#include "mgmt_handlers.h"

int main(void) {
    pthread_t t_sig, t_tcp;
    pthread_t t_mgmt;
    int t_mgmt_created = 0;
    int registry_initialized = 0;
    int mgmt_ok = 1;
    AppContext ctx;
    sigset_t set;
    atomic_bool g_mgmt_running;
    atomic_init(&g_mgmt_running, true);
    ViewerMgmtCtx mgmt_ctx;
    MgmtWorkerArg mgmt_arg;

    log_init("Viewer");

    GLOG_INFO("==========================================");
    GLOG_INFO(" Viewer 起動");
    GLOG_INFO("==========================================");

    // 前準備
    sigemptyset(&set);
    sigaddset(&set, SIGTERM);
    sigaddset(&set, SIGINT);
    sigaddset(&set, SIGUSR1);
    pthread_sigmask(SIG_BLOCK, &set, NULL);

    /* パイプやソケットへの書き込みエラー（SIGPIPE）による突然死を防ぐ */
    signal(SIGPIPE, SIG_IGN);

    pthread_mutex_init(&ctx.mtx, NULL);
    pthread_cond_init(&ctx.cond, NULL);
    ctx.shutdown_requested = 0;
    ctx.signal_thread_ready = false;

    // ★追加: TCPスレッドを叩き起こす用のパイプを作成
    if (pipe(ctx.shutdown_pipe) < 0) {
        log_err("pipe: %s", safe_strerror(errno));
        log_close();
        return EXIT_FAILURE;
    }

    pthread_create(&t_sig, NULL, signal_worker, (void *)&ctx);

    pthread_mutex_lock(&ctx.mtx);
    while (!ctx.signal_thread_ready) {
        pthread_cond_wait(&ctx.cond, &ctx.mtx);
    }
    pthread_mutex_unlock(&ctx.mtx);

    pthread_create(&t_tcp, NULL, tcp_worker, (void *)&ctx);

    /* mgmt: ハンドラ登録 → ワーカースレッド起動（失敗時はプロセス終了） */
    if (handler_registry_init() != 0) {
        log_err("[Mgmt] ハンドラレジストリ初期化失敗");
        mgmt_ok = 0;
    } else {
        registry_initialized = 1;
        mgmt_ctx.start_time   = time(NULL);
        mgmt_ctx.app_ctx      = &ctx;
        mgmt_ctx.keep_running = &g_mgmt_running;
        if (viewer_mgmt_register(&mgmt_ctx) != 0) {
            log_err("[Mgmt] ハンドラ登録に失敗");
            mgmt_ok = 0;
        }
    }
    if (mgmt_ok) {
        mgmt_arg.socket_path  = MGMT_SOCKET_PATH_VIEWER;
        mgmt_arg.keep_running = &g_mgmt_running;
        if (pthread_create(&t_mgmt, NULL, mgmt_worker, &mgmt_arg) == 0) {
            t_mgmt_created = 1;
        } else {
            log_err("[Mgmt] mgmt_worker スレッド作成失敗: %s", safe_strerror(errno));
            mgmt_ok = 0;
        }
    }
    if (!mgmt_ok) {
        /* t_ipc/t_tcp/t_sig が動いているため cond で終了シーケンスへ乗せる */
        pthread_mutex_lock(&ctx.mtx);
        ctx.shutdown_requested = 1;
        pthread_cond_signal(&ctx.cond);
        pthread_mutex_unlock(&ctx.mtx);
    }

    // いずれかのルートで終了フラグが立つのを待つ
    GLOG_INFO("[Viewer] 準備完了 — イベント待機中");
    pthread_mutex_lock(&ctx.mtx);
    while (ctx.shutdown_requested == 0) {
        pthread_cond_wait(&ctx.cond, &ctx.mtx);
    }
    pthread_mutex_unlock(&ctx.mtx);

    /* mgmt_worker ループを停止（SHUTDOWN ハンドラ経由でない場合） */
    atomic_store_explicit(&g_mgmt_running, false, memory_order_release);

    log_info("[Main] --- 終了通知シーケンス開始 ---");

    // 3. Signalスレッドを叩き起こす
    pthread_kill(t_sig, SIGUSR1);

    // ★追加: 4. TCPスレッド(View)をパイプで叩き起こす
    char dummy = 'X';
    if (write(ctx.shutdown_pipe[1], &dummy, 1) < 0) {
        log_err("write to shutdown_pipe: %s", safe_strerror(errno));
    }
    
    // 各スレッドの合流を待つ
    pthread_join(t_sig, NULL);
    pthread_join(t_tcp, NULL);
    if (t_mgmt_created) {
        pthread_join(t_mgmt, NULL);
        log_info("t_mgmt (Mgmt Worker) 終了");
    }

    // 最終後片付け
    close(ctx.shutdown_pipe[0]);
    close(ctx.shutdown_pipe[1]);
    if (registry_initialized) {
        handler_registry_destroy();
    }
    log_info("[Main] 全てのスレッドを回収。リソースを解放しました。");

    log_close();
    return mgmt_ok ? EXIT_SUCCESS : EXIT_FAILURE;
}
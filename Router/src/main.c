#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <pthread.h>
#include <signal.h>
#include <semaphore.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <time.h>
#include "unified_logger.h"
#include "msg_common.h"
#include "router_worker.h"
#include "shared_ipc.h"
#include "shm_api.h"
#include "event_handler.h"
#include "mgmt_worker.h"
#include "mgmt_paths.h"
#include "mgmt_handlers.h"

#ifdef ENABLE_FAULT_INJECTION
static void print_usage(const char* prog) {
    fprintf(stderr, "Usage: %s [--fail-leak] [--fail-use-after-free] [--fail-race]\n", prog);
}

static void* race_worker(void* arg) {
    (void)arg;
    while (1) {
        g_race_flag++;
        pthread_testcancel();
    }
    return NULL;
}
#endif

static void send_shm_quit(int msqid) {
    IpcNotifyMessage notify;
    notify.mtype = MSG_TYPE_SHM_NOTIFY;
    notify.shm_status_id = MSG_TYPE_SHM_QUIT;
    if (msgsnd(msqid, &notify, sizeof(IpcNotifyMessage) - sizeof(long), 0) == -1) {
        GLOG_ERR("msgsnd (SHM_QUIT): %s", safe_strerror(errno));
    }
}

int g_fail_race = 0;
int volatile g_race_flag = 0;
atomic_bool g_keep_running = true;
sem_t g_signal_worker_ready;
int g_sem_initialized = 0;
int g_shutdown_pipe[2] = {-1, -1};

int main(int argc, char *argv[]) {
    int fail_leak = 0;
    RouterContext router_ctx;
#ifdef ENABLE_FAULT_INJECTION
    int fail_use_after_free = 0;
    int fail_race = 0;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--fail-leak") == 0) {
            fail_leak = 1;
        } else if (strcmp(argv[i], "--fail-use-after-free") == 0) {
            fail_use_after_free = 1;
        } else if (strcmp(argv[i], "--fail-race") == 0) {
            fail_race = 1;
        } else if (strcmp(argv[i], "--help") == 0) {
            print_usage(argv[0]);
            return EXIT_SUCCESS;
        } else {
            fprintf(stderr, "Unknown option: %s\n", argv[i]);
            print_usage(argv[0]);
            return EXIT_FAILURE;
        }
    }
#else
    (void)argc;
    (void)argv;
#endif

#ifdef ENABLE_FAULT_INJECTION
    if (fail_use_after_free) {
        char *bad = malloc(16);
        if (bad) {
            free(bad);
            bad[0] = 'X';
            GLOG_INFO("[Fault] use-after-free injected");
        }
    }

    char *leak_buf = NULL;
    if (fail_leak) {
        leak_buf = malloc(64);
        if (leak_buf) {
            memset(leak_buf, 0xAA, 64);
            GLOG_INFO("[Fault] memory leak injected");
        }
    }

        g_fail_race = 1;
    }
#else
    char *leak_buf = NULL;
#endif

    // ★ Helgrind対策: スレッド作成前にタイムゾーンを初期化し、
    // syslog 内部での競合（誤検知）を防止する
    tzset();    

    pthread_t t2, t3;
    int t2_created = 0, t3_created = 0;
    pthread_t t_mgmt;
    int t_mgmt_created = 0;
    int registry_initialized = 0;
    int mgmt_ok = 1;
    sigset_t set;
    int router_started = 0;
    int ret = EXIT_FAILURE; // Default to failure

    RouterMgmtCtx mgmt_ctx;
    MgmtWorkerArg mgmt_arg;

    log_init("Router");

    int mainmsqid = msgget(IPC_PRIVATE, 0666 | IPC_CREAT);
    if (mainmsqid == -1) {
        GLOG_ERR("msgget: %s", safe_strerror(errno));
        goto cleanup_log;
    }

    int msqid = msgget(MSG_KEY, 0666 | IPC_CREAT);
    if (msqid == -1) {
        GLOG_ERR("msgget: %s", safe_strerror(errno));
        goto cleanup_main_msq;
    }

    router_ctx.main_msqid = mainmsqid;
    router_ctx.ipc_msqid = msgget(SYSTEM_IPC_KEY, 0666 | IPC_CREAT);
    router_ctx.shm_handle = shm_api_init();
    if (!router_ctx.shm_handle) {
        GLOG_ERR("共有メモリの初期化に失敗しました。");
        goto cleanup_msq;
    }

    if (pipe(g_shutdown_pipe) != 0) {
        GLOG_ERR("pipe: %s", safe_strerror(errno));
        goto cleanup_shm;
    }

    if (sem_init(&g_signal_worker_ready, 0, 0) != 0) {
        GLOG_ERR("sem_init: %s", safe_strerror(errno));
        goto cleanup_pipe;
    }
    g_sem_initialized = 1;

    sigemptyset(&set);
    sigaddset(&set, SIGINT);
    sigaddset(&set, SIGTERM);
    if (pthread_sigmask(SIG_BLOCK, &set, NULL) != 0) {
        GLOG_ERR("pthread_sigmask: %s", safe_strerror(errno));
        goto cleanup_sem;
    }

    if (pthread_create(&t2, NULL, signal_worker, &mainmsqid) != 0) {
        GLOG_ERR("pthread_create signal_worker: %s", safe_strerror(errno));
        goto cleanup_sem;
    }
    t2_created = 1;

    if (sem_wait(&g_signal_worker_ready) != 0) {
        GLOG_ERR("sem_wait: %s", safe_strerror(errno));
        goto cleanup_t2;
    }

    if (pthread_create(&t3, NULL, router_worker, &router_ctx) != 0) {
        GLOG_ERR("pthread_create router_worker: %s", safe_strerror(errno));
        goto cleanup_t2;
    }
    t3_created = 1;
    router_started = 1;

    /* mgmt: ハンドラ登録 → ワーカースレッド起動（失敗時はプロセス終了） */
    if (handler_registry_init() != 0) {
        GLOG_ERR("[Mgmt] ハンドラレジストリ初期化失敗");
        mgmt_ok = 0;
    } else {
        registry_initialized = 1;
        mgmt_ctx.start_time   = time(NULL);
        mgmt_ctx.keep_running = &g_keep_running;
        mgmt_ctx.mainmsqid    = mainmsqid;
        if (router_mgmt_register(&mgmt_ctx) != 0) {
            GLOG_ERR("[Mgmt] ハンドラ登録に失敗");
            mgmt_ok = 0;
        }
    }
    if (mgmt_ok) {
        mgmt_arg.socket_path  = MGMT_SOCKET_PATH_ROUTER;
        mgmt_arg.keep_running = &g_keep_running;
        if (pthread_create(&t_mgmt, NULL, mgmt_worker, &mgmt_arg) == 0) {
            t_mgmt_created = 1;
        } else {
            GLOG_ERR("[Mgmt] mgmt_worker スレッド作成失敗: %s", safe_strerror(errno));
            mgmt_ok = 0;
        }
    }
    if (!mgmt_ok) {
        /* t2/t3 が動いているため既存の終了フローへ乗せる */
        atomic_store_explicit(&g_keep_running, false, memory_order_release);
        send_shm_quit(router_ctx.ipc_msqid);
        if (g_shutdown_pipe[1] != -1) {
            close(g_shutdown_pipe[1]);
            g_shutdown_pipe[1] = -1;
        }
        goto skip_to_cleanup;
    }

#ifdef ENABLE_FAULT_INJECTION
    pthread_t t_race;
    int t_race_created = 0;
    if (fail_race) {
        if (pthread_create(&t_race, NULL, race_worker, NULL) != 0) {
            GLOG_ERR("pthread_create race_worker: %s", safe_strerror(errno));
            goto cleanup_t3;
        }
        t_race_created = 1;
    }
#endif

    GLOG_INFO("[Main] 指揮官、msgrcvにてイベント待機を開始します。");

    InternalMsg rx_msg;

    while (g_keep_running) {
        // ここで全イベントを一本化して待機（CPU負荷 0）
        if (msgrcv(mainmsqid, &rx_msg, sizeof(InternalMsg) - sizeof(long), 0, 0) == -1) {
            if (errno == EINTR) continue;
            GLOG_ERR("msgrcv: %s", safe_strerror(errno));
            break;
        }

        switch (rx_msg.event) {
            case EV_QUIT:
                GLOG_INFO("[Main] 外部 UDP QUIT を受信しました。終了命令として扱いません。");
                break;
            case EV_UDP:
                GLOG_INFO("[Main] UDPデータ受信: %s", rx_msg.data.udp_payload);
                break;
            case EV_SIGNAL:
                GLOG_INFO("[Main] シグナル(%d)を検知しました。", rx_msg.data.sig_num);
                if (rx_msg.data.sig_num == SIGINT || rx_msg.data.sig_num == SIGTERM) {
                    atomic_store_explicit(&g_keep_running, false, memory_order_release);
                    send_shm_quit(router_ctx.ipc_msqid);
                    if (g_shutdown_pipe[1] != -1) {
                        close(g_shutdown_pipe[1]);
                        g_shutdown_pipe[1] = -1;
                    }
                }
                break;
            case EV_IPC:
                GLOG_INFO("[Main] IPCイベント受信: %s", rx_msg.data.ipc_payload);
                break;
            case EV_FATAL:
                GLOG_ERR("[Main] 内部致命エラー通知: %s", rx_msg.data.ipc_payload);
                atomic_store_explicit(&g_keep_running, false, memory_order_release);
                if (g_shutdown_pipe[1] != -1) {
                    close(g_shutdown_pipe[1]);
                    g_shutdown_pipe[1] = -1;
                }
                break;
            default:
                break;
        }
    }

    atomic_store_explicit(&g_keep_running, false, memory_order_release);
    if (g_shutdown_pipe[1] != -1) {
        close(g_shutdown_pipe[1]);
        g_shutdown_pipe[1] = -1;
    }
    if (router_started) {
        send_shm_quit(router_ctx.ipc_msqid);
    }

    if (t_mgmt_created) {
        pthread_join(t_mgmt, NULL);
        GLOG_INFO("t_mgmt (Mgmt Worker) 終了");
    }

    ret = EXIT_SUCCESS; // Mark as success before cleanup

skip_to_cleanup:
#ifdef ENABLE_FAULT_INJECTION
cleanup_t_race:
    if (fail_race && t_race_created) {
        pthread_cancel(t_race);
        pthread_join(t_race, NULL);
    }
cleanup_t3:
#endif
    if (t3_created) {
        pthread_join(t3, NULL);
        GLOG_INFO("t3 (Router Worker) 終了");
    }

cleanup_t2:
    if (t2_created) {
        int cancel_ret = pthread_cancel(t2);
        if (cancel_ret != 0 && cancel_ret != ESRCH) {
            GLOG_ERR("pthread_cancel(signal_worker): %s", safe_strerror(cancel_ret));
        }
        pthread_join(t2, NULL);
        GLOG_INFO("t2 (Signal Worker) 終了");
    }

cleanup_sem:
    if (g_sem_initialized) {
        sem_destroy(&g_signal_worker_ready);
    }

cleanup_pipe:
    if (g_shutdown_pipe[0] != -1) close(g_shutdown_pipe[0]);
    if (g_shutdown_pipe[1] != -1) close(g_shutdown_pipe[1]);

cleanup_shm:
    shm_api_close(router_ctx.shm_handle);

cleanup_msq:
    if (msgctl(msqid, IPC_RMID, NULL) == -1) GLOG_ERR("msgctl(IPC_RMID): %s", safe_strerror(errno));
cleanup_main_msq:
    if (msgctl(mainmsqid, IPC_RMID, NULL) == -1) GLOG_ERR("msgctl(IPC_RMID): %s", safe_strerror(errno));
cleanup_log:
    GLOG_INFO("[Main] 各スレッドを回収中...");
    if (!fail_leak && leak_buf != NULL) {
        free(leak_buf);
    }
    if (registry_initialized) {
        handler_registry_destroy();
    }
    GLOG_INFO("[Main] リソース解放完了。正常終了。");
    log_close();

    return ret;
}

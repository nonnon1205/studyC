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
        log_err("msgsnd (SHM_QUIT): %s", strerror(errno));
    }
}

int g_fail_race = 0;
int volatile g_race_flag = 0;
atomic_bool g_keep_running = ATOMIC_VAR_INIT(true);
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
            log_info("[Fault] use-after-free injected");
        }
    }

    char *leak_buf = NULL;
    if (fail_leak) {
        leak_buf = malloc(64);
        if (leak_buf) {
            memset(leak_buf, 0xAA, 64);
            log_info("[Fault] memory leak injected");
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

    pthread_t t1, t2, t3;
    int t2_created = 0, t3_created = 0;
    sigset_t set;
    int router_started = 0;
    int ret = EXIT_FAILURE; // Default to failure

    log_init("TestMsgRcv");

    int mainmsqid = msgget(IPC_PRIVATE, 0666 | IPC_CREAT);
    if (mainmsqid == -1) {
        log_err("msgget: %s", strerror(errno));
        goto cleanup_log;
    }

    int msqid = msgget(MSG_KEY, 0666 | IPC_CREAT);
    if (msqid == -1) {
        log_err("msgget: %s", strerror(errno));
        goto cleanup_main_msq;
    }

    router_ctx.main_msqid = mainmsqid;
    router_ctx.ipc_msqid = msgget(SYSTEM_IPC_KEY, 0666 | IPC_CREAT);
    router_ctx.shm_handle = shm_api_init();
    if (!router_ctx.shm_handle) {
        fprintf(stderr, "共有メモリの初期化に失敗しました。\n");
        goto cleanup_msq;
    }

    if (pipe(g_shutdown_pipe) != 0) {
        log_err("pipe: %s", strerror(errno));
        goto cleanup_shm;
    }

    if (sem_init(&g_signal_worker_ready, 0, 0) != 0) {
        log_err("sem_init: %s", strerror(errno));
        goto cleanup_pipe;
    }
    g_sem_initialized = 1;

    sigemptyset(&set);
    sigaddset(&set, SIGINT);
    sigaddset(&set, SIGTERM);
    if (pthread_sigmask(SIG_BLOCK, &set, NULL) != 0) {
        log_err("pthread_sigmask: %s", strerror(errno));
        goto cleanup_sem;
    }

    if (pthread_create(&t2, NULL, signal_worker, &mainmsqid) != 0) {
        log_err("pthread_create signal_worker: %s", strerror(errno));
        goto cleanup_sem;
    }
    t2_created = 1;

    if (sem_wait(&g_signal_worker_ready) != 0) {
        log_err("sem_wait: %s", strerror(errno));
        goto cleanup_t2;
    }

    if (pthread_create(&t3, NULL, router_worker, &router_ctx) != 0) {
        log_err("pthread_create router_worker: %s", strerror(errno));
        goto cleanup_t2;
    }
    t3_created = 1;
    router_started = 1;

#ifdef ENABLE_FAULT_INJECTION
    pthread_t t_race;
    int t_race_created = 0;
    if (fail_race) {
        if (pthread_create(&t_race, NULL, race_worker, NULL) != 0) {
            log_err("pthread_create race_worker: %s", strerror(errno));
            goto cleanup_t3;
        }
        t_race_created = 1;
    }
#endif

    log_info("[Main] 指揮官、msgrcvにてイベント待機を開始します。");

    InternalMsg rx_msg;

    while (g_keep_running) {
        // ここで全イベントを一本化して待機（CPU負荷 0）
        if (msgrcv(mainmsqid, &rx_msg, sizeof(InternalMsg) - sizeof(long), 0, 0) == -1) {
            if (errno == EINTR) continue;
            log_err("msgrcv: %s", strerror(errno));
            break;
        }

        switch (rx_msg.event) {
            case EV_QUIT:
                log_info("[Main] 外部 UDP QUIT を受信しました。終了命令として扱いません。");
                break;
            case EV_UDP:
                log_info("[Main] UDPデータ受信: %s", rx_msg.data.udp_payload);
                break;
            case EV_SIGNAL:
                log_info("[Main] シグナル(%d)を検知しました。", rx_msg.data.sig_num);
                if (rx_msg.data.sig_num == SIGINT || rx_msg.data.sig_num == SIGTERM) {
                    atomic_store_explicit(&g_keep_running, false, memory_order_release);
                    send_shm_quit(router_ctx.ipc_msqid);
                    if (g_shutdown_pipe[1] != -1) {
                        char dummy = 'x';
                        write(g_shutdown_pipe[1], &dummy, 1);
                    }
                }
                break;
            case EV_IPC:
                log_info("[Main] IPCイベント受信: %s", rx_msg.data.ipc_payload);
                break;
            case EV_FATAL:
                log_err("[Main] 内部致命エラー通知: %s", rx_msg.data.ipc_payload);
                atomic_store_explicit(&g_keep_running, false, memory_order_release);
                if (g_shutdown_pipe[1] != -1) {
                    char dummy = 'x';
                    //write(g_shutdown_pipe[1], &dummy, 1);
                    close(g_shutdown_pipe[1]);
                }
                break;
            default:
                break;
        }
    }

    atomic_store_explicit(&g_keep_running, false, memory_order_release);
    if (g_shutdown_pipe[1] != -1) {
        char dummy = 'x';
        //write(g_shutdown_pipe[1], &dummy, 1);
        close(g_shutdown_pipe[1]);
        
    }
    if (router_started) {
        send_shm_quit(router_ctx.ipc_msqid);
    }

    ret = EXIT_SUCCESS; // Mark as success before cleanup

#ifdef ENABLE_FAULT_INJECTION
cleanup_t_race:
    if (fail_race && t_race_created) {
        pthread_cancel(t_race);
        pthread_join(t_race, NULL);
    }
#endif

cleanup_t3:
    if (t3_created) {
        pthread_join(t3, NULL);
        log_info("t3 (Router Worker) 終了");
    }

cleanup_t2:
    if (t2_created) {
        int cancel_ret = pthread_cancel(t2);
        if (cancel_ret != 0 && cancel_ret != ESRCH) {
            log_err("pthread_cancel(signal_worker): %s", strerror(cancel_ret));
        }
        pthread_join(t2, NULL);
        log_info("t2 (Signal Worker) 終了");
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
    if (msgctl(msqid, IPC_RMID, NULL) == -1) log_err("msgctl(IPC_RMID): %s", strerror(errno));
cleanup_main_msq:
    if (msgctl(mainmsqid, IPC_RMID, NULL) == -1) log_err("msgctl(IPC_RMID): %s", strerror(errno));
cleanup_log:
    log_info("[Main] 各スレッドを回収中...");
    if (!fail_leak && leak_buf != NULL) {
        free(leak_buf);
    }
    log_info("[Main] リソース解放完了。正常終了。");
    log_close();

    return ret;
}

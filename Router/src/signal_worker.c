#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <stdatomic.h>
#include <pthread.h>
#include <signal.h>
#include <unistd.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include "unified_logger.h"
#define MODULE_NAME "Signal"
#include "debug_log.h"
#include "msg_common.h"

// --- 2. シグナルスレッド (正規化してキューへ) ---
void* signal_worker(void* arg) {
    int msqid = *(int*)arg;
    sigset_t set;
    int sig;

    pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
    pthread_setcanceltype(PTHREAD_CANCEL_DEFERRED, NULL);

    sigemptyset(&set);
    sigaddset(&set, SIGINT);
    sigaddset(&set, SIGTERM);

    if (pthread_sigmask(SIG_BLOCK, &set, NULL) != 0) {
        GLOG_ERR("[Signal] pthread_sigmask: %s", safe_strerror(errno));
        return NULL;
    }

    sem_post(&g_signal_worker_ready);

    while (atomic_load_explicit(&g_keep_running, memory_order_acquire)) {
        int ret = sigwait(&set, &sig);
        if (ret != 0) {
            GLOG_ERR("[Signal] sigwait: %s", safe_strerror(ret));
            break;
        }
        DBG("シグナル受信: sig=%d", sig);
#ifdef ENABLE_FAULT_INJECTION
        if (g_fail_race) {
            g_race_flag++;
        }
#endif
        if (send_signal_event(msqid, sig) != 0) {
            GLOG_ERR("[Signal] send_signal_event failed");
        }
        if (sig == SIGINT || sig == SIGTERM) break;
        pthread_testcancel();
    }
    return NULL;
}

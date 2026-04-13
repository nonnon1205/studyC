#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <pthread.h>
#include <signal.h>
#include <unistd.h>
#include <syslog.h>
#include "msg_common.h"

// --- 2. シグナルスレッド (正規化してキューへ) ---
void* signal_worker(void* arg) {
    int msqid = *(int*)arg;
    sigset_t set;
    int sig;
    sigemptyset(&set);
    sigaddset(&set, SIGINT);
    sigaddset(&set, SIGTERM);

    while (1) {
        int ret = sigwait(&set, &sig);
        if (ret != 0) {
            syslog(LOG_ERR, "TestMsgRcv: [Signal] sigwait: %s", strerror(ret));
            break;
        }
        send_signal_event(msqid, sig);
        if (sig == SIGINT || sig == SIGTERM) break;
    }
    return NULL;
}

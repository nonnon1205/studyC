#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include "unified_logger.h"
#define MODULE_NAME "ShmWorker"
#include "debug_log.h"
#include "msg_common.h"
#include "time.h"
#include "shm_api.h"

void* shm_worker(void* arg) {
    int msqid = *(int*)arg;

    ShmHandle shm = shm_api_init();
    if (!shm) {
        GLOG_ERR("[SHM_Worker] 共有メモリの初期化に失敗しました");
        return NULL;
    }

    GLOG_INFO("[SHM_Worker] 共有メモリの監視を開始します");

    int last_status = -1;
    char msg_buf[256];

    while (1) {
        int current_status;

        if (shm_api_read(shm, &current_status, msg_buf)) {
            if (current_status != last_status) {
                DBG("SHM状態変化: %d -> %d, msg=\"%s\"", last_status, current_status, msg_buf);
                GLOG_INFO("[SHM_Worker] 変化検知: status=%d, msg=%s", current_status, msg_buf);

                send_ipc_event(msqid, msg_buf);
                last_status = current_status;
            }
        }

        struct timespec req = {0, 500000000};
        nanosleep(&req, NULL);
    }

    shm_api_close(shm);
    return NULL;
}

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
#include "shm_api.h" // 隣のプロジェクトの公開API

void* shm_worker(void* arg) {
    int msqid = *(int*)arg;
    
    // 共有メモリAPIの初期化
    ShmHandle shm = shm_api_init();
    if (!shm) {
        log_err("[SHM_Worker] 共有メモリの初期化に失敗しました");
        return NULL;
    }

    log_info("[SHM_Worker] 共有メモリの監視を開始します");

    int last_status = -1;
    char msg_buf[256];

    // ※現状は無限ループ（メイン終了時にプロセスごと刈り取られる想定）
    while (1) {
        int current_status;
        
        // API経由で安全に読み取り（内部でFutexロックが走る）
        if (shm_api_read(shm, &current_status, msg_buf)) {
            // ステータスが前回と違う場合（更新された場合）
            if (current_status != last_status) {
                DBG("SHM状態変化: %d -> %d, msg=\"%s\"", last_status, current_status, msg_buf);
                log_info("[SHM_Worker] 変化検知: status=%d, msg=%s", current_status, msg_buf);
                
                // メインスレッドへ EV_IPC イベントとして送信
                send_ipc_event(msqid, msg_buf);

                last_status = current_status;
            }
        }
        
        // 500ミリ秒（0.5秒）ごとにポーリング
        struct timespec req = {0, 500000000};
        nanosleep(&req, NULL);
    }

    shm_api_close(shm);
    return NULL;
}
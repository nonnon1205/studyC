#define _POSIX_C_SOURCE 200809L

#ifndef _GNU_SOURCE
#define _GNU_SOURCE // EOWNERDEAD などのマクロを有効にするため
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <syslog.h>
#include "shm_common.h"
#include "shm_api.h"

// 不透明ポインタ (ShmHandle) の実体
struct ShmContext {
    int         shmid;
    SharedData* shm_ptr;
};

// --- ヘルパー：安全なロック処理（ロバスト回復付き） ---
static bool safe_lock(pthread_mutex_t *mtx) {
    int ret = pthread_mutex_lock(mtx);
    
    if (ret == 0) {
        return true; // 正常にロック取得
    }
    
#ifdef PTHREAD_MUTEX_ROBUST
    if (ret == EOWNERDEAD) {
        // 前のオーナー（プロセス）が鍵を持ったまま死んだ！
        syslog(LOG_WARNING, "[SHM] ロック保持者の死亡を検知。状態を復旧します。");
        
        // データの不整合があればここで直す（今回は上書きするだけなのでパス）
        
        // Mutexを「一貫性あり」の状態に戻し、自分が新しいオーナーになる
        pthread_mutex_consistent(mtx);
        return true;
    }
#endif

    syslog(LOG_ERR, "[SHM] Mutexロック失敗: %s", strerror(ret));
    return false;
}

// --- API: 初期化と接続 ---
ShmHandle shm_api_init(void) {
    struct ShmContext* ctx = malloc(sizeof(struct ShmContext));
    if (!ctx) return NULL;

    bool is_creator = false;

    // 技巧2: IPC_EXCL を使って「自分が最初の作成者か」を判定する
    ctx->shmid = shmget(SHM_KEY, sizeof(SharedData), IPC_CREAT | IPC_EXCL | 0666);
    if (ctx->shmid >= 0) {
        is_creator = true; // 私が作りました
        syslog(LOG_INFO, "[SHM] 共有メモリを新規作成しました。");
    } else if (errno == EEXIST) {
        // 既に誰かが作っている場合は、単に取得する
        ctx->shmid = shmget(SHM_KEY, sizeof(SharedData), 0666);
    }

    if (ctx->shmid < 0) {
        syslog(LOG_ERR, "[SHM] shmget失敗: %m");
        free(ctx);
        return NULL;
    }

    ctx->shm_ptr = (SharedData*)shmat(ctx->shmid, NULL, 0);
    if (ctx->shm_ptr == (void*)-1) {
        syslog(LOG_ERR, "[SHM] shmat失敗: %m");
        free(ctx);
        return NULL;
    }

    // 作成者のみが Mutex の初期化を行う（二重初期化による破壊を防ぐため）
    if (is_creator) {
        pthread_mutexattr_t attr;
        pthread_mutexattr_init(&attr);
        
        // プロセス間共有の設定
        pthread_mutexattr_setpshared(&attr, PTHREAD_PROCESS_SHARED);
        
#ifdef PTHREAD_MUTEX_ROBUST
        // ロバスト属性の設定（対応している場合のみ）
        pthread_mutexattr_setrobust(&attr, PTHREAD_MUTEX_ROBUST);
#endif
        
        pthread_mutex_init(&ctx->shm_ptr->mtx, &attr);
        pthread_mutexattr_destroy(&attr);
    } else {
        // ※厳密には作成者の初期化完了を少し待つべきですが、今回は簡略化します
        usleep(10000); 
    }

    return ctx;
}

// --- API: 書き込み ---
bool shm_api_write(ShmHandle handle, int status, const char* msg) {
    if (!handle || !handle->shm_ptr) return false;

    if (!safe_lock(&handle->shm_ptr->mtx)) return false;

    handle->shm_ptr->status_code = status;
    strncpy(handle->shm_ptr->message, msg, 255);
    
    pthread_mutex_unlock(&handle->shm_ptr->mtx);
    return true;
}

// --- API: 読み取り ---
bool shm_api_read(ShmHandle handle, int* out_status, char* out_msg) {
    if (!handle || !handle->shm_ptr) return false;

    if (!safe_lock(&handle->shm_ptr->mtx)) return false;

    if (out_status) *out_status = handle->shm_ptr->status_code;
    if (out_msg) strncpy(out_msg, handle->shm_ptr->message, 255);

    pthread_mutex_unlock(&handle->shm_ptr->mtx);
    return true;
}

// --- API: 切断（プロセス終了時） ---
void shm_api_close(ShmHandle handle) {
    if (handle) {
        shmdt(handle->shm_ptr);
        free(handle);
    }
}

// --- API: 完全破棄（システム全体の終了時） ---
void shm_api_destroy(ShmHandle handle) {
    if (handle) {
        // メモリ空間からの切り離し
        shmdt(handle->shm_ptr);
        // OSから共有メモリそのものを削除
        shmctl(handle->shmid, IPC_RMID, NULL);
        free(handle);
        syslog(LOG_INFO, "[SHM] 共有メモリをシステムから破棄しました。");
    }
}

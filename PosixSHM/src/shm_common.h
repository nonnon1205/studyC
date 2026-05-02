#ifndef POSIX_SHM_COMMON_H
#define POSIX_SHM_COMMON_H

#include <pthread.h>

// 共有メモリ用の名前 (POSIX IPC ではスラッシュから始まる文字列を使用)
#define SHM_NAME "/studyc_posix_shm"

// 共有メモリ上に配置するデータ構造体
typedef struct {
    pthread_mutex_t mtx;    // プロセス間共有Mutex
    int  status_code;       // 状態コード
    char message[256];      // メッセージ内容
    volatile int updated;   // 書き込み完了フラグ
} SharedData;

// 共有メモリのサイズ（構造体のサイズ）
#define SHM_SIZE sizeof(SharedData)

// 関数プロトタイプ（各プロセスで実装）
void cleanup_system(int shmid, SharedData *shm_ptr);

#endif // POSIX_SHM_COMMON_H
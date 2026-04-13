#ifndef COMMON_H
#define COMMON_H

#include <pthread.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <syslog.h>

// 共有メモリ用のキー
#define SHM_KEY 0x67890
// 共有メモリのサイズ（構造体のサイズ）
#define SHM_SIZE sizeof(SharedData)

// 共有メモリ上に配置するデータ構造体
typedef struct {
    pthread_mutex_t mtx;    // プロセス間共有Mutex
    int  status_code;       // 状態コード
    char message[256];      // メッセージ内容
    volatile int updated;   // 書き込み完了フラグ
} SharedData;

// 関数プロトタイプ（各プロセスで実装）
void cleanup_system(int shmid, SharedData *shm_ptr);

#endif
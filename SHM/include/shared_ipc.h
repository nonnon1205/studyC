#ifndef SHARED_IPC_H
#define SHARED_IPC_H

#include <sys/ipc.h>
#include <sys/msg.h>

// システム全体で共有するメッセージキューの鍵
#define SYSTEM_IPC_KEY 0x54321

// メッセージの種類
#define MSG_TYPE_SHM_NOTIFY 1 // SHMが更新されたという通知
#define MSG_TYPE_SHM_QUIT 2 // SHM関連スレッドへの終了通知

// 【Control Plane】メッセージキューに投げる超軽量な通知データ
typedef struct {
    long mtype;          // System V MQの必須フィールド
    int shm_status_id;   // 「共有メモリのこのステータス番号を読んでね」という指示
} IpcNotifyMessage;

#endif
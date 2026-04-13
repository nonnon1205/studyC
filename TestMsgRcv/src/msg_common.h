#ifndef COMMON_H
#define COMMON_H
#define _POSIX_C_SOURCE 200809L
#include <sys/ipc.h>
#include <sys/msg.h>

// 1. メッセージ型の定義
typedef enum {
    EV_QUIT,    // 終了要求
    EV_UDP,     // UDPパケット受信
    EV_IPC,     // 外部プロセスからの通知
    EV_SIGNAL   // OSシグナル検知
} EventType;

// 2. 共用体によるデータ実体
typedef union {
    char udp_payload[256];
    char ipc_payload[256];
    int sig_num;
} EventData;

// 3. メッセージキュー用の構造体（封筒）
typedef struct {
    long mtype;        // 優先度やフィルタ用（今回は1固定でOK）
    EventType event;   // ここで型を判断
    EventData data;    // 実体
} InternalMsg;

void send_to_main(int ,EventType, const char*,int);
void* udp_worker(void* arg);
void* signal_worker(void* arg);

#define MSG_KEY 0x54321  // 内部通信用メッセージキー
#define UDP_PORT 8888
#define MSG_SIZE sizeof(InternalMsg) - sizeof(long)
#define MSG_QUEUE_SIZE 10
#define MSG_TYPE 1

#endif
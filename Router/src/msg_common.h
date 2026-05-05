#ifndef COMMON_H
#define COMMON_H
#define _POSIX_C_SOURCE 200809L
#include <stdatomic.h>
#include <semaphore.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <signal.h>
#include "shared_ipc.h"
#include "shm_api.h"
#include "network_config.h"

extern atomic_bool g_keep_running;
extern sem_t g_signal_worker_ready;
extern int g_shutdown_pipe[2];

// 1. メッセージ型の定義
typedef enum
{
	EV_QUIT,  // 終了要求
	EV_UDP,	  // UDPパケット受信
	EV_IPC,	  // 外部プロセスからの通知
	EV_FATAL, // 内部致命エラー
	EV_SIGNAL // OSシグナル検知
} EventType;

// 2. 共用体によるデータ実体
typedef union
{
	char udp_payload[256];
	char ipc_payload[256];
	int sig_num;
} EventData;

// 3. メッセージキュー用の構造体（封筒）
typedef struct
{
	long mtype;		 // 優先度やフィルタ用（今回は1固定でOK）
	EventType event; // ここで型を判断
	EventData data;	 // 実体
} InternalMsg;

#ifdef ENABLE_FAULT_INJECTION
extern int g_fail_race;
extern volatile int g_race_flag;
#endif

InternalMsg build_internal_msg(EventType type, const char *text, int sig);
int send_to_main(int msqid, const InternalMsg *msg);
int send_quit_event(int msqid);
int send_udp_event(int msqid, const char *payload);
int send_signal_event(int msqid, int sig);
int send_ipc_event(int msqid, const char *payload);
int send_fatal_event(int msqid, const char *payload);
void *udp_worker(void *arg);
void *signal_worker(void *arg);
void *ipc_worker(void *arg);
#define MSG_KEY 0x54321 // 内部通信用メッセージキー
#define UDP_QUIT_CMD "QUIT"
#define MSG_SIZE sizeof(InternalMsg) - sizeof(long)
#define MSG_QUEUE_SIZE 10
#define MSG_TYPE 1

#endif
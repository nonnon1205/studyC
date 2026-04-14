// common.h
#ifndef COMMON_H
#define COMMON_H
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <signal.h>
#include <unistd.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdbool.h>
#define UDP_PORT 8888
#define MSG_KEY 1234L

struct msg_buffer {
    long msg_type;
    char msg_text[100];
};

typedef struct {
    pthread_mutex_t mtx;
    pthread_cond_t  cond;
    volatile int shutdown_requested;
    int msqid;
    int udp_fd;
    bool signal_thread_ready; // 追加: Signalスレッドの準備完了フラグ   
} AppContext;

typedef struct {
    AppContext *ctx;    // 全員共有のコンテキスト
    sigset_t *sig_set;  // このスレッド専用の設定
} SignalWorkerArgs;


// プロトタイプ宣言（各スレッドの「顔」を並べる）
void* udp_worker(void* arg);
void* ipc_worker(void* arg);
void* signal_worker(void* arg);

#endif
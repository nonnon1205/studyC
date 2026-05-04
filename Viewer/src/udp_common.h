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
#include "network_config.h"

#define UDP_PORT get_network_udp_send_port()

typedef struct {
    pthread_mutex_t mtx;
    pthread_cond_t  cond;
    volatile int shutdown_requested;
    int udp_fd;
    bool signal_thread_ready; // 追加: Signalスレッドの準備完了フラグ   
    int shutdown_pipe[2]; 
} AppContext;

typedef struct {
    AppContext *ctx;    // 全員共有のコンテキスト
    sigset_t *sig_set;  // このスレッド専用の設定
} SignalWorkerArgs;

void* udp_worker(void* arg);
void* signal_worker(void* arg);
void* tcp_worker(void* arg);

#endif
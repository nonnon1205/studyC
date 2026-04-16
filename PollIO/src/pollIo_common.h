// pollIo_common.h
#ifndef POLLIO_COMMON_H
#define POLLIO_COMMON_H
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <poll.h>
#include <stdbool.h>

#define UDP_PORT 8080
#define UDP_SEND_PORT 8888

// --- 1. 初期化系（セットアップ） ---
// 戻り値として、監視対象となるファイルディスクリプタ(FD)やIDを返します
// 初期化・終了系
int setup_udp_socket(uint16_t port);
void close_udp_socket(int fd);

// ハンドラ系
bool handle_stdin_read(int udp_fd);
void handle_udp_read(int udp_fd);

// コアエンジン
void run_event_loop(int udp_fd);

#endif
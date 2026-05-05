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
#include "shm_api.h"
#include "mgmt_socket.h"
#include "network_config.h"

#define UDP_PORT get_network_udp_port()
#define UDP_SEND_PORT get_network_udp_send_port()

// --- 1. 初期化系（セットアップ） ---
int setup_udp_socket(uint16_t port);
void close_udp_socket(int fd);

// ハンドラ系
bool handle_stdin_read(int udp_fd);
void handle_udp_read(int udp_fd, int ipc_msqid, ShmHandle shm_handle);

// コアエンジン
void run_event_loop(int udp_fd, int ipc_msqid, ShmHandle shm_handle,
					MgmtSocketHandle mgmt_sock);

#endif
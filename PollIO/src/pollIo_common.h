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

// --- 1. 初期化系（セットアップ） ---
// 戻り値として、監視対象となるファイルディスクリプタ(FD)やIDを返します
int setup_udp_socket(const char* ip, uint16_t port);
int setup_ipc_queue(key_t key);

// --- 2. イベントハンドラ系（コールバック処理） ---
// poll() が反応した時に呼び出される純粋な処理関数群です

// 標準入力（キーボード）にデータが来たときの処理
// （例: "quit"と打ち込まれたら終了フラグを立てる、"shm ..."なら共有メモリへ書く）
bool handle_stdin_read(int ipc_msqid, int udp_fd); 

// UDPパケットが届いたときの処理（★ここがプロキシの心臓部！）
// （例: パケットをrecvfromで読み取り、InternalMsgに詰め替えてmsgsndする）
void handle_udp_read(int udp_fd, int ipc_msqid);

// --- 3. メインループ（Reactorコア） ---
// poll() を使って STDIN と udp_fd を同時に監視し、イベントを各ハンドラへ振り分ける
void run_event_loop(int udp_fd, int ipc_msqid);


#endif
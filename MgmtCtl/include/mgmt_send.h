/**
 * @file mgmt_send.h
 * @brief Client-side send/receive for mgmt commands over UNIX datagram socket
 */
#ifndef MGMT_SEND_H
#define MGMT_SEND_H

#include "mgmt_protocol.h"

/**
 * @brief コマンドをモジュールのソケットへ送信し応答を受け取る
 * @param socket_path  送信先サーバーソケットパス (mgmt_paths.h の定数を使用)
 * @param req          送信するコマンドリクエスト
 * @param resp         受信した応答の格納先
 * @param timeout_ms   recvfrom タイムアウト（ミリ秒）
 * @return 0 on success, -1 on error (timeout / server unreachable)
 */
int mgmt_send_command(const char *socket_path, const MgmtCommandRequest *req,
					  MgmtCommandResponse *resp, int timeout_ms);

#endif /* MGMT_SEND_H */

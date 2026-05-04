/**
 * @file network_config.h
 * @brief studyC プロジェクト全体のネットワーク共通設定
 * 
 * 稼働環境やテスト環境（make test）でのポート競合を防ぐため、
 * 環境変数が設定されている場合はそれを優先し、未設定時はデフォルト値を使用する。
 */
#ifndef NETWORK_CONFIG_H
#define NETWORK_CONFIG_H

#include <stdlib.h>

/* Viewer スレッド間通信用 MQ キー */
#define VIEWER_MSG_KEY 1234L

/* デフォルトのポート番号（マジックナンバーの集約） */
#define DEFAULT_NETWORK_UDP_PORT 9999
#define DEFAULT_NETWORK_TCP_PORT 7777
#define DEFAULT_NETWORK_UDP_SEND_PORT 8888

/** @brief Collector が受信する UDP ポート */
static inline int get_network_udp_port(void) {
    const char *env = getenv("STUDYC_UDP_PORT");
    return env ? atoi(env) : DEFAULT_NETWORK_UDP_PORT;
}

/** @brief Router が送信し、Viewer が受信する TCP ポート */
static inline int get_network_tcp_port(void) {
    const char *env = getenv("STUDYC_TCP_PORT");
    return env ? atoi(env) : DEFAULT_NETWORK_TCP_PORT;
}

/** @brief Viewer(旧設計) が受信する UDP ポート */
static inline int get_network_udp_send_port(void) {
    const char *env = getenv("STUDYC_UDP_SEND_PORT");
    return env ? atoi(env) : DEFAULT_NETWORK_UDP_SEND_PORT;
}

#endif /* NETWORK_CONFIG_H */
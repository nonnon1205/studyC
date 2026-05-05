/**
 * @file mgmt_handlers.h
 * @brief Collector モジュール向け mgmt コマンドハンドラ定義
 */
#ifndef COLLECTOR_MGMT_HANDLERS_H
#define COLLECTOR_MGMT_HANDLERS_H

#include <time.h>
#include <signal.h>

/** @brief ハンドラが参照する Collector 固有の実行時状態 */
typedef struct
{
	time_t start_time; /**< 起動時刻（uptime 算出用） */
	volatile sig_atomic_t *keep_running; /**< SHUTDOWN ハンドラが 0 にセット */
} CollectorMgmtCtx;

/**
 * @brief Collector のハンドラをグローバルレジストリへ登録する
 * @param ctx  ハンドラへ渡すコンテキスト
 * @return 0 on success, -1 on failure
 */
int collector_mgmt_register(CollectorMgmtCtx *ctx);

#endif /* COLLECTOR_MGMT_HANDLERS_H */

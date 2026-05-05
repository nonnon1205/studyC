/**
 * @file mgmt_handlers.h
 * @brief Viewer モジュール向け mgmt コマンドハンドラ定義
 */
#ifndef VIEWER_MGMT_HANDLERS_H
#define VIEWER_MGMT_HANDLERS_H

#include <time.h>
#include <stdatomic.h>
#include "udp_common.h"

/** @brief ハンドラが参照する Viewer 固有の実行時状態 */
typedef struct
{
	time_t start_time;	 /**< 起動時刻（uptime 算出用） */
	AppContext *app_ctx; /**< SHUTDOWN 時に cond で主ループを起こす */
	atomic_bool *keep_running; /**< SHUTDOWN 時に mgmt_worker ループを止める */
} ViewerMgmtCtx;

/**
 * @brief Viewer のハンドラをグローバルレジストリへ登録する
 * @param ctx  ハンドラへ渡すコンテキスト
 * @return 0 on success, -1 on failure
 */
int viewer_mgmt_register(ViewerMgmtCtx *ctx);

#endif /* VIEWER_MGMT_HANDLERS_H */

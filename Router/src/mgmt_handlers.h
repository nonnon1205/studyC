/**
 * @file mgmt_handlers.h
 * @brief Router モジュール向け mgmt コマンドハンドラ定義
 */
#ifndef ROUTER_MGMT_HANDLERS_H
#define ROUTER_MGMT_HANDLERS_H

#include <time.h>
#include <stdatomic.h>

/** @brief ハンドラが参照する Router 固有の実行時状態 */
typedef struct {
    time_t       start_time;   /**< 起動時刻（uptime 算出用） */
    atomic_bool* keep_running; /**< 将来の直接停止操作用に保持 */
    int          mainmsqid;    /**< SHUTDOWN 時に EV_SIGNAL を送るキュー ID */
} RouterMgmtCtx;

/**
 * @brief Router のハンドラをグローバルレジストリへ登録する
 * @param ctx  ハンドラへ渡すコンテキスト
 * @return 0 on success, -1 on failure
 */
int router_mgmt_register(RouterMgmtCtx* ctx);

#endif /* ROUTER_MGMT_HANDLERS_H */

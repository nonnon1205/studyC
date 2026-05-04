/**
 * @file mgmt_handlers.c
 * @brief Viewer モジュール向け mgmt コマンドハンドラ実装
 */
#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <stdatomic.h>
#include "mgmt_handlers.h"
#include "event_handler.h"
#include "mgmt_protocol.h"
#include "unified_logger.h"

static int handler_ping(const MgmtCommandRequest* req,
                        MgmtCommandResponse* resp, void* ctx)
{
    (void)ctx;
    const char* msg = "pong";
    mgmt_response_init(resp, req->request_id, MGMT_RESULT_OK,
                       "viewer", msg, strlen(msg) + 1);
    return 0;
}

static int handler_get_status(const MgmtCommandRequest* req,
                              MgmtCommandResponse* resp, void* ctx)
{
    ViewerMgmtCtx* c = (ViewerMgmtCtx*)ctx;
    long uptime = (long)(time(NULL) - c->start_time);
    char buf[256];
    snprintf(buf, sizeof(buf),
             "{\"status\":\"running\",\"uptime_s\":%ld}", uptime);
    mgmt_response_init(resp, req->request_id, MGMT_RESULT_OK,
                       "viewer", buf, strlen(buf) + 1);
    return 0;
}

static int handler_shutdown(const MgmtCommandRequest* req,
                            MgmtCommandResponse* resp, void* ctx)
{
    ViewerMgmtCtx* c = (ViewerMgmtCtx*)ctx;
    const char* msg = "shutdown initiated";
    mgmt_response_init(resp, req->request_id, MGMT_RESULT_OK,
                       "viewer", msg, strlen(msg) + 1);
    GLOG_INFO("[Mgmt] SHUTDOWN コマンドを受信");
    /* mgmt_worker ループを止める（レスポンス送信後に反映される） */
    atomic_store_explicit(c->keep_running, false, memory_order_release);
    /* 主ループの cond_wait を起こして終了シーケンスへ乗せる */
    pthread_mutex_lock(&c->app_ctx->mtx);
    c->app_ctx->shutdown_requested = 1;
    pthread_cond_signal(&c->app_ctx->cond);
    pthread_mutex_unlock(&c->app_ctx->mtx);
    return 0;
}

int viewer_mgmt_register(ViewerMgmtCtx* ctx)
{
    if (handler_register("viewer", MGMT_CMD_PING,
                         handler_ping, ctx) < 0) return -1;
    if (handler_register("viewer", MGMT_CMD_GET_STATUS,
                         handler_get_status, ctx) < 0) return -1;
    if (handler_register("viewer", MGMT_CMD_SHUTDOWN,
                         handler_shutdown, ctx) < 0) return -1;
    return 0;
}

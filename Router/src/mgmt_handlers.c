/**
 * @file mgmt_handlers.c
 * @brief Router モジュール向け mgmt コマンドハンドラ実装
 */
#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <signal.h>
#include "mgmt_handlers.h"
#include "event_handler.h"
#include "mgmt_protocol.h"
#include "msg_common.h"
#include "unified_logger.h"

static int handler_ping(const MgmtCommandRequest* req,
                        MgmtCommandResponse* resp, void* ctx)
{
    (void)ctx;
    const char* msg = "pong";
    mgmt_response_init(resp, req->request_id, MGMT_RESULT_OK,
                       "router", msg, strlen(msg) + 1);
    return 0;
}

static int handler_get_status(const MgmtCommandRequest* req,
                              MgmtCommandResponse* resp, void* ctx)
{
    RouterMgmtCtx* c = (RouterMgmtCtx*)ctx;
    long uptime = (long)(time(NULL) - c->start_time);
    char buf[256];
    snprintf(buf, sizeof(buf),
             "{\"status\":\"running\",\"uptime_s\":%ld}", uptime);
    mgmt_response_init(resp, req->request_id, MGMT_RESULT_OK,
                       "router", buf, strlen(buf) + 1);
    return 0;
}

static int handler_shutdown(const MgmtCommandRequest* req,
                            MgmtCommandResponse* resp, void* ctx)
{
    RouterMgmtCtx* c = (RouterMgmtCtx*)ctx;
    const char* msg = "shutdown initiated";
    mgmt_response_init(resp, req->request_id, MGMT_RESULT_OK,
                       "router", msg, strlen(msg) + 1);
    /* 主ループの msgrcv を SIGTERM イベントで起こして正常終了フローへ乗せる */
    GLOG_INFO("[Mgmt] SHUTDOWN コマンドを受信");
    send_signal_event(c->mainmsqid, SIGTERM);
    return 0;
}

int router_mgmt_register(RouterMgmtCtx* ctx)
{
    if (handler_register("router", MGMT_CMD_PING,
                         handler_ping, ctx) < 0) return -1;
    if (handler_register("router", MGMT_CMD_GET_STATUS,
                         handler_get_status, ctx) < 0) return -1;
    if (handler_register("router", MGMT_CMD_SHUTDOWN,
                         handler_shutdown, ctx) < 0) return -1;
    return 0;
}

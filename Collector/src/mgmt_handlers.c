/**
 * @file mgmt_handlers.c
 * @brief Collector モジュール向け mgmt コマンドハンドラ実装
 */
#include <stdio.h>
#include <string.h>
#include <time.h>
#include "mgmt_handlers.h"
#include "event_handler.h"
#include "mgmt_protocol.h"
#include "unified_logger.h"

static int handler_ping(const MgmtCommandRequest *req,
						MgmtCommandResponse *resp, void *ctx)
{
	(void)ctx;
	const char *msg = "pong";
	mgmt_response_init(resp, req->request_id, MGMT_RESULT_OK, "collector", msg,
					   strlen(msg) + 1);
	return 0;
}

static int handler_get_status(const MgmtCommandRequest *req,
							  MgmtCommandResponse *resp, void *ctx)
{
	CollectorMgmtCtx *c = (CollectorMgmtCtx *)ctx;
	long uptime = (long)(time(NULL) - c->start_time);
	char buf[256];
	snprintf(buf, sizeof(buf), "{\"status\":\"running\",\"uptime_s\":%ld}",
			 uptime);
	mgmt_response_init(resp, req->request_id, MGMT_RESULT_OK, "collector", buf,
					   strlen(buf) + 1);
	return 0;
}

static int handler_shutdown(const MgmtCommandRequest *req,
							MgmtCommandResponse *resp, void *ctx)
{
	CollectorMgmtCtx *c = (CollectorMgmtCtx *)ctx;
	const char *msg = "shutdown initiated";
	mgmt_response_init(resp, req->request_id, MGMT_RESULT_OK, "collector", msg,
					   strlen(msg) + 1);
	/* レスポンス送信後にループが終了するよう、ここでフラグを下ろす */
	*c->keep_running = 0;
	GLOG_INFO("[Mgmt] SHUTDOWN コマンドを受信");
	return 0;
}

int collector_mgmt_register(CollectorMgmtCtx *ctx)
{
	if (handler_register("collector", MGMT_CMD_PING, handler_ping, ctx) < 0)
		return -1;
	if (handler_register("collector", MGMT_CMD_GET_STATUS, handler_get_status,
						 ctx) < 0)
		return -1;
	if (handler_register("collector", MGMT_CMD_SHUTDOWN, handler_shutdown,
						 ctx) < 0)
		return -1;
	return 0;
}

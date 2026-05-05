#include <string.h>
#include <time.h>
#include "mgmt_protocol.h"

#define USEC_PER_SEC 1000000ULL

/**
 * Initialize a management command request
 */
void mgmt_request_init(MgmtCommandRequest *req, uint8_t cmd_type,
					   const char *module, const void *payload, size_t len)
{
	if (!req)
		return;

	memset(req, 0, sizeof(MgmtCommandRequest));

	req->cmd_type = cmd_type;
	req->flags = 0;
	clock_gettime(CLOCK_REALTIME, &req->timestamp);

	if (module)
	{
		strncpy(req->target_module, module, MGMT_MODULE_NAME_SIZE - 1);
	}

	if (payload && len > 0)
	{
		size_t copy_len =
			(len > MGMT_PAYLOAD_REQUEST_SIZE) ? MGMT_PAYLOAD_REQUEST_SIZE : len;
		memcpy(req->payload, payload, copy_len);
		req->payload_len = (uint16_t)copy_len;
	}
	else
	{
		req->payload_len = 0;
	}
}

/**
 * Initialize a management command response
 */
void mgmt_response_init(MgmtCommandResponse *resp, uint32_t req_id,
						uint8_t result, const char *module, const void *payload,
						size_t len)
{
	if (!resp)
		return;

	memset(resp, 0, sizeof(MgmtCommandResponse));

	resp->request_id = req_id;
	resp->result_code = result;
	clock_gettime(CLOCK_REALTIME, &resp->response_timestamp);

	if (module)
	{
		strncpy(resp->source_module, module, MGMT_MODULE_NAME_SIZE - 1);
	}

	if (payload && len > 0)
	{
		size_t copy_len = (len > MGMT_PAYLOAD_RESPONSE_SIZE)
							  ? MGMT_PAYLOAD_RESPONSE_SIZE
							  : len;
		memcpy(resp->payload, payload, copy_len);
	}
}

/**
 * Calculate latency between request and response
 */
uint64_t mgmt_response_latency_us(const MgmtCommandResponse *resp)
{
	if (!resp)
		return 0;

	uint64_t req_us = (uint64_t)resp->request_timestamp.tv_sec * USEC_PER_SEC +
					  resp->request_timestamp.tv_nsec / 1000;
	uint64_t resp_us =
		(uint64_t)resp->response_timestamp.tv_sec * USEC_PER_SEC +
		resp->response_timestamp.tv_nsec / 1000;

	return (resp_us > req_us) ? (resp_us - req_us) : 0;
}

/**
 * Validate request structure
 */
int mgmt_request_validate(const MgmtCommandRequest *req)
{
	if (!req)
		return 0;

	/* Validate command type */
	if (req->cmd_type >= MGMT_CMD_MAX)
	{
		return 0;
	}

	/* Validate target module name is set */
	if (req->target_module[0] == '\0')
	{
		return 0;
	}

	/* Validate payload length */
	if (req->payload_len > MGMT_PAYLOAD_REQUEST_SIZE)
	{
		return 0;
	}

	return 1;
}

/**
 * Convert result code to human-readable string
 */
const char *mgmt_result_str(uint8_t code)
{
	switch (code)
	{
	case MGMT_RESULT_OK:
		return "OK";
	case MGMT_RESULT_INVALID_CMD:
		return "INVALID_CMD";
	case MGMT_RESULT_MODULE_NOT_FOUND:
		return "MODULE_NOT_FOUND";
	case MGMT_RESULT_HANDLER_FAILED:
		return "HANDLER_FAILED";
	case MGMT_RESULT_TIMEOUT:
		return "TIMEOUT";
	case MGMT_RESULT_BUFFER_OVERFLOW:
		return "BUFFER_OVERFLOW";
	case MGMT_RESULT_UNAUTHORIZED:
		return "UNAUTHORIZED";
	case MGMT_RESULT_INTERNAL_ERROR:
		return "INTERNAL_ERROR";
	default:
		return "UNKNOWN";
	}
}

/**
 * Convert command type to human-readable string
 */
const char *mgmt_command_str(uint8_t cmd)
{
	switch (cmd)
	{
	case MGMT_CMD_PING:
		return "PING";
	case MGMT_CMD_SET_LOG_LEVEL:
		return "SET_LOG_LEVEL";
	case MGMT_CMD_GET_STATUS:
		return "GET_STATUS";
	case MGMT_CMD_GET_METRICS:
		return "GET_METRICS";
	case MGMT_CMD_SET_BUFFER_SIZE:
		return "SET_BUFFER_SIZE";
	case MGMT_CMD_ENABLE_PROFILING:
		return "ENABLE_PROFILING";
	case MGMT_CMD_RESET_METRICS:
		return "RESET_METRICS";
	case MGMT_CMD_SHUTDOWN:
		return "SHUTDOWN";
	case MGMT_CMD_GET_CONFIG:
		return "GET_CONFIG";
	case MGMT_CMD_ENABLE_TRACING:
		return "ENABLE_TRACING";
	default:
		return "UNKNOWN";
	}
}

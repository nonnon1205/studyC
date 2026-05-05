#ifndef MGMT_PROTOCOL_H
#define MGMT_PROTOCOL_H

#include <stdint.h>
#include <stddef.h>
#include <time.h>

/* ============================================================================
 * Management Protocol Definition
 *
 * Unified protocol for inter-module communication and command dispatch
 * Uses UNIX domain datagram sockets for low-latency, multi-client support
 * ============================================================================
 */

/* Management Command Types */
typedef enum
{
	MGMT_CMD_PING = 0,			   /* Liveness check */
	MGMT_CMD_SET_LOG_LEVEL = 1,	   /* Change module log level */
	MGMT_CMD_GET_STATUS = 2,	   /* Request current module status */
	MGMT_CMD_GET_METRICS = 3,	   /* Request performance metrics snapshot */
	MGMT_CMD_SET_BUFFER_SIZE = 4,  /* Resize buffer/queue capacity */
	MGMT_CMD_ENABLE_PROFILING = 5, /* Enable performance profiling */
	MGMT_CMD_RESET_METRICS = 6,	   /* Clear metric counters */
	MGMT_CMD_SHUTDOWN = 7,		   /* Graceful shutdown request */
	MGMT_CMD_GET_CONFIG = 8,	   /* Dump current configuration */
	MGMT_CMD_ENABLE_TRACING = 9,   /* Enable detailed event tracing */
	MGMT_CMD_MAX = 16
} MgmtCommandType;

/* Log Levels */
typedef enum
{
	LOG_LEVEL_TRACE = 0,
	LOG_LEVEL_DEBUG = 1,
	LOG_LEVEL_INFO = 2,
	LOG_LEVEL_WARN = 3,
	LOG_LEVEL_ERROR = 4,
	LOG_LEVEL_FATAL = 5
} LogLevel;

/* Result Codes */
typedef enum
{
	MGMT_RESULT_OK = 0,				  /* Success */
	MGMT_RESULT_INVALID_CMD = 1,	  /* Unknown command */
	MGMT_RESULT_MODULE_NOT_FOUND = 2, /* Target module not found */
	MGMT_RESULT_HANDLER_FAILED = 3,	  /* Handler returned error */
	MGMT_RESULT_TIMEOUT = 4,		  /* Response timeout */
	MGMT_RESULT_BUFFER_OVERFLOW = 5,  /* Response payload too large */
	MGMT_RESULT_UNAUTHORIZED = 6,	  /* Operation not permitted */
	MGMT_RESULT_INTERNAL_ERROR = 7	  /* Internal module error */
} MgmtResultCode;

/* Maximum payload sizes */
#define MGMT_PAYLOAD_REQUEST_SIZE 512
#define MGMT_PAYLOAD_RESPONSE_SIZE 2048
#define MGMT_MODULE_NAME_SIZE 64
#define MGMT_SOCKET_PATH "/tmp/sutdyc_mgmt.sock"

/* ============================================================================
 * Request Structure (Client → Management Socket)
 *
 * Binary format for efficient wire transfer
 * ============================================================================
 */
typedef struct
{
	/* Header (8 bytes) */
	uint32_t request_id;  /* Unique request ID for correlation */
	uint8_t cmd_type;	  /* MgmtCommandType enum value */
	uint8_t flags;		  /* Flags: 0x01=async, 0x02=no-response */
	uint16_t payload_len; /* Actual payload length (0 if none) */

	/* Timing (8 bytes) */
	struct timespec timestamp; /* Request creation time */

	/* Target Identification (70 bytes) */
	char target_module[MGMT_MODULE_NAME_SIZE]; /* e.g., "router", "pollio" */

	/* Command Payload (512 bytes) */
	uint8_t payload[MGMT_PAYLOAD_REQUEST_SIZE];
} MgmtCommandRequest;

/* ============================================================================
 * Response Structure (Management Socket → Client)
 *
 * Contains result code and JSON-formatted response data
 * ============================================================================
 */
typedef struct
{
	/* Header (16 bytes) */
	uint32_t request_id; /* Echoes the request ID */
	uint8_t result_code; /* MgmtResultCode enum value */
	uint8_t reserved[3]; /* Padding for alignment */

	/* Timing (16 bytes) */
	struct timespec request_timestamp;	/* Original request time */
	struct timespec response_timestamp; /* When response was generated */

	/* Source Module Info (70 bytes) */
	char source_module[MGMT_MODULE_NAME_SIZE];

	/* Response Payload (2048 bytes, typically JSON) */
	uint8_t payload[MGMT_PAYLOAD_RESPONSE_SIZE];
} MgmtCommandResponse;

/* Helper macros for payload access */
#define MGMT_REQ_PAYLOAD_AS_STRING(req) ((const char *)(req)->payload)
#define MGMT_REQ_PAYLOAD_AS_INT(req) (*(const int *)(req)->payload)
#define MGMT_RESP_PAYLOAD_AS_STRING(resp) ((const char *)(resp)->payload)

/* ============================================================================
 * Protocol Version and Constants
 * ============================================================================
 */
#define MGMT_PROTOCOL_VERSION 1
#define MGMT_REQUEST_SIZE sizeof(MgmtCommandRequest)
#define MGMT_RESPONSE_SIZE sizeof(MgmtCommandResponse)

/* Socket configuration constants */
#define MGMT_SOCKET_BACKLOG 16
#define MGMT_SOCKET_TIMEOUT_MS 5000 /* 5 second timeout for responses */
#define MGMT_SOCKET_BUFFER_SIZE (MGMT_REQUEST_SIZE * 32)

/* ============================================================================
 * Utility Functions for Protocol
 * ============================================================================
 */

/**
 * Initialize a management command request
 * @param req       Pointer to request structure
 * @param cmd_type  Command type (MgmtCommandType)
 * @param module    Target module name
 * @param payload   Optional payload data
 * @param len       Payload length (0 if no payload)
 */
void mgmt_request_init(MgmtCommandRequest *req, uint8_t cmd_type,
					   const char *module, const void *payload, size_t len);

/**
 * Initialize a management command response
 * @param resp      Pointer to response structure
 * @param req_id    Request ID to echo
 * @param result    Result code
 * @param module    Source module name
 * @param payload   Response payload (typically JSON)
 * @param len       Payload length
 */
void mgmt_response_init(MgmtCommandResponse *resp, uint32_t req_id,
						uint8_t result, const char *module, const void *payload,
						size_t len);

/**
 * Calculate latency between request and response
 * @param resp      Response structure
 * @return          Latency in microseconds
 */
uint64_t mgmt_response_latency_us(const MgmtCommandResponse *resp);

/**
 * Validate request structure
 * @param req       Request to validate
 * @return          True if valid, false otherwise
 */
int mgmt_request_validate(const MgmtCommandRequest *req);

/**
 * Convert result code to human-readable string
 * @param code      Result code
 * @return          Static string describing the result
 */
const char *mgmt_result_str(uint8_t code);

/**
 * Convert command type to human-readable string
 * @param cmd       Command type
 * @return          Static string describing the command
 */
const char *mgmt_command_str(uint8_t cmd);

#endif /* MGMT_PROTOCOL_H */

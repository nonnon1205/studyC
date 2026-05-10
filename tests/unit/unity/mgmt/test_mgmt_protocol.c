/**
 * @file test_mgmt_protocol.c
 * @brief mgmt_protocol.c の単体テスト
 */
#include "unity.h"
#include "mgmt_protocol.h"
#include <string.h>

void setUp(void) {}
void tearDown(void) {}

/* ============================================================
 * mgmt_request_validate
 * ============================================================ */

void test_validate_null_returns_false(void)
{
    TEST_ASSERT_EQUAL(0, mgmt_request_validate(NULL));
}

void test_validate_valid_request(void)
{
    MgmtCommandRequest req;
    mgmt_request_init(&req, MGMT_CMD_PING, "collector", NULL, 0);
    TEST_ASSERT_EQUAL(1, mgmt_request_validate(&req));
}

void test_validate_invalid_cmd_type(void)
{
    MgmtCommandRequest req;
    mgmt_request_init(&req, MGMT_CMD_PING, "collector", NULL, 0);
    req.cmd_type = MGMT_CMD_MAX;
    TEST_ASSERT_EQUAL(0, mgmt_request_validate(&req));
}

void test_validate_empty_module_returns_false(void)
{
    MgmtCommandRequest req;
    mgmt_request_init(&req, MGMT_CMD_PING, NULL, NULL, 0);
    TEST_ASSERT_EQUAL(0, mgmt_request_validate(&req));
}

void test_validate_payload_len_at_max(void)
{
    MgmtCommandRequest req;
    mgmt_request_init(&req, MGMT_CMD_PING, "collector", NULL, 0);
    req.payload_len = MGMT_PAYLOAD_REQUEST_SIZE;
    TEST_ASSERT_EQUAL(1, mgmt_request_validate(&req));
}

void test_validate_payload_len_overflow_returns_false(void)
{
    MgmtCommandRequest req;
    mgmt_request_init(&req, MGMT_CMD_PING, "collector", NULL, 0);
    req.payload_len = MGMT_PAYLOAD_REQUEST_SIZE + 1;
    TEST_ASSERT_EQUAL(0, mgmt_request_validate(&req));
}

/* ============================================================
 * mgmt_result_str
 * ============================================================ */

void test_result_str_ok(void)
{
    TEST_ASSERT_EQUAL_STRING("OK", mgmt_result_str(MGMT_RESULT_OK));
}

void test_result_str_invalid_cmd(void)
{
    TEST_ASSERT_EQUAL_STRING("INVALID_CMD",
                             mgmt_result_str(MGMT_RESULT_INVALID_CMD));
}

void test_result_str_module_not_found(void)
{
    TEST_ASSERT_EQUAL_STRING("MODULE_NOT_FOUND",
                             mgmt_result_str(MGMT_RESULT_MODULE_NOT_FOUND));
}

void test_result_str_handler_failed(void)
{
    TEST_ASSERT_EQUAL_STRING("HANDLER_FAILED",
                             mgmt_result_str(MGMT_RESULT_HANDLER_FAILED));
}

void test_result_str_timeout(void)
{
    TEST_ASSERT_EQUAL_STRING("TIMEOUT", mgmt_result_str(MGMT_RESULT_TIMEOUT));
}

void test_result_str_buffer_overflow(void)
{
    TEST_ASSERT_EQUAL_STRING("BUFFER_OVERFLOW",
                             mgmt_result_str(MGMT_RESULT_BUFFER_OVERFLOW));
}

void test_result_str_unauthorized(void)
{
    TEST_ASSERT_EQUAL_STRING("UNAUTHORIZED",
                             mgmt_result_str(MGMT_RESULT_UNAUTHORIZED));
}

void test_result_str_internal_error(void)
{
    TEST_ASSERT_EQUAL_STRING("INTERNAL_ERROR",
                             mgmt_result_str(MGMT_RESULT_INTERNAL_ERROR));
}

void test_result_str_unknown(void)
{
    TEST_ASSERT_EQUAL_STRING("UNKNOWN", mgmt_result_str(0xFF));
}

/* ============================================================
 * mgmt_command_str
 * ============================================================ */

void test_command_str_ping(void)
{
    TEST_ASSERT_EQUAL_STRING("PING", mgmt_command_str(MGMT_CMD_PING));
}

void test_command_str_set_log_level(void)
{
    TEST_ASSERT_EQUAL_STRING("SET_LOG_LEVEL",
                             mgmt_command_str(MGMT_CMD_SET_LOG_LEVEL));
}

void test_command_str_get_status(void)
{
    TEST_ASSERT_EQUAL_STRING("GET_STATUS",
                             mgmt_command_str(MGMT_CMD_GET_STATUS));
}

void test_command_str_get_metrics(void)
{
    TEST_ASSERT_EQUAL_STRING("GET_METRICS",
                             mgmt_command_str(MGMT_CMD_GET_METRICS));
}

void test_command_str_set_buffer_size(void)
{
    TEST_ASSERT_EQUAL_STRING("SET_BUFFER_SIZE",
                             mgmt_command_str(MGMT_CMD_SET_BUFFER_SIZE));
}

void test_command_str_enable_profiling(void)
{
    TEST_ASSERT_EQUAL_STRING("ENABLE_PROFILING",
                             mgmt_command_str(MGMT_CMD_ENABLE_PROFILING));
}

void test_command_str_reset_metrics(void)
{
    TEST_ASSERT_EQUAL_STRING("RESET_METRICS",
                             mgmt_command_str(MGMT_CMD_RESET_METRICS));
}

void test_command_str_shutdown(void)
{
    TEST_ASSERT_EQUAL_STRING("SHUTDOWN", mgmt_command_str(MGMT_CMD_SHUTDOWN));
}

void test_command_str_get_config(void)
{
    TEST_ASSERT_EQUAL_STRING("GET_CONFIG",
                             mgmt_command_str(MGMT_CMD_GET_CONFIG));
}

void test_command_str_enable_tracing(void)
{
    TEST_ASSERT_EQUAL_STRING("ENABLE_TRACING",
                             mgmt_command_str(MGMT_CMD_ENABLE_TRACING));
}

void test_command_str_max_boundary(void)
{
    TEST_ASSERT_EQUAL_STRING("UNKNOWN", mgmt_command_str(MGMT_CMD_MAX));
}

void test_command_str_unknown(void)
{
    TEST_ASSERT_EQUAL_STRING("UNKNOWN", mgmt_command_str(0xFF));
}

/* ============================================================
 * mgmt_response_latency_us
 * ============================================================ */

void test_latency_null_returns_zero(void)
{
    TEST_ASSERT_EQUAL(0, mgmt_response_latency_us(NULL));
}

void test_latency_normal(void)
{
    MgmtCommandResponse resp;
    memset(&resp, 0, sizeof(resp));
    resp.request_timestamp.tv_sec  = 1000;
    resp.request_timestamp.tv_nsec = 0;
    resp.response_timestamp.tv_sec = 1001;
    resp.response_timestamp.tv_nsec = 0;
    TEST_ASSERT_EQUAL(1000000, mgmt_response_latency_us(&resp));
}

void test_latency_sub_second(void)
{
    MgmtCommandResponse resp;
    memset(&resp, 0, sizeof(resp));
    resp.request_timestamp.tv_sec  = 1000;
    resp.request_timestamp.tv_nsec = 0;
    resp.response_timestamp.tv_sec = 1000;
    resp.response_timestamp.tv_nsec = 500000000; /* 500ms */
    TEST_ASSERT_EQUAL(500000, mgmt_response_latency_us(&resp));
}

void test_latency_same_timestamp(void)
{
    MgmtCommandResponse resp;
    memset(&resp, 0, sizeof(resp));
    resp.request_timestamp.tv_sec  = 1000;
    resp.request_timestamp.tv_nsec = 0;
    resp.response_timestamp.tv_sec = 1000;
    resp.response_timestamp.tv_nsec = 0;
    TEST_ASSERT_EQUAL(0, mgmt_response_latency_us(&resp));
}

void test_latency_response_before_request_returns_zero(void)
{
    MgmtCommandResponse resp;
    memset(&resp, 0, sizeof(resp));
    resp.request_timestamp.tv_sec  = 1001;
    resp.response_timestamp.tv_sec = 1000;
    TEST_ASSERT_EQUAL(0, mgmt_response_latency_us(&resp));
}

/* ============================================================
 * mgmt_request_init
 * ============================================================ */

void test_init_sets_cmd_type(void)
{
    MgmtCommandRequest req;
    mgmt_request_init(&req, MGMT_CMD_GET_STATUS, "router", NULL, 0);
    TEST_ASSERT_EQUAL(MGMT_CMD_GET_STATUS, req.cmd_type);
}

void test_init_copies_module_name(void)
{
    MgmtCommandRequest req;
    mgmt_request_init(&req, MGMT_CMD_PING, "collector", NULL, 0);
    TEST_ASSERT_EQUAL_STRING("collector", req.target_module);
}

void test_init_null_module_leaves_empty(void)
{
    MgmtCommandRequest req;
    mgmt_request_init(&req, MGMT_CMD_PING, NULL, NULL, 0);
    TEST_ASSERT_EQUAL('\0', req.target_module[0]);
}

void test_init_payload_copied(void)
{
    MgmtCommandRequest req;
    const char payload[] = "hello";
    mgmt_request_init(&req, MGMT_CMD_PING, "collector", payload,
                      sizeof(payload));
    TEST_ASSERT_EQUAL_STRING("hello", (const char *)req.payload);
    TEST_ASSERT_EQUAL((uint16_t)sizeof(payload), req.payload_len);
}

void test_init_payload_truncated_at_max(void)
{
    MgmtCommandRequest req;
    uint8_t big[MGMT_PAYLOAD_REQUEST_SIZE + 64];
    memset(big, 0xAB, sizeof(big));
    mgmt_request_init(&req, MGMT_CMD_PING, "collector", big, sizeof(big));
    TEST_ASSERT_EQUAL(MGMT_PAYLOAD_REQUEST_SIZE, req.payload_len);
}

void test_init_null_payload_len_zero(void)
{
    MgmtCommandRequest req;
    mgmt_request_init(&req, MGMT_CMD_PING, "collector", NULL, 5);
    TEST_ASSERT_EQUAL(0, req.payload_len);
}

void test_init_module_name_truncated_null_terminated(void)
{
    MgmtCommandRequest req;
    char long_module[MGMT_MODULE_NAME_SIZE + 10];
    memset(long_module, 'A', sizeof(long_module) - 1);
    long_module[sizeof(long_module) - 1] = '\0';
    mgmt_request_init(&req, MGMT_CMD_PING, long_module, NULL, 0);
    TEST_ASSERT_EQUAL('\0', req.target_module[MGMT_MODULE_NAME_SIZE - 1]);
}

void test_init_null_req_does_not_crash(void)
{
    mgmt_request_init(NULL, MGMT_CMD_PING, "collector", NULL, 0);
    TEST_PASS();
}

/* ============================================================
 * mgmt_response_init
 * ============================================================ */

void test_resp_init_null_does_not_crash(void)
{
    mgmt_response_init(NULL, 1, MGMT_RESULT_OK, "router", NULL, 0);
    TEST_PASS();
}

void test_resp_init_sets_request_id(void)
{
    MgmtCommandResponse resp;
    mgmt_response_init(&resp, 42, MGMT_RESULT_OK, "router", NULL, 0);
    TEST_ASSERT_EQUAL(42, resp.request_id);
}

void test_resp_init_sets_result_code(void)
{
    MgmtCommandResponse resp;
    mgmt_response_init(&resp, 1, MGMT_RESULT_TIMEOUT, "router", NULL, 0);
    TEST_ASSERT_EQUAL(MGMT_RESULT_TIMEOUT, resp.result_code);
}

void test_resp_init_copies_module_name(void)
{
    MgmtCommandResponse resp;
    mgmt_response_init(&resp, 1, MGMT_RESULT_OK, "router", NULL, 0);
    TEST_ASSERT_EQUAL_STRING("router", resp.source_module);
}

void test_resp_init_null_module_leaves_empty(void)
{
    MgmtCommandResponse resp;
    mgmt_response_init(&resp, 1, MGMT_RESULT_OK, NULL, NULL, 0);
    TEST_ASSERT_EQUAL('\0', resp.source_module[0]);
}

void test_resp_init_payload_copied(void)
{
    MgmtCommandResponse resp;
    const char payload[] = "hello";
    mgmt_response_init(&resp, 1, MGMT_RESULT_OK, "router", payload,
                       sizeof(payload));
    TEST_ASSERT_EQUAL_STRING("hello", (const char *)resp.payload);
}

void test_resp_init_null_payload_no_crash(void)
{
    MgmtCommandResponse resp;
    mgmt_response_init(&resp, 1, MGMT_RESULT_OK, "router", NULL, 5);
    TEST_ASSERT_EQUAL('\0', resp.payload[0]);
}

void test_resp_init_payload_len_at_max(void)
{
    MgmtCommandResponse resp;
    uint8_t buf[MGMT_PAYLOAD_RESPONSE_SIZE];
    memset(buf, 0xAB, sizeof(buf));
    mgmt_response_init(&resp, 1, MGMT_RESULT_OK, "router", buf, sizeof(buf));
    TEST_ASSERT_EQUAL(0xAB, resp.payload[MGMT_PAYLOAD_RESPONSE_SIZE - 1]);
}

void test_resp_init_payload_truncated_at_max(void)
{
    MgmtCommandResponse resp;
    uint8_t big[MGMT_PAYLOAD_RESPONSE_SIZE + 64];
    memset(big, 0xCD, sizeof(big));
    mgmt_response_init(&resp, 1, MGMT_RESULT_OK, "router", big, sizeof(big));
    TEST_ASSERT_EQUAL(0xCD, resp.payload[0]);
    TEST_ASSERT_EQUAL(0xCD, resp.payload[MGMT_PAYLOAD_RESPONSE_SIZE - 1]);
}

void test_resp_init_module_truncated_null_terminated(void)
{
    MgmtCommandResponse resp;
    char long_module[MGMT_MODULE_NAME_SIZE + 10];
    memset(long_module, 'B', sizeof(long_module) - 1);
    long_module[sizeof(long_module) - 1] = '\0';
    mgmt_response_init(&resp, 1, MGMT_RESULT_OK, long_module, NULL, 0);
    TEST_ASSERT_EQUAL('\0', resp.source_module[MGMT_MODULE_NAME_SIZE - 1]);
}

void test_init_nonnull_payload_zero_len(void)
{
    MgmtCommandRequest req;
    const char dummy[] = "x";
    mgmt_request_init(&req, MGMT_CMD_PING, "mod", dummy, 0);
    TEST_ASSERT_EQUAL(0, req.payload_len);
}

void test_resp_init_nonnull_payload_zero_len(void)
{
    MgmtCommandResponse resp;
    const char dummy[] = "x";
    mgmt_response_init(&resp, 1, MGMT_RESULT_OK, "mod", dummy, 0);
    TEST_ASSERT_EQUAL('\0', resp.payload[0]);
}

/* ============================================================
 * main
 * ============================================================ */

int main(void)
{
    UNITY_BEGIN();

    RUN_TEST(test_validate_null_returns_false);
    RUN_TEST(test_validate_valid_request);
    RUN_TEST(test_validate_invalid_cmd_type);
    RUN_TEST(test_validate_empty_module_returns_false);
    RUN_TEST(test_validate_payload_len_at_max);
    RUN_TEST(test_validate_payload_len_overflow_returns_false);

    RUN_TEST(test_result_str_ok);
    RUN_TEST(test_result_str_invalid_cmd);
    RUN_TEST(test_result_str_module_not_found);
    RUN_TEST(test_result_str_handler_failed);
    RUN_TEST(test_result_str_timeout);
    RUN_TEST(test_result_str_buffer_overflow);
    RUN_TEST(test_result_str_unauthorized);
    RUN_TEST(test_result_str_internal_error);
    RUN_TEST(test_result_str_unknown);

    RUN_TEST(test_command_str_ping);
    RUN_TEST(test_command_str_set_log_level);
    RUN_TEST(test_command_str_get_status);
    RUN_TEST(test_command_str_get_metrics);
    RUN_TEST(test_command_str_set_buffer_size);
    RUN_TEST(test_command_str_enable_profiling);
    RUN_TEST(test_command_str_reset_metrics);
    RUN_TEST(test_command_str_shutdown);
    RUN_TEST(test_command_str_get_config);
    RUN_TEST(test_command_str_enable_tracing);
    RUN_TEST(test_command_str_max_boundary);
    RUN_TEST(test_command_str_unknown);

    RUN_TEST(test_latency_null_returns_zero);
    RUN_TEST(test_latency_normal);
    RUN_TEST(test_latency_sub_second);
    RUN_TEST(test_latency_same_timestamp);
    RUN_TEST(test_latency_response_before_request_returns_zero);

    RUN_TEST(test_init_sets_cmd_type);
    RUN_TEST(test_init_copies_module_name);
    RUN_TEST(test_init_null_module_leaves_empty);
    RUN_TEST(test_init_payload_copied);
    RUN_TEST(test_init_payload_truncated_at_max);
    RUN_TEST(test_init_null_payload_len_zero);
    RUN_TEST(test_init_module_name_truncated_null_terminated);
    RUN_TEST(test_init_null_req_does_not_crash);

    RUN_TEST(test_resp_init_null_does_not_crash);
    RUN_TEST(test_resp_init_sets_request_id);
    RUN_TEST(test_resp_init_sets_result_code);
    RUN_TEST(test_resp_init_copies_module_name);
    RUN_TEST(test_resp_init_null_module_leaves_empty);
    RUN_TEST(test_resp_init_payload_copied);
    RUN_TEST(test_resp_init_null_payload_no_crash);
    RUN_TEST(test_resp_init_payload_len_at_max);
    RUN_TEST(test_resp_init_payload_truncated_at_max);
    RUN_TEST(test_resp_init_module_truncated_null_terminated);
    RUN_TEST(test_init_nonnull_payload_zero_len);
    RUN_TEST(test_resp_init_nonnull_payload_zero_len);

    return UNITY_END();
}

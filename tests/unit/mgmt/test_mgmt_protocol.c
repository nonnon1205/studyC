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

void test_command_str_shutdown(void)
{
    TEST_ASSERT_EQUAL_STRING("SHUTDOWN", mgmt_command_str(MGMT_CMD_SHUTDOWN));
}

void test_command_str_unknown(void)
{
    TEST_ASSERT_EQUAL_STRING("UNKNOWN", mgmt_command_str(0xFF));
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

void test_init_null_req_does_not_crash(void)
{
    mgmt_request_init(NULL, MGMT_CMD_PING, "collector", NULL, 0);
    TEST_PASS();
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
    RUN_TEST(test_validate_payload_len_overflow_returns_false);

    RUN_TEST(test_result_str_ok);
    RUN_TEST(test_result_str_invalid_cmd);
    RUN_TEST(test_result_str_unknown);

    RUN_TEST(test_command_str_ping);
    RUN_TEST(test_command_str_shutdown);
    RUN_TEST(test_command_str_unknown);

    RUN_TEST(test_init_sets_cmd_type);
    RUN_TEST(test_init_copies_module_name);
    RUN_TEST(test_init_null_module_leaves_empty);
    RUN_TEST(test_init_payload_copied);
    RUN_TEST(test_init_payload_truncated_at_max);
    RUN_TEST(test_init_null_req_does_not_crash);

    return UNITY_END();
}

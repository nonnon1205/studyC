/**
 * @file test_mgmt_send_unity.c
 * @brief mgmt_send_command の Unity テスト（スタブによるシステムコール差し替え）
 */
#include <string.h>
#include "unity.h"
#include "mgmt_send.h"
#include "stub_syscalls.h"

static MgmtCommandRequest  req;
static MgmtCommandResponse resp;

void setUp(void)
{
    stubs_reset();
    memset(&req,  0, sizeof(req));
    memset(&resp, 0, sizeof(resp));
    req.cmd_type = MGMT_CMD_PING;
    strncpy(req.target_module, "collector", MGMT_MODULE_NAME_SIZE - 1);
}

void tearDown(void) {}

/* NULL 引数は即 -1（システムコール一切呼ばれない） */
void test_null_args_return_minus_one(void)
{
    TEST_ASSERT_EQUAL(-1, mgmt_send_command(NULL,       &req,  &resp, 1000));
    TEST_ASSERT_EQUAL(-1, mgmt_send_command("/tmp/s",   NULL,  &resp, 1000));
    TEST_ASSERT_EQUAL(-1, mgmt_send_command("/tmp/s",   &req,  NULL,  1000));
    TEST_ASSERT_EQUAL(0, g_stubs.socket_calls);
}

/* socket() 失敗 → 即 return。unlink/close は呼ばれない */
void test_socket_fail_returns_minus_one(void)
{
    g_stubs.socket_ret = -1;

    TEST_ASSERT_EQUAL(-1, mgmt_send_command("/tmp/mgmt.sock", &req, &resp, 1000));
    TEST_ASSERT_EQUAL(1, g_stubs.socket_calls);
    TEST_ASSERT_EQUAL(0, g_stubs.unlink_calls);
    TEST_ASSERT_EQUAL(0, g_stubs.close_calls);
}

/* bind() 失敗 → cleanup。unlink 2回・close 1回 */
void test_bind_fail_returns_minus_one(void)
{
    g_stubs.socket_ret = 5;
    g_stubs.bind_ret   = -1;

    TEST_ASSERT_EQUAL(-1, mgmt_send_command("/tmp/mgmt.sock", &req, &resp, 1000));
    TEST_ASSERT_EQUAL(1, g_stubs.bind_calls);
    TEST_ASSERT_EQUAL(0, g_stubs.sendto_calls);
    TEST_ASSERT_EQUAL(2, g_stubs.unlink_calls);
    TEST_ASSERT_EQUAL(1, g_stubs.close_calls);
}

/* poll() タイムアウト → recv は呼ばれない・cleanup は呼ばれる */
void test_timeout_returns_minus_one(void)
{
    g_stubs.socket_ret  = 5;
    g_stubs.bind_ret    = 0;
    g_stubs.sendto_ret  = (ssize_t)sizeof(req);
    g_stubs.poll_ret    = 0;   /* タイムアウト */

    TEST_ASSERT_EQUAL(-1, mgmt_send_command("/tmp/mgmt.sock", &req, &resp, 1000));
    TEST_ASSERT_EQUAL(1, g_stubs.poll_calls);
    TEST_ASSERT_EQUAL(0, g_stubs.recv_calls);
    TEST_ASSERT_EQUAL(2, g_stubs.unlink_calls);
    TEST_ASSERT_EQUAL(1, g_stubs.close_calls);
}

/* 正常系：全ステップ成功 → 0 を返す */
void test_success_returns_zero(void)
{
    g_stubs.socket_ret  = 5;
    g_stubs.bind_ret    = 0;
    g_stubs.sendto_ret  = (ssize_t)sizeof(req);
    g_stubs.poll_ret    = 1;
    g_stubs.recv_ret    = (ssize_t)sizeof(resp);

    TEST_ASSERT_EQUAL(0, mgmt_send_command("/tmp/mgmt.sock", &req, &resp, 1000));
    TEST_ASSERT_EQUAL(1, g_stubs.recv_calls);
    TEST_ASSERT_EQUAL(2, g_stubs.unlink_calls);
    TEST_ASSERT_EQUAL(1, g_stubs.close_calls);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_null_args_return_minus_one);
    RUN_TEST(test_socket_fail_returns_minus_one);
    RUN_TEST(test_bind_fail_returns_minus_one);
    RUN_TEST(test_timeout_returns_minus_one);
    RUN_TEST(test_success_returns_zero);
    return UNITY_END();
}

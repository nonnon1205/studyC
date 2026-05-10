/**
 * @file test_socket_ops_cmock.c
 * @brief CMock デモ：socket_ops 抽象レイヤーを通じた mgmt_send_command 相当のテスト
 *
 * socket_create / socket_bind / ... を呼び出す仮想の send_command を
 * CMock で生成したモックで検証する学習用サンプル。
 */
#include "unity.h"
#include "mock_socket_ops.h"  /* CMock が自動生成 */

/* ============================================================
 * テスト対象の仮想関数（本来は mgmt_send.c の中にある想定）
 * ここでは socket_ops.h の抽象レイヤーを通じてシステムコールを呼ぶ
 * ============================================================ */
static int fake_send(const char *path, int timeout_ms)
{
    if (!path) return -1;

    int fd = socket_create(AF_UNIX, SOCK_DGRAM, 0);
    if (fd < 0) return -1;

    struct sockaddr addr = {0};
    if (socket_bind(fd, &addr, sizeof(addr)) < 0) {
        socket_unlink(path);
        socket_unlink("/tmp/client.sock");
        socket_close(fd);
        return -1;
    }

    struct pollfd pfd = {.fd = fd, .events = POLLIN};
    if (socket_poll(&pfd, 1, timeout_ms) <= 0) {
        socket_unlink(path);
        socket_unlink("/tmp/client.sock");
        socket_close(fd);
        return -1;
    }

    char buf[64];
    socket_recv(fd, buf, sizeof(buf), 0);
    socket_unlink(path);
    socket_unlink("/tmp/client.sock");
    socket_close(fd);
    return 0;
}

/* ============================================================
 * fixture
 * ============================================================ */
void setUp(void)
{
    mock_socket_ops_Init();   /* CMock 内部状態を初期化 */
}

void tearDown(void)
{
    mock_socket_ops_Verify();   /* 未消化の Expect があれば FAIL */
    mock_socket_ops_Destroy();
}

/* ============================================================
 * テストケース
 * ============================================================ */

/* NULL 引数は即 -1（socket_create すら呼ばれない） */
void test_null_path_returns_minus_one(void)
{
    /* Expect を一切書かない → 呼ばれたら Verify で FAIL */
    TEST_ASSERT_EQUAL(-1, fake_send(NULL, 1000));
}

/* socket_create 失敗 → 即 return */
void test_socket_fail_returns_minus_one(void)
{
    socket_create_ExpectAndReturn(AF_UNIX, SOCK_DGRAM, 0, -1);
    /* unlink / close の Expect なし → 呼ばれたら FAIL */

    TEST_ASSERT_EQUAL(-1, fake_send("/tmp/mgmt.sock", 1000));
}

/* socket_bind 失敗 → cleanup */
void test_bind_fail_returns_minus_one(void)
{
    socket_create_ExpectAndReturn(AF_UNIX, SOCK_DGRAM, 0, 5);
    socket_bind_IgnoreAndReturn(-1);          /* 引数は気にしない */
    socket_unlink_IgnoreAndReturn(0);         /* 2 回呼ばれる */
    socket_unlink_IgnoreAndReturn(0);
    socket_close_ExpectAndReturn(5, 0);

    TEST_ASSERT_EQUAL(-1, fake_send("/tmp/mgmt.sock", 1000));
}

/* poll タイムアウト → recv は呼ばれない */
void test_timeout_returns_minus_one(void)
{
    socket_create_ExpectAndReturn(AF_UNIX, SOCK_DGRAM, 0, 5);
    socket_bind_IgnoreAndReturn(0);
    socket_poll_IgnoreAndReturn(0);           /* タイムアウト = 0 */
    socket_unlink_IgnoreAndReturn(0);
    socket_unlink_IgnoreAndReturn(0);
    socket_close_ExpectAndReturn(5, 0);
    /* socket_recv の Expect なし → 呼ばれたら FAIL */

    TEST_ASSERT_EQUAL(-1, fake_send("/tmp/mgmt.sock", 1000));
}

/* 正常系 */
void test_success_returns_zero(void)
{
    socket_create_ExpectAndReturn(AF_UNIX, SOCK_DGRAM, 0, 5);
    socket_bind_IgnoreAndReturn(0);
    socket_poll_IgnoreAndReturn(1);
    socket_recv_IgnoreAndReturn(32);
    socket_unlink_IgnoreAndReturn(0);
    socket_unlink_IgnoreAndReturn(0);
    socket_close_ExpectAndReturn(5, 0);

    TEST_ASSERT_EQUAL(0, fake_send("/tmp/mgmt.sock", 1000));
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_null_path_returns_minus_one);
    RUN_TEST(test_socket_fail_returns_minus_one);
    RUN_TEST(test_bind_fail_returns_minus_one);
    RUN_TEST(test_timeout_returns_minus_one);
    RUN_TEST(test_success_returns_zero);
    return UNITY_END();
}

/**
 * @file test_mgmt_send_real_gmock.cpp
 * @brief 実際の mgmt_send_command (C関数) を Google Mock でテスト
 *
 * システムコール (socket/bind/sendto/poll/recv/close/unlink) を
 * --wrap リンカオプションで差し替え、C 関数の振る舞いをモックで検証する。
 */

extern "C" {
#include "mgmt_send.h"
}

#include <sys/socket.h>
#include <sys/un.h>
#include <poll.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

using ::testing::_;
using ::testing::Return;
using ::testing::AnyNumber;

/* ============================================================
 * モッククラス：システムコールの抽象
 * ============================================================ */

class MockSyscalls
{
public:
    MOCK_METHOD(int,     socket,  (int domain, int type, int protocol),              ());
    MOCK_METHOD(int,     bind,    (int fd, const struct sockaddr *addr, socklen_t l), ());
    MOCK_METHOD(ssize_t, sendto,  (int fd, const void *buf, size_t len, int flags,
                                   const struct sockaddr *dst, socklen_t dstlen),    ());
    MOCK_METHOD(int,     poll,    (struct pollfd *fds, nfds_t nfds, int timeout),    ());
    MOCK_METHOD(ssize_t, recv,    (int fd, void *buf, size_t len, int flags),        ());
    MOCK_METHOD(int,     close,   (int fd),                                          ());
    MOCK_METHOD(int,     unlink,  (const char *path),                                ());
};

/* __wrap 関数からアクセスするグローバルポインタ */
static MockSyscalls *g_mock = nullptr;

/* ============================================================
 * __wrap 関数：--wrap オプションでシステムコールを横取りする
 * ============================================================ */

extern "C" {
    int __wrap_socket(int d, int t, int p)
    {
        return g_mock->socket(d, t, p);
    }
    int __wrap_bind(int fd, const struct sockaddr *a, socklen_t l)
    {
        return g_mock->bind(fd, a, l);
    }
    ssize_t __wrap_sendto(int fd, const void *buf, size_t len, int flags,
                          const struct sockaddr *dst, socklen_t dl)
    {
        return g_mock->sendto(fd, buf, len, flags, dst, dl);
    }
    int __wrap_poll(struct pollfd *fds, nfds_t n, int t)
    {
        return g_mock->poll(fds, n, t);
    }
    ssize_t __wrap_recv(int fd, void *buf, size_t len, int flags)
    {
        return g_mock->recv(fd, buf, len, flags);
    }
    int __wrap_close(int fd)
    {
        return g_mock->close(fd);
    }
    int __wrap_unlink(const char *p)
    {
        return g_mock->unlink(p);
    }
}

/* ============================================================
 * fixture
 * ============================================================ */

class MgmtSendCommandTest : public ::testing::Test
{
protected:
    MockSyscalls        mock;
    MgmtCommandRequest  req  = {};
    MgmtCommandResponse resp = {};

    void SetUp() override
    {
        g_mock = &mock;
        req.cmd_type = MGMT_CMD_PING;
        strncpy(req.target_module, "collector", MGMT_MODULE_NAME_SIZE - 1);
    }

    void TearDown() override
    {
        g_mock = nullptr;
    }
};

/* ============================================================
 * テストケース
 * ============================================================ */

/* NULL 引数は即 -1（システムコール一切呼ばれない） */
TEST_F(MgmtSendCommandTest, NullArgsReturnMinusOne)
{
    EXPECT_CALL(mock, socket(_, _, _)).Times(0);

    EXPECT_EQ(-1, mgmt_send_command(nullptr, &req,  &resp, 1000));
    EXPECT_EQ(-1, mgmt_send_command("/tmp/s", nullptr, &resp, 1000));
    EXPECT_EQ(-1, mgmt_send_command("/tmp/s", &req,  nullptr, 1000));
}

/* socket() 失敗 → 即 return。unlink/close は呼ばれない */
TEST_F(MgmtSendCommandTest, SocketFailReturnsMinusOne)
{
    EXPECT_CALL(mock, socket(AF_UNIX, SOCK_DGRAM, 0)).WillOnce(Return(-1));
    EXPECT_CALL(mock, unlink(_)).Times(0);
    EXPECT_CALL(mock, close(_)).Times(0);

    EXPECT_EQ(-1, mgmt_send_command("/tmp/mgmt.sock", &req, &resp, 1000));
}

/* bind() 失敗 → cleanup。unlink 2回・close 1回 */
TEST_F(MgmtSendCommandTest, BindFailReturnsMinusOne)
{
    EXPECT_CALL(mock, socket(_, _, _)).WillOnce(Return(5));
    EXPECT_CALL(mock, unlink(_)).Times(2).WillRepeatedly(Return(0));
    EXPECT_CALL(mock, bind(5, _, _)).WillOnce(Return(-1));
    EXPECT_CALL(mock, sendto(_, _, _, _, _, _)).Times(0);
    EXPECT_CALL(mock, close(5)).Times(1).WillOnce(Return(0));

    EXPECT_EQ(-1, mgmt_send_command("/tmp/mgmt.sock", &req, &resp, 1000));
}

/* poll() タイムアウト → recv は呼ばれない・cleanup は呼ばれる */
TEST_F(MgmtSendCommandTest, TimeoutReturnsMinusOne)
{
    EXPECT_CALL(mock, socket(_, _, _)).WillOnce(Return(5));
    EXPECT_CALL(mock, unlink(_)).Times(2).WillRepeatedly(Return(0));
    EXPECT_CALL(mock, bind(5, _, _)).WillOnce(Return(0));
    EXPECT_CALL(mock, sendto(5, _, _, _, _, _))
        .WillOnce(Return((ssize_t)sizeof(req)));
    EXPECT_CALL(mock, poll(_, 1, 1000)).WillOnce(Return(0));
    EXPECT_CALL(mock, recv(_, _, _, _)).Times(0);
    EXPECT_CALL(mock, close(5)).Times(1).WillOnce(Return(0));

    EXPECT_EQ(-1, mgmt_send_command("/tmp/mgmt.sock", &req, &resp, 1000));
}

/* 正常系：全ステップ成功 → 0 を返す */
TEST_F(MgmtSendCommandTest, SuccessReturnsZero)
{
    EXPECT_CALL(mock, socket(_, _, _)).WillOnce(Return(5));
    EXPECT_CALL(mock, unlink(_)).Times(2).WillRepeatedly(Return(0));
    EXPECT_CALL(mock, bind(5, _, _)).WillOnce(Return(0));
    EXPECT_CALL(mock, sendto(5, _, _, _, _, _))
        .WillOnce(Return((ssize_t)sizeof(req)));
    EXPECT_CALL(mock, poll(_, 1, 1000)).WillOnce(Return(1));
    EXPECT_CALL(mock, recv(5, _, sizeof(MgmtCommandResponse), _))
        .WillOnce(Return((ssize_t)sizeof(MgmtCommandResponse)));
    EXPECT_CALL(mock, close(5)).Times(1).WillOnce(Return(0));

    EXPECT_EQ(0, mgmt_send_command("/tmp/mgmt.sock", &req, &resp, 1000));
}

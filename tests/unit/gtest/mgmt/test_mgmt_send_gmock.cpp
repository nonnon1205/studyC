/**
 * @file test_mgmt_send_gmock.cpp
 * @brief MgmtSender の Google Mock テスト
 *        実際のソケットを使わずにソケット操作をモックで差し替える
 */

extern "C" {
#include "mgmt_protocol.h"
}

#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include "ISocketOps.h"
#include "MgmtSender.h"

using ::testing::Return;
using ::testing::_;

/* ============================================================
 * MockSocketOps: ISocketOps の全メソッドをモック化
 * ============================================================ */

class MockSocketOps : public ISocketOps
{
public:
    MOCK_METHOD(int,  createSocket,  (),                                              (override));
    MOCK_METHOD(int,  bindClient,    (int fd, const char *tmp_path),                  (override));
    MOCK_METHOD(int,  sendRequest,   (int fd, const char *path,
                                      const MgmtCommandRequest *req),                 (override));
    MOCK_METHOD(int,  waitReadable,  (int fd, int timeout_ms),                        (override));
    MOCK_METHOD(int,  recvResponse,  (int fd, MgmtCommandResponse *resp),             (override));
    MOCK_METHOD(void, cleanup,       (int fd, const char *tmp_path),                  (override));
};

/* ============================================================
 * fixture
 * ============================================================ */

class MgmtSenderTest : public ::testing::Test
{
protected:
    MockSocketOps mock;
    MgmtCommandRequest  req  = {};
    MgmtCommandResponse resp = {};

    void SetUp() override
    {
        req.cmd_type = MGMT_CMD_PING;
        strncpy(req.target_module, "collector", MGMT_MODULE_NAME_SIZE - 1);
    }
};

/* ============================================================
 * テストケース
 * ============================================================ */

/* createSocket が失敗したら -1 を返し、以降は何も呼ばれない */
TEST_F(MgmtSenderTest, CreateSocketFailReturnsError)
{
    EXPECT_CALL(mock, createSocket()).WillOnce(Return(-1));
    EXPECT_CALL(mock, bindClient(_, _)).Times(0);
    EXPECT_CALL(mock, cleanup(_, _)).Times(0);

    MgmtSender sender(&mock);
    EXPECT_EQ(-1, sender.sendCommand("/tmp/mgmt.sock", &req, &resp, 1000));
}

/* bind が失敗したら -1 を返し、cleanup は呼ばれる */
TEST_F(MgmtSenderTest, BindFailReturnsErrorAndCleansUp)
{
    EXPECT_CALL(mock, createSocket()).WillOnce(Return(5));
    EXPECT_CALL(mock, bindClient(5, _)).WillOnce(Return(-1));
    EXPECT_CALL(mock, sendRequest(_, _, _)).Times(0);
    EXPECT_CALL(mock, cleanup(5, _)).Times(1);

    MgmtSender sender(&mock);
    EXPECT_EQ(-1, sender.sendCommand("/tmp/mgmt.sock", &req, &resp, 1000));
}

/* タイムアウトしたら -1 を返す */
TEST_F(MgmtSenderTest, TimeoutReturnsError)
{
    EXPECT_CALL(mock, createSocket()).WillOnce(Return(5));
    EXPECT_CALL(mock, bindClient(5, _)).WillOnce(Return(0));
    EXPECT_CALL(mock, sendRequest(5, _, _)).WillOnce(Return(0));
    EXPECT_CALL(mock, waitReadable(5, 1000)).WillOnce(Return(0));  // タイムアウト
    EXPECT_CALL(mock, recvResponse(_, _)).Times(0);
    EXPECT_CALL(mock, cleanup(5, _)).Times(1);

    MgmtSender sender(&mock);
    EXPECT_EQ(-1, sender.sendCommand("/tmp/mgmt.sock", &req, &resp, 1000));
}

/* 正常系：全ステップ成功 */
TEST_F(MgmtSenderTest, SuccessReturnsZero)
{
    EXPECT_CALL(mock, createSocket()).WillOnce(Return(5));
    EXPECT_CALL(mock, bindClient(5, _)).WillOnce(Return(0));
    EXPECT_CALL(mock, sendRequest(5, _, _)).WillOnce(Return(0));
    EXPECT_CALL(mock, waitReadable(5, 1000)).WillOnce(Return(1));
    EXPECT_CALL(mock, recvResponse(5, _)).WillOnce(Return(0));
    EXPECT_CALL(mock, cleanup(5, _)).Times(1);

    MgmtSender sender(&mock);
    EXPECT_EQ(0, sender.sendCommand("/tmp/mgmt.sock", &req, &resp, 1000));
}

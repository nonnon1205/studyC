/**
 * @file test_mgmt_protocol_gtest.cpp
 * @brief mgmt_protocol.c の Google Test による単体テスト（Unity との比較用）
 */

extern "C" {
#include "mgmt_protocol.h"
}

#include <cstring>
#include <gtest/gtest.h>

/* ============================================================
 * mgmt_result_str / mgmt_command_str
 * fixture 不要なので TEST を使う
 * ============================================================ */

TEST(MgmtResultStr, ReturnsOk)
{
    EXPECT_STREQ("OK", mgmt_result_str(MGMT_RESULT_OK));
}

TEST(MgmtResultStr, ReturnsUnknownForUndefinedCode)
{
    EXPECT_STREQ("UNKNOWN", mgmt_result_str(0xFF));
}

TEST(MgmtCommandStr, ReturnsPing)
{
    EXPECT_STREQ("PING", mgmt_command_str(MGMT_CMD_PING));
}

TEST(MgmtCommandStr, ReturnsUnknownForMax)
{
    EXPECT_STREQ("UNKNOWN", mgmt_command_str(MGMT_CMD_MAX));
}

/* ============================================================
 * mgmt_request_validate
 * SetUp で req を正常な状態に初期化し、各テストで1つだけ壊す
 * ============================================================ */

class MgmtRequestValidateTest : public ::testing::Test
{
protected:
    MgmtCommandRequest req;

    void SetUp() override
    {
        memset(&req, 0, sizeof(req));
        req.cmd_type = MGMT_CMD_PING;
        strncpy(req.target_module, "collector", MGMT_MODULE_NAME_SIZE - 1);
        req.payload_len = 0;
    }
};

TEST_F(MgmtRequestValidateTest, NullReturnsFalse)
{
    EXPECT_EQ(0, mgmt_request_validate(NULL));
}

TEST_F(MgmtRequestValidateTest, ValidRequestReturnsTrue)
{
    EXPECT_EQ(1, mgmt_request_validate(&req));
}

TEST_F(MgmtRequestValidateTest, InvalidCmdTypeReturnsFalse)
{
    req.cmd_type = MGMT_CMD_MAX;
    EXPECT_EQ(0, mgmt_request_validate(&req));
}

TEST_F(MgmtRequestValidateTest, EmptyModuleReturnsFalse)
{
    req.target_module[0] = '\0';
    EXPECT_EQ(0, mgmt_request_validate(&req));
}

TEST_F(MgmtRequestValidateTest, PayloadLenAtMaxReturnsTrue)
{
    req.payload_len = MGMT_PAYLOAD_REQUEST_SIZE;
    EXPECT_EQ(1, mgmt_request_validate(&req));
}

TEST_F(MgmtRequestValidateTest, PayloadLenOverflowReturnsFalse)
{
    req.payload_len = MGMT_PAYLOAD_REQUEST_SIZE + 1;
    EXPECT_EQ(0, mgmt_request_validate(&req));
}

/* ============================================================
 * mgmt_response_latency_us
 * ============================================================ */

class MgmtLatencyTest : public ::testing::Test
{
protected:
    MgmtCommandResponse resp;

    void SetUp() override
    {
        memset(&resp, 0, sizeof(resp));
    }
};

TEST_F(MgmtLatencyTest, NullReturnsZero)
{
    EXPECT_EQ(0ULL, mgmt_response_latency_us(NULL));
}

TEST_F(MgmtLatencyTest, NormalLatency)
{
    resp.request_timestamp  = {1000, 0};
    resp.response_timestamp = {1001, 0};
    EXPECT_EQ(1000000ULL, mgmt_response_latency_us(&resp));
}

TEST_F(MgmtLatencyTest, SameTimestampReturnsZero)
{
    resp.request_timestamp  = {1000, 0};
    resp.response_timestamp = {1000, 0};
    EXPECT_EQ(0ULL, mgmt_response_latency_us(&resp));
}

TEST_F(MgmtLatencyTest, ResponseBeforeRequestReturnsZero)
{
    resp.request_timestamp  = {1001, 0};
    resp.response_timestamp = {1000, 0};
    EXPECT_EQ(0ULL, mgmt_response_latency_us(&resp));
}

/* ============================================================
 * mgmt_request_init
 * ============================================================ */

class MgmtRequestInitTest : public ::testing::Test
{
protected:
    MgmtCommandRequest req;

    void SetUp() override
    {
        memset(&req, 0, sizeof(req));
    }
};

TEST_F(MgmtRequestInitTest, NullDoesNotCrash)
{
    EXPECT_NO_FATAL_FAILURE(
        mgmt_request_init(NULL, MGMT_CMD_PING, "mod", NULL, 0));
}

TEST_F(MgmtRequestInitTest, SetsCmdType)
{
    mgmt_request_init(&req, MGMT_CMD_GET_STATUS, "mod", NULL, 0);
    EXPECT_EQ(MGMT_CMD_GET_STATUS, req.cmd_type);
}

TEST_F(MgmtRequestInitTest, NullModuleLeavesEmpty)
{
    mgmt_request_init(&req, MGMT_CMD_PING, NULL, NULL, 0);
    EXPECT_EQ('\0', req.target_module[0]);
}

TEST_F(MgmtRequestInitTest, PayloadCopied)
{
    const char payload[] = "hello";
    mgmt_request_init(&req, MGMT_CMD_PING, "mod", payload, sizeof(payload));
    EXPECT_STREQ("hello", reinterpret_cast<const char *>(req.payload));
    EXPECT_EQ(static_cast<uint16_t>(sizeof(payload)), req.payload_len);
}

TEST_F(MgmtRequestInitTest, PayloadTruncatedAtMax)
{
    uint8_t big[MGMT_PAYLOAD_REQUEST_SIZE + 64] = {};
    mgmt_request_init(&req, MGMT_CMD_PING, "mod", big, sizeof(big));
    EXPECT_EQ(static_cast<uint16_t>(MGMT_PAYLOAD_REQUEST_SIZE), req.payload_len);
}

/* ============================================================
 * mgmt_response_init
 * ============================================================ */

class MgmtResponseInitTest : public ::testing::Test
{
protected:
    MgmtCommandResponse resp;

    void SetUp() override
    {
        memset(&resp, 0, sizeof(resp));
    }
};

TEST_F(MgmtResponseInitTest, NullDoesNotCrash)
{
    EXPECT_NO_FATAL_FAILURE(
        mgmt_response_init(NULL, 1, MGMT_RESULT_OK, "mod", NULL, 0));
}

TEST_F(MgmtResponseInitTest, SetsRequestId)
{
    mgmt_response_init(&resp, 42, MGMT_RESULT_OK, "mod", NULL, 0);
    EXPECT_EQ(42U, resp.request_id);
}

TEST_F(MgmtResponseInitTest, NullModuleLeavesEmpty)
{
    mgmt_response_init(&resp, 1, MGMT_RESULT_OK, NULL, NULL, 0);
    EXPECT_EQ('\0', resp.source_module[0]);
}

TEST_F(MgmtResponseInitTest, PayloadTruncatedAtMax)
{
    uint8_t big[MGMT_PAYLOAD_RESPONSE_SIZE + 64];
    memset(big, 0xCD, sizeof(big));
    mgmt_response_init(&resp, 1, MGMT_RESULT_OK, "mod", big, sizeof(big));
    EXPECT_EQ(0xCD, resp.payload[0]);
    EXPECT_EQ(0xCD, resp.payload[MGMT_PAYLOAD_RESPONSE_SIZE - 1]);
}

/**
 * @file test_mgmt_protocol_cpptest.cpp
 * @brief CppTest による mgmt_protocol.c のテスト（GoogleTest と比較用）
 */
#include <cpptest.h>
#include <cstring>

extern "C" {
#include "mgmt_protocol.h"
}

/* ============================================================
 * Test::Suite を継承してテストスイートを定義する
 * GoogleTest の ::testing::Test に相当
 * ============================================================ */

class MgmtProtocolSuite : public Test::Suite
{
public:
    MgmtProtocolSuite()
    {
        /* コンストラクタでテスト関数を登録する（GoogleTest はマクロが自動登録） */
        TEST_ADD(MgmtProtocolSuite::test_result_str_ok);
        TEST_ADD(MgmtProtocolSuite::test_result_str_unknown);
        TEST_ADD(MgmtProtocolSuite::test_validate_null_returns_false);
        TEST_ADD(MgmtProtocolSuite::test_validate_valid_request);
        TEST_ADD(MgmtProtocolSuite::test_validate_empty_module_returns_false);
        TEST_ADD(MgmtProtocolSuite::test_latency_null_returns_zero);
        TEST_ADD(MgmtProtocolSuite::test_latency_normal);
    }

protected:
    void setup() override
    {
        memset(&req, 0, sizeof(req));
        req.cmd_type = MGMT_CMD_PING;
        strncpy(req.target_module, "collector", MGMT_MODULE_NAME_SIZE - 1);
    }

    void tear_down() override {}

private:
    MgmtCommandRequest req;

    void test_result_str_ok()
    {
        /* 文字列比較は TEST_ASSERT(strcmp) — CppTest に EQUAL_STR はない */
        TEST_ASSERT(strcmp("OK", mgmt_result_str(MGMT_RESULT_OK)) == 0);
    }

    void test_result_str_unknown()
    {
        TEST_ASSERT(strcmp("UNKNOWN", mgmt_result_str((MgmtResultCode)255)) == 0);
    }

    void test_validate_null_returns_false()
    {
        TEST_ASSERT_EQUALS(0, mgmt_request_validate(NULL));
    }

    void test_validate_valid_request()
    {
        TEST_ASSERT_EQUALS(1, mgmt_request_validate(&req));
    }

    void test_validate_empty_module_returns_false()
    {
        req.target_module[0] = '\0';
        TEST_ASSERT_EQUALS(0, mgmt_request_validate(&req));
    }

    void test_latency_null_returns_zero()
    {
        TEST_ASSERT_EQUALS((uint64_t)0, mgmt_response_latency_us(NULL));
    }

    void test_latency_normal()
    {
        MgmtCommandResponse resp = {};
        resp.request_timestamp  = {1, 0};
        resp.response_timestamp = {2, 500000000};  /* 1.5 秒後 = 1500000 us */
        TEST_ASSERT_EQUALS((uint64_t)1500000, mgmt_response_latency_us(&resp));
    }
};

/* ============================================================
 * main：出力形式をここで選択する（CppTest の最大の特徴）
 * ============================================================ */
int main(int argc, char *argv[])
{
    MgmtProtocolSuite suite;

    /* 引数で出力形式を切り替える */
    if (argc > 1 && std::string(argv[1]) == "--html") {
        Test::HtmlOutput output;
        suite.run(output);
        output.generate(std::cout, true, "MgmtProtocol");
        return 0;
    }

    Test::TextOutput output(Test::TextOutput::Verbose);
    return suite.run(output) ? 0 : 1;
}

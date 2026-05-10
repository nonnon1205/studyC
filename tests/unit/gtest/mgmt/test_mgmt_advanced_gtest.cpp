/**
 * @file test_mgmt_advanced_gtest.cpp
 * @brief GoogleTest 応用機能のデモ
 *
 * - TEST_P: パラメータ化テスト（同じ検証を複数入力値で回す）
 * - Death テスト: abort()/assert() で落ちることを検証する
 */

extern "C" {
#include "mgmt_protocol.h"
}

#include <gtest/gtest.h>
#include <cstring>

/* ============================================================
 * TEST_P：パラメータ化テスト
 *
 * mgmt_request_validate() は無効な入力を全て false で返す。
 * 「無効なコマンドタイプ」をいくつか試したい場合、
 * TEST_F を量産する代わりに TEST_P で1本にまとめられる。
 * ============================================================ */

/* パラメータの型を決める（ここでは MgmtCommandType の無効値） */
class InvalidCmdTypeTest : public ::testing::TestWithParam<int>
{
protected:
    MgmtCommandRequest req;

    void SetUp() override
    {
        memset(&req, 0, sizeof(req));
        strncpy(req.target_module, "collector", MGMT_MODULE_NAME_SIZE - 1);
    }
};

TEST_P(InvalidCmdTypeTest, ReturnsFalse)
{
    req.cmd_type = (MgmtCommandType)GetParam();   /* パラメータを取り出す */
    EXPECT_EQ(0, mgmt_request_validate(&req));
}

/* テストに流し込む値の一覧 */
INSTANTIATE_TEST_SUITE_P(
    InvalidValues,
    InvalidCmdTypeTest,
    ::testing::Values(
        -1,                          /* 負値 */
        MGMT_CMD_MAX,                /* 上限そのもの（境界） */
        (int)MGMT_CMD_MAX + 1,       /* 上限+1 */
        255                          /* uint8_t の最大値 */
    )
);

/* ============================================================
 * TEST_P：正常値のパラメータ化
 *
 * mgmt_result_str() が全ての有効コードに対して
 * "UNKNOWN" 以外を返すことを一括検証する。
 * ============================================================ */

class ResultStrValidTest : public ::testing::TestWithParam<MgmtResultCode> {};

TEST_P(ResultStrValidTest, DoesNotReturnUnknown)
{
    const char *s = mgmt_result_str(GetParam());
    EXPECT_STRNE("UNKNOWN", s);
}

INSTANTIATE_TEST_SUITE_P(
    AllValidCodes,
    ResultStrValidTest,
    ::testing::Values(
        MGMT_RESULT_OK,
        MGMT_RESULT_INVALID_CMD,
        MGMT_RESULT_MODULE_NOT_FOUND,
        MGMT_RESULT_HANDLER_FAILED,
        MGMT_RESULT_TIMEOUT,
        MGMT_RESULT_BUFFER_OVERFLOW,
        MGMT_RESULT_UNAUTHORIZED,
        MGMT_RESULT_INTERNAL_ERROR
    )
);

/* ============================================================
 * Death テスト
 *
 * プロダクションコードが「意図的に落ちる」ことを検証する。
 * assert() や abort() を使った防御コードの確認に使う。
 *
 * EXPECT_DEATH(式, 正規表現)
 *   → 式を実行してプロセスが異常終了することを期待する
 *   → 正規表現は stderr の出力にマッチさせる（空文字列なら無条件）
 * ============================================================ */

/* デモ用：NULL を渡したら assert で落ちる関数 */
static void must_not_be_null(const char *p)
{
    /* assert は NDEBUG 無効時にのみ有効 */
    assert(p != nullptr);
    (void)p;
}

/* デモ用：範囲外アクセスを abort() で防ぐ関数 */
static int safe_array_get(const int *arr, int idx, int size)
{
    if (idx < 0 || idx >= size) {
        abort();
    }
    return arr[idx];
}

class DeathTest : public ::testing::Test {};

/* assert() で落ちることを検証 */
TEST_F(DeathTest, NullAssertKillsProcess)
{
    /* "Assertion" が stderr に出てプロセスが死ぬことを期待 */
    EXPECT_DEATH(must_not_be_null(nullptr), "Assertion");
}

/* abort() で落ちることを検証 */
TEST_F(DeathTest, OutOfBoundsAborts)
{
    int arr[] = {1, 2, 3};
    /* "" = stderr の内容は問わず、異常終了すれば OK */
    EXPECT_DEATH(safe_array_get(arr, 5, 3), "");
}

/* 正常系：落ちないことも確認できる */
TEST_F(DeathTest, ValidAccessDoesNotDie)
{
    int arr[] = {10, 20, 30};
    /* EXPECT_DEATH の逆 — 落ちたら FAIL */
    EXPECT_EQ(20, safe_array_get(arr, 1, 3));
}

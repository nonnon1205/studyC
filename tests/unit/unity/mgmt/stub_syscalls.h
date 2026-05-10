/**
 * @file stub_syscalls.h
 * @brief システムコールのスタブ制御（Unity テスト用）
 *
 * 各テストの前に stubs_reset() で初期化し、
 * 戻り値は g_stubs.xxx_ret で設定、呼び出し回数は g_stubs.xxx_calls で確認する。
 */
#ifndef STUB_SYSCALLS_H
#define STUB_SYSCALLS_H

#include <sys/types.h>
#include "mgmt_protocol.h"

typedef struct
{
    /* 戻り値設定（テスト前にセット） */
    int     socket_ret;
    int     bind_ret;
    ssize_t sendto_ret;
    int     poll_ret;
    ssize_t recv_ret;   /* sizeof(MgmtCommandResponse) を返すと成功扱い */
    int     close_ret;
    int     unlink_ret;

    /* 呼び出し回数（テスト後に確認） */
    int socket_calls;
    int bind_calls;
    int sendto_calls;
    int poll_calls;
    int recv_calls;
    int close_calls;
    int unlink_calls;
} SyscallStubs;

extern SyscallStubs g_stubs;

void stubs_reset(void);

#endif /* STUB_SYSCALLS_H */

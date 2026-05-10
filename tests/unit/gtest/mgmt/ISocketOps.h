/**
 * @file ISocketOps.h
 * @brief ソケット操作の抽象インターフェース（テスト用モック差し替えのため）
 */
#pragma once

#include <sys/types.h>
#include "mgmt_protocol.h"

class ISocketOps
{
public:
    virtual ~ISocketOps() = default;

    virtual int  createSocket()                                              = 0;
    virtual int  bindClient(int fd, const char *tmp_path)                   = 0;
    virtual int  sendRequest(int fd, const char *server_path,
                             const MgmtCommandRequest *req)                 = 0;
    virtual int  waitReadable(int fd, int timeout_ms)                       = 0;
    virtual int  recvResponse(int fd, MgmtCommandResponse *resp)            = 0;
    virtual void cleanup(int fd, const char *tmp_path)                      = 0;
};

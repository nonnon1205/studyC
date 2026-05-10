/**
 * @file MgmtSender.h
 * @brief mgmt_send_command を ISocketOps に依存するよう C++ でラップしたクラス
 *        本番は RealSocketOps を注入、テストは MockSocketOps を注入する
 */
#pragma once

#include "ISocketOps.h"

class MgmtSender
{
    ISocketOps *ops;

public:
    explicit MgmtSender(ISocketOps *o) : ops(o) {}

    int sendCommand(const char *server_path, const MgmtCommandRequest *req,
                    MgmtCommandResponse *resp, int timeout_ms)
    {
        if (!server_path || !req || !resp)
            return -1;

        int fd = ops->createSocket();
        if (fd < 0)
            return -1;

        char tmp_path[64];
        snprintf(tmp_path, sizeof(tmp_path), "/tmp/studyc_test_%d.sock",
                 (int)getpid());

        int ret = -1;

        if (ops->bindClient(fd, tmp_path) < 0)
            goto done;

        if (ops->sendRequest(fd, server_path, req) < 0)
            goto done;

        if (ops->waitReadable(fd, timeout_ms) <= 0)
            goto done;

        if (ops->recvResponse(fd, resp) == 0)
            ret = 0;

    done:
        ops->cleanup(fd, tmp_path);
        return ret;
    }
};

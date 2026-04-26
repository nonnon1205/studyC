#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <signal.h>
#include <unistd.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include "udp_common.h"
#define MODULE_NAME "IPC"
#include "debug_log.h"



// --- 2. IPCスレッド ---
void* ipc_worker(void* arg) {
    AppContext *ctx = (AppContext *)arg;
    struct msg_buffer message;

    ctx->msqid = msgget(MSG_KEY, 0666 | IPC_CREAT);
    printf("[IPC] 待機中...\n");

    while (1) {
        if (msgrcv(ctx->msqid, &message, sizeof(message.msg_text), 1, 0) != -1) {
            DBG("IPC受信: \"%s\"", message.msg_text);
            if (strncmp(message.msg_text, "EXIT", 4) == 0) {
                printf("[IPC] 終了メッセージを受信。ループを抜けます。\n");
                break;
            }
        }
    }
    return NULL;
}

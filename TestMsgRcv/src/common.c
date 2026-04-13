#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include <sys/msg.h>
#include "msg_common.h"

// --- ヘルパー：内部キューへ送信 ---
void send_to_main(int msqid, EventType type, const char* text, int sig) {
    InternalMsg msg;
    memset(&msg, 0, sizeof(msg));
    msg.mtype = MSG_TYPE;
    msg.event = type;

    if (text) {
        strncpy(msg.data.udp_payload, text, sizeof(msg.data.udp_payload) - 1);
        msg.data.udp_payload[sizeof(msg.data.udp_payload) - 1] = '\0';
    }
    if (sig) {
        msg.data.sig_num = sig;
    }

    if (msgsnd(msqid, &msg, sizeof(InternalMsg) - sizeof(long), 0) == -1) {
        perror("msgsnd");
    }
}

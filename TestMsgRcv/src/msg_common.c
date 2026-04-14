#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <pthread.h>
#include <unistd.h>
#include <sys/msg.h>
#include "log_wrapper.h"
#include "msg_common.h"

// --- ヘルパー：内部メッセージ構築 ---
InternalMsg build_internal_msg(EventType type, const char* text, int sig) {
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
    return msg;
}

// --- ヘルパー：内部キューへ送信 ---
int send_to_main(int msqid, const InternalMsg* msg) {
    if (msg == NULL) {
        return -1;
    }
    if (msgsnd(msqid, msg, sizeof(InternalMsg) - sizeof(long), 0) == -1) {
        log_err("[Common] msgsnd: %s", strerror(errno));
        return -1;
    }
    return 0;
}

int send_quit_event(int msqid) {
    InternalMsg msg = build_internal_msg(EV_QUIT, NULL, 0);
    return send_to_main(msqid, &msg);
}

int send_udp_event(int msqid, const char* payload) {
    InternalMsg msg = build_internal_msg(EV_UDP, payload, 0);
    return send_to_main(msqid, &msg);
}

int send_signal_event(int msqid, int sig) {
    InternalMsg msg = build_internal_msg(EV_SIGNAL, NULL, sig);
    return send_to_main(msqid, &msg);
}

int send_ipc_event(int msqid, const char* payload) {
    InternalMsg msg = build_internal_msg(EV_IPC, payload, 0);
    return send_to_main(msqid, &msg);
}
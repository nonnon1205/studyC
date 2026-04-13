#define _POSIX_C_SOURCE 200809L
#include <assert.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <unistd.h>
#include "../src/msg_common.h"

int main(void) {
    InternalMsg msg;

    msg = build_internal_msg(EV_UDP, "hello", 0);
    assert(msg.mtype == MSG_TYPE);
    assert(msg.event == EV_UDP);
    assert(strcmp(msg.data.udp_payload, "hello") == 0);

    msg = build_internal_msg(EV_SIGNAL, NULL, SIGINT);
    assert(msg.event == EV_SIGNAL);
    assert(msg.data.sig_num == SIGINT);

    char long_text[512];
    memset(long_text, 'x', sizeof(long_text) - 1);
    long_text[sizeof(long_text) - 1] = '\0';

    msg = build_internal_msg(EV_UDP, long_text, 0);
    assert(msg.data.udp_payload[sizeof(msg.data.udp_payload) - 1] == '\0');
    assert(strncmp(msg.data.udp_payload, long_text, sizeof(msg.data.udp_payload) - 1) == 0);

    int msqid = msgget(IPC_PRIVATE, IPC_CREAT | 0600);
    assert(msqid != -1);

    assert(send_quit_event(msqid) == 0);
    InternalMsg recv_msg;
    ssize_t rc = msgrcv(msqid, &recv_msg, sizeof(recv_msg) - sizeof(long), MSG_TYPE, 0);
    assert(rc != -1);
    assert(recv_msg.event == EV_QUIT);

    assert(send_udp_event(msqid, "abc") == 0);
    rc = msgrcv(msqid, &recv_msg, sizeof(recv_msg) - sizeof(long), MSG_TYPE, 0);
    assert(rc != -1);
    assert(recv_msg.event == EV_UDP);
    assert(strcmp(recv_msg.data.udp_payload, "abc") == 0);

    assert(send_signal_event(msqid, SIGTERM) == 0);
    rc = msgrcv(msqid, &recv_msg, sizeof(recv_msg) - sizeof(long), MSG_TYPE, 0);
    assert(rc != -1);
    assert(recv_msg.event == EV_SIGNAL);
    assert(recv_msg.data.sig_num == SIGTERM);

    assert(msgctl(msqid, IPC_RMID, NULL) == 0);

    printf("ALL TESTS PASSED\n");
    return 0;
}

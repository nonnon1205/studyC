#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <pthread.h>
#include <unistd.h>

void* signal_handler_thread(void* arg) {
    sigset_t *set = (sigset_t *)arg;
    int sig;

    printf("[Signal Thread] 属性設定済みスレッドで待機中...\n");

    while (1) {
        sigwait(set, &sig);
        if (sig == SIGINT) {
            printf("\n[Signal Thread] SIGINTを受信。終了します。\n");
            exit(0);
        }
    }
    return NULL;
}


#ifndef ROUTER_WORKER_H
#define ROUTER_WORKER_H

#define _POSIX_C_SOURCE 200809L

#include "shared_ipc.h"
#include "shm_api.h"

typedef struct {
    int main_msqid;
    int ipc_msqid;
    ShmHandle shm_handle;
} RouterContext;

void* router_worker(void* arg);

#endif // ROUTER_WORKER_H

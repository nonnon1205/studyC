#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <errno.h>
#include <pthread.h>
#include <time.h>
#include <signal.h>
#include <stdatomic.h>
#include "mgmt_socket.h"

/* ============================================================================
 * Management Socket State
 * ============================================================================
 */

struct MgmtSocket {
    int socket_fd;                  /* UNIX domain socket FD */
    struct sockaddr_un sock_addr;   /* Socket address structure */
    uint64_t total_requests;        /* Statistics: total commands received */
    uint64_t total_responses;       /* Statistics: total responses sent */
    uint64_t failed_requests;       /* Statistics: request failures */
    pthread_mutex_t stats_lock;     /* Protects statistics */
};

/* ============================================================================
 * Socket Creation and Destruction
 * ============================================================================
 */

MgmtSocketHandle mgmt_socket_create(void)
{
    struct MgmtSocket* ms = (struct MgmtSocket*)malloc(sizeof(struct MgmtSocket));
    if (!ms) return NULL;

    memset(ms, 0, sizeof(struct MgmtSocket));

    /* Create UNIX domain datagram socket */
    ms->socket_fd = socket(AF_UNIX, SOCK_DGRAM, 0);
    if (ms->socket_fd < 0) {
        perror("mgmt_socket_create: socket");
        free(ms);
        return NULL;
    }

    /* Prepare socket address */
    memset(&ms->sock_addr, 0, sizeof(struct sockaddr_un));
    ms->sock_addr.sun_family = AF_UNIX;
    strncpy(ms->sock_addr.sun_path, MGMT_SOCKET_PATH,
            sizeof(ms->sock_addr.sun_path) - 1);

    /* Remove stale socket file if it exists */
    unlink(MGMT_SOCKET_PATH);

    /* Bind socket */
    if (bind(ms->socket_fd, (struct sockaddr*)&ms->sock_addr,
             sizeof(struct sockaddr_un)) < 0) {
        perror("mgmt_socket_create: bind");
        close(ms->socket_fd);
        free(ms);
        return NULL;
    }

    /* Initialize statistics */
    pthread_mutex_init(&ms->stats_lock, NULL);

    printf("[Mgmt] Socket created at %s\n", MGMT_SOCKET_PATH);

    return ms;
}

void mgmt_socket_destroy(MgmtSocketHandle handle)
{
    if (!handle) return;

    if (handle->socket_fd >= 0) {
        close(handle->socket_fd);
    }

    unlink(MGMT_SOCKET_PATH);

    pthread_mutex_destroy(&handle->stats_lock);
    free(handle);

    printf("[Mgmt] Socket destroyed\n");
}

/* ============================================================================
 * Command Processing
 * ============================================================================
 */

int mgmt_socket_process_one(MgmtSocketHandle handle, int timeout_ms)
{
    if (!handle || handle->socket_fd < 0) return -1;

    /* Setup timeout if specified */
    struct timeval tv = {0};
    if (timeout_ms > 0) {
        tv.tv_sec = timeout_ms / 1000;
        tv.tv_usec = (timeout_ms % 1000) * 1000;

        if (setsockopt(handle->socket_fd, SOL_SOCKET, SO_RCVTIMEO, &tv,
                       sizeof(tv)) < 0) {
            perror("setsockopt");
            return -1;
        }
    }

    /* Receive request */
    MgmtCommandRequest req;
    struct sockaddr_un client_addr;
    socklen_t client_len = sizeof(struct sockaddr_un);

    int bytes_rcvd = recvfrom(handle->socket_fd, &req, sizeof(req), 0,
                              (struct sockaddr*)&client_addr, &client_len);

    if (bytes_rcvd < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            return -1;  /* Timeout */
        }
        perror("recvfrom");
        return -1;
    }

    /* Update statistics */
    pthread_mutex_lock(&handle->stats_lock);
    handle->total_requests++;
    pthread_mutex_unlock(&handle->stats_lock);

    /* Dispatch command */
    MgmtCommandResponse resp;
    memset(&resp, 0, sizeof(resp));

    int dispatch_result = handler_dispatch(&req, &resp);

    if (dispatch_result != 0) {
        pthread_mutex_lock(&handle->stats_lock);
        handle->failed_requests++;
        pthread_mutex_unlock(&handle->stats_lock);
    }

    /* Send response back */
    if (sendto(handle->socket_fd, &resp, sizeof(resp), 0,
               (struct sockaddr*)&client_addr, client_len) < 0) {
        perror("sendto");
        return -1;
    }

    /* Update statistics */
    pthread_mutex_lock(&handle->stats_lock);
    handle->total_responses++;
    pthread_mutex_unlock(&handle->stats_lock);

    /* Log command */
    printf("[Mgmt] cmd=%s result=%s\n",
           mgmt_command_str(req.cmd_type),
           mgmt_result_str(resp.result_code));

    return 0;
}

int mgmt_socket_process_all(MgmtSocketHandle handle)
{
    if (!handle || handle->socket_fd < 0) return -1;

    int processed = 0;

    while (1) {
        /* Set non-blocking mode temporarily */
        struct timeval tv = {0, 1000};  /* 1ms timeout */
        if (setsockopt(handle->socket_fd, SOL_SOCKET, SO_RCVTIMEO, &tv,
                       sizeof(tv)) < 0) {
            break;
        }

        int result = mgmt_socket_process_one(handle, 1);
        if (result < 0) break;  /* No more commands or error */

        processed++;
    }

    return processed;
}

/* ============================================================================
 * Worker Thread
 * ============================================================================
 */

void* mgmt_worker(void* arg)
{
    MgmtSocketHandle sock = (MgmtSocketHandle)arg;

    if (!sock) {
        fprintf(stderr, "[Mgmt Worker] Invalid socket handle\n");
        return NULL;
    }

    printf("[Mgmt Worker] Started, listening on %s\n", MGMT_SOCKET_PATH);

    /* Signal handling: ignore SIGPIPE */
    signal(SIGPIPE, SIG_IGN);

    /* Main loop: process commands indefinitely */
    while (1) {
        /* Block until command received (default: infinite timeout) */
        int result = mgmt_socket_process_one(sock, -1);

        if (result < 0 && errno != EAGAIN) {
            perror("[Mgmt Worker] process_one failed");
            break;
        }
    }

    printf("[Mgmt Worker] Exiting\n");

    return NULL;
}

/* ============================================================================
 * Statistics
 * ============================================================================
 */

int mgmt_socket_stats(MgmtSocketHandle handle,
                      uint64_t* total_requests,
                      uint64_t* total_responses,
                      uint64_t* failed_requests)
{
    if (!handle) return -1;

    pthread_mutex_lock(&handle->stats_lock);

    if (total_requests) *total_requests = handle->total_requests;
    if (total_responses) *total_responses = handle->total_responses;
    if (failed_requests) *failed_requests = handle->failed_requests;

    pthread_mutex_unlock(&handle->stats_lock);

    return 0;
}

int mgmt_socket_stats_reset(MgmtSocketHandle handle)
{
    if (!handle) return -1;

    pthread_mutex_lock(&handle->stats_lock);

    handle->total_requests = 0;
    handle->total_responses = 0;
    handle->failed_requests = 0;

    pthread_mutex_unlock(&handle->stats_lock);

    return 0;
}

/* ============================================================================
 * Configuration
 * ============================================================================
 */

int mgmt_socket_set_backlog(MgmtSocketHandle handle, int depth)
{
    if (!handle || handle->socket_fd < 0) return -1;

    /* Note: UNIX domain sockets don't support listen() directly for DGRAM
     * This is a placeholder for future extension */

    return 0;
}

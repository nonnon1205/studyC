#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <stdatomic.h>
#include "mgmt_socket.h"

/* ============================================================================
 * Management Worker Thread
 *
 * Dedicated thread that:
 * 1. Creates and binds the management UNIX domain socket
 * 2. Listens for incoming management commands
 * 3. Dispatches commands to registered handlers
 * 4. Sends responses back to clients
 *
 * Runs indefinitely until shutdown signal
 * ============================================================================
 */

/* Global shutdown flag (set by main thread on shutdown) */
extern atomic_bool g_keep_running;

void* mgmt_worker(void* arg)
{
    (void)arg;

    printf("[Mgmt Worker] Starting...\n");

    /* Create management socket */
    MgmtSocketHandle sock = mgmt_socket_create();
    if (!sock) {
        fprintf(stderr, "[Mgmt Worker] Failed to create management socket\n");
        return NULL;
    }

    printf("[Mgmt Worker] Socket created successfully\n");

    /* Main loop: process commands until shutdown */
    while (atomic_load_explicit(&g_keep_running, memory_order_acquire)) {
        /* Process one command with 1-second timeout */
        int result = mgmt_socket_process_one(sock, 1000);

        if (result < 0) {
            /* Timeout or error - check shutdown flag and continue */
            continue;
        }
    }

    printf("[Mgmt Worker] Shutdown requested, cleaning up...\n");

    /* Cleanup */
    mgmt_socket_destroy(sock);

    printf("[Mgmt Worker] Exited\n");

    return NULL;
}

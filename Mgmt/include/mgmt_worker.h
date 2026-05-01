#ifndef MGMT_WORKER_H
#define MGMT_WORKER_H

#include <stdatomic.h>

/* ============================================================================
 * Management Worker Thread
 *
 * Dedicated thread that binds the management socket and processes commands.
 * Used by Router and Viewer (multi-threaded modules).
 * Collector uses poll-based integration instead (see mgmt_socket_get_fd).
 * ============================================================================
 */

typedef struct {
    const char*  socket_path;   /* UNIX domain socket path (use MGMT_SOCKET_PATH_*) */
    atomic_bool* keep_running;  /* Points to the module's shutdown flag */
} MgmtWorkerArg;

/**
 * Management worker thread entry point.
 *
 * @param arg  Pointer to MgmtWorkerArg (must remain valid for thread lifetime)
 * @return     NULL
 *
 * Example:
 *   static MgmtWorkerArg mgmt_arg = {
 *       .socket_path  = MGMT_SOCKET_PATH_ROUTER,
 *       .keep_running = &g_keep_running,
 *   };
 *   pthread_create(&t_mgmt, NULL, mgmt_worker, &mgmt_arg);
 */
void* mgmt_worker(void* arg);

#endif /* MGMT_WORKER_H */

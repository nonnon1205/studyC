#ifndef MGMT_SOCKET_H
#define MGMT_SOCKET_H

#include <stdint.h>
#include "mgmt_protocol.h"
#include "event_handler.h"

/* ============================================================================
 * Management Socket Server
 *
 * UNIX domain datagram socket listener for management commands
 * Receives requests, dispatches to handlers, sends responses
 * Runs in a dedicated thread (mgmt_worker)
 * ============================================================================
 */

typedef struct MgmtSocket* MgmtSocketHandle;

/* ============================================================================
 * Initialization and Shutdown
 * ============================================================================
 */

/**
 * Create and bind the management socket
 *
 * Creates a UNIX domain datagram socket at MGMT_SOCKET_PATH
 * Initializes the socket for receiving commands
 *
 * @return  Socket handle on success, NULL on failure
 */
MgmtSocketHandle mgmt_socket_create(void);

/**
 * Destroy the management socket
 *
 * Closes the socket and removes the socket file from filesystem
 *
 * @param handle    Socket handle
 */
void mgmt_socket_destroy(MgmtSocketHandle handle);

/**
 * Wait for and process a single command
 *
 * Blocking call that waits for next incoming management command.
 * Dispatches to handler and sends response back to client.
 *
 * @param handle        Socket handle
 * @param timeout_ms    Timeout in milliseconds (0 = no timeout)
 * @return              0 on success, -1 on timeout/error
 *
 * Note: Intended to be called in a loop from a dedicated thread
 */
int mgmt_socket_process_one(MgmtSocketHandle handle, int timeout_ms);

/**
 * Process all pending commands
 *
 * Non-blocking: processes all available commands, then returns
 *
 * @param handle    Socket handle
 * @return          Number of commands processed, -1 on error
 */
int mgmt_socket_process_all(MgmtSocketHandle handle);

/* ============================================================================
 * Thread Entry Point
 * ============================================================================
 */

/**
 * Management worker thread entry point
 *
 * Call this with pthread_create to spawn the management socket server thread
 *
 * @param arg   Should be a pointer to MgmtSocketHandle
 * @return      NULL on exit
 *
 * Example:
 *   MgmtSocketHandle sock = mgmt_socket_create();
 *   pthread_t t_mgmt;
 *   pthread_create(&t_mgmt, NULL, mgmt_worker, sock);
 */
void* mgmt_worker(void* arg);

/* ============================================================================
 * Statistics and Monitoring
 * ============================================================================
 */

/**
 * Get socket statistics
 *
 * @param handle            Socket handle
 * @param total_requests    Output: total requests received
 * @param total_responses   Output: total responses sent
 * @param failed_requests   Output: failed request processing
 * @return                  0 on success
 */
int mgmt_socket_stats(MgmtSocketHandle handle,
                      uint64_t* total_requests,
                      uint64_t* total_responses,
                      uint64_t* failed_requests);

/**
 * Reset socket statistics
 *
 * @param handle    Socket handle
 * @return          0 on success
 */
int mgmt_socket_stats_reset(MgmtSocketHandle handle);

/* ============================================================================
 * Configuration
 * ============================================================================
 */

/**
 * Set the maximum queue depth for pending commands
 *
 * @param handle    Socket handle
 * @param depth     Queue depth (default: MGMT_SOCKET_BACKLOG)
 * @return          0 on success
 */
int mgmt_socket_set_backlog(MgmtSocketHandle handle, int depth);

#endif /* MGMT_SOCKET_H */

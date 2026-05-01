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
 * ============================================================================
 */

typedef struct MgmtSocket* MgmtSocketHandle;

/* ============================================================================
 * Initialization and Shutdown
 * ============================================================================
 */

/**
 * Create and bind the management socket at the given path.
 *
 * @param socket_path  UNIX domain socket path (use MGMT_SOCKET_PATH_* from mgmt_paths.h)
 * @return  Socket handle on success, NULL on failure
 */
MgmtSocketHandle mgmt_socket_create(const char* socket_path);

/**
 * Destroy the management socket
 *
 * Closes the socket and removes the socket file from filesystem.
 *
 * @param handle    Socket handle
 */
void mgmt_socket_destroy(MgmtSocketHandle handle);

/**
 * Return the underlying file descriptor (for use with select/poll).
 *
 * @param handle    Socket handle
 * @return          File descriptor, or -1 on error
 */
int mgmt_socket_get_fd(MgmtSocketHandle handle);

/**
 * Wait for and process a single command
 *
 * Blocking call that waits for next incoming management command.
 * Dispatches to handler and sends response back to client.
 *
 * @param handle        Socket handle
 * @param timeout_ms    Timeout in milliseconds (0 = no timeout, -1 = block)
 * @return              0 on success, -1 on timeout/error
 */
int mgmt_socket_process_one(MgmtSocketHandle handle, int timeout_ms);

/**
 * Process all pending commands (non-blocking)
 *
 * @param handle    Socket handle
 * @return          Number of commands processed, -1 on error
 */
int mgmt_socket_process_all(MgmtSocketHandle handle);

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

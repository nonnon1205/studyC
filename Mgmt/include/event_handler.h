#ifndef EVENT_HANDLER_H
#define EVENT_HANDLER_H

#include <stdint.h>
#include <pthread.h>
#include "mgmt_protocol.h"

/* ============================================================================
 * Event Handler Registry
 *
 * Dynamic handler registration and dispatch system for management commands
 * Thread-safe registry supporting multiple handlers per module
 * ============================================================================
 */

/**
 * Event handler function signature
 *
 * @param req       Incoming management command request
 * @param resp      Response structure to be filled by handler
 * @param context   Module-specific context (opaque pointer)
 * @return          0 on success, negative on error
 */
typedef int (*MgmtEventHandler)(const MgmtCommandRequest *req,
								MgmtCommandResponse *resp, void *context);

/**
 * Handler registry entry
 */
typedef struct
{
	uint8_t cmd_type; /* Command type this handler serves */
	char module_name[MGMT_MODULE_NAME_SIZE]; /* Module name */
	MgmtEventHandler handler;				 /* Handler function pointer */
	void *context;			   /* Module context passed to handler */
	uint32_t invocation_count; /* Statistics: call count */
	uint64_t total_latency_us; /* Statistics: cumulative latency */
} EventHandlerEntry;

#define MAX_EVENT_HANDLERS 64

/* ============================================================================
 * Global Registry State
 * ============================================================================
 */

typedef struct
{
	EventHandlerEntry entries[MAX_EVENT_HANDLERS];
	int entry_count;
	pthread_mutex_t lock;	   /* Protects concurrent access */
	uint32_t total_dispatches; /* Total command dispatches */
	uint32_t total_failures;   /* Total dispatch failures */
} EventHandlerRegistry;

/**
 * Initialize the global handler registry
 * Must be called once at program startup before any registration
 * @return  0 on success, -1 on failure
 */
int handler_registry_init(void);

/**
 * Cleanup and destroy the handler registry
 * Should be called during shutdown
 */
void handler_registry_destroy(void);

/**
 * Register an event handler for a specific command in a module
 *
 * Multiple handlers can be registered for the same command
 * (though typically one per module). Most recent registration wins.
 *
 * @param module_name   Name of the module (e.g., "router", "pollio")
 * @param cmd_type      Command type to handle (MgmtCommandType)
 * @param handler       Handler function to invoke
 * @param context       Module-specific context (passed to handler)
 * @return              0 on success, -1 if registry full, -2 if already
 * registered
 */
int handler_register(const char *module_name, uint8_t cmd_type,
					 MgmtEventHandler handler, void *context);

/**
 * Unregister a handler for a specific module/command combination
 *
 * @param module_name   Module name
 * @param cmd_type      Command type
 * @return              0 on success, -1 if not found
 */
int handler_unregister(const char *module_name, uint8_t cmd_type);

/**
 * Dispatch a management command to the appropriate handler
 *
 * This is the main entry point for command processing.
 * Performs lookup in registry and invokes the matching handler.
 *
 * @param req           Incoming command request
 * @param resp          Response structure filled by handler
 * @return              0 on success, negative on error
 *                      Specific codes:
 *                      -1: Handler not found
 *                      -2: Handler invocation failed
 *                      -3: Internal error
 */
int handler_dispatch(const MgmtCommandRequest *req, MgmtCommandResponse *resp);

/**
 * List all registered handlers (for debugging/view dashboard)
 *
 * @param buf           Output buffer for handler list (JSON format)
 * @param buf_size      Size of output buffer
 * @return              Number of handlers listed, or negative on error
 */
int handler_list(char *buf, size_t buf_size);

/**
 * Get registry statistics
 *
 * @param total_cmds    Output: total commands dispatched
 * @param total_fails   Output: total failed dispatches
 * @return              0 on success
 */
int handler_stats(uint32_t *total_cmds, uint32_t *total_fails);

/**
 * Reset registry statistics
 *
 * @return              0 on success
 */
int handler_stats_reset(void);

#endif /* EVENT_HANDLER_H */

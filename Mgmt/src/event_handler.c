#include <string.h>
#include <stdio.h>
#include <errno.h>
#include "event_handler.h"
#include "time.h"

/* Global registry instance */
static EventHandlerRegistry g_registry = {
	.entry_count = 0, .total_dispatches = 0, .total_failures = 0};

static int g_registry_initialized = 0;

/**
 * Initialize the global handler registry
 */
int handler_registry_init(void)
{
	if (g_registry_initialized)
	{
		return 0; /* Already initialized */
	}

	if (pthread_mutex_init(&g_registry.lock, NULL) != 0)
	{
		return -1;
	}

	g_registry.entry_count = 0;
	g_registry.total_dispatches = 0;
	g_registry.total_failures = 0;
	g_registry_initialized = 1;

	return 0;
}

/**
 * Cleanup and destroy the handler registry
 */
void handler_registry_destroy(void)
{
	if (!g_registry_initialized)
		return;

	pthread_mutex_lock(&g_registry.lock);
	g_registry.entry_count = 0;
	g_registry.total_dispatches = 0;
	g_registry.total_failures = 0;
	pthread_mutex_unlock(&g_registry.lock);

	pthread_mutex_destroy(&g_registry.lock);
	g_registry_initialized = 0;
}

/**
 * Register an event handler
 */
int handler_register(const char *module_name, uint8_t cmd_type,
					 MgmtEventHandler handler, void *context)
{
	if (!handler || !module_name || cmd_type >= MGMT_CMD_MAX)
	{
		return -1;
	}

	pthread_mutex_lock(&g_registry.lock);

	/* Check if handler already registered for this module/command combo */
	for (int i = 0; i < g_registry.entry_count; i++)
	{
		EventHandlerEntry *entry = &g_registry.entries[i];
		if (entry->cmd_type == cmd_type &&
			strcmp(entry->module_name, module_name) == 0)
		{
			/* Update existing handler */
			entry->handler = handler;
			entry->context = context;
			entry->invocation_count = 0;
			entry->total_latency_us = 0;
			pthread_mutex_unlock(&g_registry.lock);
			return 0;
		}
	}

	/* Add new handler if registry not full */
	if (g_registry.entry_count >= MAX_EVENT_HANDLERS)
	{
		pthread_mutex_unlock(&g_registry.lock);
		return -1; /* Registry full */
	}

	EventHandlerEntry *new_entry = &g_registry.entries[g_registry.entry_count];
	new_entry->cmd_type = cmd_type;
	new_entry->handler = handler;
	new_entry->context = context;
	new_entry->invocation_count = 0;
	new_entry->total_latency_us = 0;
	strncpy(new_entry->module_name, module_name, MGMT_MODULE_NAME_SIZE - 1);
	new_entry->module_name[MGMT_MODULE_NAME_SIZE - 1] = '\0';

	g_registry.entry_count++;

	pthread_mutex_unlock(&g_registry.lock);
	return 0;
}

/**
 * Unregister a handler
 */
int handler_unregister(const char *module_name, uint8_t cmd_type)
{
	if (!module_name)
		return -1;

	pthread_mutex_lock(&g_registry.lock);

	for (int i = 0; i < g_registry.entry_count; i++)
	{
		EventHandlerEntry *entry = &g_registry.entries[i];
		if (entry->cmd_type == cmd_type &&
			strcmp(entry->module_name, module_name) == 0)
		{

			/* Remove by shifting remaining entries */
			for (int j = i; j < g_registry.entry_count - 1; j++)
			{
				g_registry.entries[j] = g_registry.entries[j + 1];
			}
			g_registry.entry_count--;

			pthread_mutex_unlock(&g_registry.lock);
			return 0;
		}
	}

	pthread_mutex_unlock(&g_registry.lock);
	return -1; /* Not found */
}

/**
 * Dispatch a command to the appropriate handler
 */
int handler_dispatch(const MgmtCommandRequest *req, MgmtCommandResponse *resp)
{
	if (!req || !resp)
		return -3;

	/* Validate request first */
	if (!mgmt_request_validate(req))
	{
		resp->result_code = MGMT_RESULT_INVALID_CMD;
		return -3;
	}

	pthread_mutex_lock(&g_registry.lock);

	/* Find matching handler */
	EventHandlerEntry *matching_entry = NULL;
	for (int i = 0; i < g_registry.entry_count; i++)
	{
		EventHandlerEntry *entry = &g_registry.entries[i];
		if (entry->cmd_type == req->cmd_type &&
			strcmp(entry->module_name, req->target_module) == 0)
		{
			matching_entry = entry;
			break;
		}
	}

	if (!matching_entry)
	{
		pthread_mutex_unlock(&g_registry.lock);
		resp->result_code = MGMT_RESULT_MODULE_NOT_FOUND;
		return -1;
	}

	/* Prepare response */
	resp->request_id = req->request_id;
	resp->request_timestamp = req->timestamp;
	strncpy(resp->source_module, req->target_module, MGMT_MODULE_NAME_SIZE - 1);

	pthread_mutex_unlock(&g_registry.lock);

	/* Invoke handler (outside lock to avoid deadlock) */
	int result = matching_entry->handler(req, resp, matching_entry->context);

	/* Update statistics */
	pthread_mutex_lock(&g_registry.lock);

	g_registry.total_dispatches++;
	matching_entry->invocation_count++;

	if (result != 0)
	{
		g_registry.total_failures++;
		resp->result_code = MGMT_RESULT_HANDLER_FAILED;
	}
	else if (resp->result_code == 0)
	{
		resp->result_code = MGMT_RESULT_OK;
	}

	/* Record latency */
	uint64_t latency = mgmt_response_latency_us(resp);
	matching_entry->total_latency_us += latency;

	pthread_mutex_unlock(&g_registry.lock);

	return result;
}

/**
 * List all registered handlers
 */
int handler_list(char *buf, size_t buf_size)
{
	if (!buf || buf_size == 0)
		return -1;

	pthread_mutex_lock(&g_registry.lock);

	int offset = 0;
	offset += snprintf(buf + offset, buf_size - offset, "{\"handlers\": [");

	for (int i = 0; i < g_registry.entry_count; i++)
	{
		EventHandlerEntry *entry = &g_registry.entries[i];

		if (i > 0)
		{
			offset += snprintf(buf + offset, buf_size - offset, ",");
		}

		uint64_t avg_latency =
			(entry->invocation_count > 0)
				? entry->total_latency_us / entry->invocation_count
				: 0;

		offset +=
			snprintf(buf + offset, buf_size - offset,
					 "{\"module\":\"%s\",\"command\":\"%s\",\"invocations\":%u,"
					 "\"avg_latency_us\":%lu}",
					 entry->module_name, mgmt_command_str(entry->cmd_type),
					 entry->invocation_count, avg_latency);

		if (offset >= (int)buf_size - 1)
		{
			pthread_mutex_unlock(&g_registry.lock);
			return -1; /* Buffer overflow */
		}
	}

	snprintf(buf + offset, buf_size - offset,
			 "], "
			 "\"total_dispatches\": %u, \"total_failures\": %u}",
			 g_registry.total_dispatches, g_registry.total_failures);

	pthread_mutex_unlock(&g_registry.lock);

	return g_registry.entry_count;
}

/**
 * Get registry statistics
 */
int handler_stats(uint32_t *total_cmds, uint32_t *total_fails)
{
	if (!total_cmds || !total_fails)
		return -1;

	pthread_mutex_lock(&g_registry.lock);
	*total_cmds = g_registry.total_dispatches;
	*total_fails = g_registry.total_failures;
	pthread_mutex_unlock(&g_registry.lock);

	return 0;
}

/**
 * Reset statistics
 */
int handler_stats_reset(void)
{
	pthread_mutex_lock(&g_registry.lock);
	g_registry.total_dispatches = 0;
	g_registry.total_failures = 0;

	for (int i = 0; i < g_registry.entry_count; i++)
	{
		g_registry.entries[i].invocation_count = 0;
		g_registry.entries[i].total_latency_us = 0;
	}

	pthread_mutex_unlock(&g_registry.lock);

	return 0;
}

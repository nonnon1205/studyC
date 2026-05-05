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
#include "unified_logger.h"

#ifndef UNIX_PATH_MAX
#define UNIX_PATH_MAX 108
#endif

/* ============================================================================
 * Management Socket State
 * ============================================================================
 */

struct MgmtSocket
{
	int socket_fd;					 /* UNIX domain socket FD */
	struct sockaddr_un sock_addr;	 /* Socket address structure */
	char socket_path[UNIX_PATH_MAX]; /* Stored path for unlink on destroy */
	uint64_t total_requests;		 /* Statistics: total commands received */
	uint64_t total_responses;		 /* Statistics: total responses sent */
	uint64_t failed_requests;		 /* Statistics: request failures */
	pthread_mutex_t stats_lock;		 /* Protects statistics */
};

/* ============================================================================
 * Socket Creation and Destruction
 * ============================================================================
 */

MgmtSocketHandle mgmt_socket_create(const char *socket_path)
{
	if (!socket_path)
		return NULL;

	struct MgmtSocket *ms =
		(struct MgmtSocket *)malloc(sizeof(struct MgmtSocket));
	if (!ms)
		return NULL;

	memset(ms, 0, sizeof(struct MgmtSocket));

	strncpy(ms->socket_path, socket_path, sizeof(ms->socket_path) - 1);

	/* Create UNIX domain datagram socket */
	ms->socket_fd = socket(AF_UNIX, SOCK_DGRAM, 0);
	if (ms->socket_fd < 0)
	{
		GLOG_ERR("mgmt_socket_create: socket: %s", safe_strerror(errno));
		free(ms);
		return NULL;
	}

	/* Prepare socket address */
	memset(&ms->sock_addr, 0, sizeof(struct sockaddr_un));
	ms->sock_addr.sun_family = AF_UNIX;
	strncpy(ms->sock_addr.sun_path, socket_path,
			sizeof(ms->sock_addr.sun_path) - 1);

	/* Remove stale socket file if it exists */
	unlink(socket_path);

	/* Bind socket */
	if (bind(ms->socket_fd, (struct sockaddr *)&ms->sock_addr,
			 sizeof(struct sockaddr_un)) < 0)
	{
		GLOG_ERR("mgmt_socket_create: bind: %s", safe_strerror(errno));
		close(ms->socket_fd);
		free(ms);
		return NULL;
	}

	/* Initialize statistics */
	pthread_mutex_init(&ms->stats_lock, NULL);

	GLOG_INFO("[Mgmt] Socket created at %s", socket_path);

	return ms;
}

void mgmt_socket_destroy(MgmtSocketHandle handle)
{
	if (!handle)
		return;

	if (handle->socket_fd >= 0)
	{
		close(handle->socket_fd);
	}

	unlink(handle->socket_path);

	pthread_mutex_destroy(&handle->stats_lock);
	free(handle);

	GLOG_INFO("[Mgmt] Socket destroyed");
}

int mgmt_socket_get_fd(MgmtSocketHandle handle)
{
	return handle ? handle->socket_fd : -1;
}

/* ============================================================================
 * Command Processing
 * ============================================================================
 */

int mgmt_socket_process_one(MgmtSocketHandle handle, int timeout_ms)
{
	if (!handle || handle->socket_fd < 0)
		return -1;

	/* Setup timeout if specified */
	struct timeval tv = {0};
	if (timeout_ms > 0)
	{
		tv.tv_sec = timeout_ms / 1000;
		tv.tv_usec = (long)(timeout_ms % 1000) * 1000;

		if (setsockopt(handle->socket_fd, SOL_SOCKET, SO_RCVTIMEO, &tv,
					   sizeof(tv)) < 0)
		{
			GLOG_ERR("setsockopt: %s", safe_strerror(errno));
			return -1;
		}
	}

	/* Receive request */
	MgmtCommandRequest req;
	struct sockaddr_un client_addr;
	socklen_t client_len = sizeof(struct sockaddr_un);

	ssize_t bytes_rcvd = recvfrom(handle->socket_fd, &req, sizeof(req), 0,
								  (struct sockaddr *)&client_addr, &client_len);

	if (bytes_rcvd < 0)
	{
		if (errno == EAGAIN || errno == EWOULDBLOCK)
		{
			return -1; /* Timeout */
		}
		GLOG_ERR("recvfrom: %s", safe_strerror(errno));
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

	if (dispatch_result != 0)
	{
		pthread_mutex_lock(&handle->stats_lock);
		handle->failed_requests++;
		pthread_mutex_unlock(&handle->stats_lock);
	}

	/* Send response back */
	if (sendto(handle->socket_fd, &resp, sizeof(resp), 0,
			   (struct sockaddr *)&client_addr, client_len) < 0)
	{
		GLOG_ERR("sendto: %s", safe_strerror(errno));
		return -1;
	}

	/* Update statistics */
	pthread_mutex_lock(&handle->stats_lock);
	handle->total_responses++;
	pthread_mutex_unlock(&handle->stats_lock);

	/* Log command */
	GLOG_INFO("[Mgmt] cmd=%s result=%s", mgmt_command_str(req.cmd_type),
			  mgmt_result_str(resp.result_code));

	return 0;
}

int mgmt_socket_process_all(MgmtSocketHandle handle)
{
	if (!handle || handle->socket_fd < 0)
		return -1;

	int processed = 0;

	while (1)
	{
		/* Set non-blocking mode temporarily */
		struct timeval tv = {0, 1000}; /* 1ms timeout */
		if (setsockopt(handle->socket_fd, SOL_SOCKET, SO_RCVTIMEO, &tv,
					   sizeof(tv)) < 0)
		{
			break;
		}

		int result = mgmt_socket_process_one(handle, 1);
		if (result < 0)
			break; /* No more commands or error */

		processed++;
	}

	return processed;
}

/* ============================================================================
 * Statistics
 * ============================================================================
 */

int mgmt_socket_stats(MgmtSocketHandle handle, uint64_t *total_requests,
					  uint64_t *total_responses, uint64_t *failed_requests)
{
	if (!handle)
		return -1;

	pthread_mutex_lock(&handle->stats_lock);

	if (total_requests)
		*total_requests = handle->total_requests;
	if (total_responses)
		*total_responses = handle->total_responses;
	if (failed_requests)
		*failed_requests = handle->failed_requests;

	pthread_mutex_unlock(&handle->stats_lock);

	return 0;
}

int mgmt_socket_stats_reset(MgmtSocketHandle handle)
{
	if (!handle)
		return -1;

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
	if (!handle || handle->socket_fd < 0)
		return -1;
	(void)depth;
	return 0;
}

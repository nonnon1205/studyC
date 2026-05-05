#include <stdio.h>
#include <stdatomic.h>
#include "mgmt_worker.h"
#include "mgmt_socket.h"

void *mgmt_worker(void *arg)
{
	MgmtWorkerArg *a = (MgmtWorkerArg *)arg;
	if (!a || !a->socket_path || !a->keep_running)
		return NULL;

	MgmtSocketHandle sock = mgmt_socket_create(a->socket_path);
	if (!sock)
	{
		fprintf(stderr, "[Mgmt Worker] Failed to create socket at %s\n",
				a->socket_path);
		return NULL;
	}

	while (atomic_load_explicit(a->keep_running, memory_order_acquire))
	{
		mgmt_socket_process_one(sock, 1000);
	}

	mgmt_socket_destroy(sock);
	return NULL;
}

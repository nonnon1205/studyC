#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <pthread.h>
#include <stdio.h>
#include "metrics_collector.h"

struct MetricsCollectorState
{
	MetricsSnapshot snapshot;
	pthread_mutex_t lock;
	time_t startup_time;
	uint32_t next_snapshot_id;
};

void latency_record(LatencyStats *stats, uint64_t latency_us)
{
	if (!stats)
		return;

	if (stats->sample_count == 0)
	{
		stats->min_us = latency_us;
		stats->max_us = latency_us;
	}
	else
	{
		if (latency_us < stats->min_us)
			stats->min_us = latency_us;
		if (latency_us > stats->max_us)
			stats->max_us = latency_us;
	}

	stats->avg_us = (stats->avg_us * stats->sample_count + latency_us) /
					(stats->sample_count + 1);
	stats->sample_count++;
}

void latency_reset(LatencyStats *stats)
{
	if (!stats)
		return;
	memset(stats, 0, sizeof(LatencyStats));
}

MetricsHandle metrics_init(void)
{
	struct MetricsCollectorState *mc = (struct MetricsCollectorState *)malloc(
		sizeof(struct MetricsCollectorState));
	if (!mc)
		return NULL;

	memset(mc, 0, sizeof(struct MetricsCollectorState));
	pthread_mutex_init(&mc->lock, NULL);
	mc->startup_time = time(NULL);
	mc->snapshot.timestamp = mc->startup_time;

	return mc;
}

void metrics_close(MetricsHandle handle)
{
	if (!handle)
		return;
	pthread_mutex_destroy(&handle->lock);
	free(handle);
}

int metrics_snapshot(MetricsHandle handle, MetricsSnapshot *snap)
{
	if (!handle || !snap)
		return -1;

	pthread_mutex_lock(&handle->lock);

	*snap = handle->snapshot;
	snap->timestamp = time(NULL);
	snap->snapshot_id = handle->next_snapshot_id++;
	snap->uptime_seconds = (uint32_t)(snap->timestamp - handle->startup_time);

	snap->total_events = handle->snapshot.collector.events_processed +
						 handle->snapshot.router.shm_reads_total +
						 handle->snapshot.viewer.messages_processed;

	snap->total_errors = handle->snapshot.collector.shm_writes_failed +
						 handle->snapshot.collector.mq_sends_failed +
						 handle->snapshot.router.shm_reads_failed;

	pthread_mutex_unlock(&handle->lock);
	return 0;
}

int metrics_update_collector(MetricsHandle handle, uint64_t latency_us,
							 int success)
{
	if (!handle)
		return -1;

	pthread_mutex_lock(&handle->lock);
	handle->snapshot.collector.events_processed++;
	if (success)
	{
		handle->snapshot.collector.shm_writes_ok++;
		latency_record(&handle->snapshot.collector.latency, latency_us);
	}
	else
	{
		handle->snapshot.collector.shm_writes_failed++;
	}
	pthread_mutex_unlock(&handle->lock);
	return 0;
}

int metrics_update_router(MetricsHandle handle, int shm_success,
						  uint64_t tcp_bytes, uint64_t latency_us)
{
	if (!handle)
		return -1;

	pthread_mutex_lock(&handle->lock);
	handle->snapshot.router.shm_reads_total++;
	if (shm_success)
	{
		handle->snapshot.router.tcp_forwards_total++;
		handle->snapshot.router.tcp_bytes_sent += tcp_bytes;
		latency_record(&handle->snapshot.router.latency, latency_us);
	}
	else
	{
		handle->snapshot.router.shm_reads_failed++;
	}
	pthread_mutex_unlock(&handle->lock);
	return 0;
}

int metrics_update_viewer(MetricsHandle handle, uint64_t bytes_rcvd,
						  uint64_t latency_us)
{
	if (!handle)
		return -1;

	pthread_mutex_lock(&handle->lock);
	handle->snapshot.viewer.messages_processed++;
	handle->snapshot.viewer.tcp_bytes_received += bytes_rcvd;
	latency_record(&handle->snapshot.viewer.latency, latency_us);
	pthread_mutex_unlock(&handle->lock);
	return 0;
}

int metrics_set_buffer_util(MetricsHandle handle, const char *module,
							uint32_t util_pct)
{
	if (!handle || !module)
		return -1;

	pthread_mutex_lock(&handle->lock);
	if (strcmp(module, "collector") == 0)
	{
		handle->snapshot.collector.buffer_util_pct = util_pct;
	}
	else if (strcmp(module, "router") == 0)
	{
		handle->snapshot.router.buffer_util_pct = util_pct;
	}
	else if (strcmp(module, "viewer") == 0)
	{
		handle->snapshot.viewer.output_buffer_util_pct = util_pct;
	}
	else
	{
		pthread_mutex_unlock(&handle->lock);
		return -1;
	}
	pthread_mutex_unlock(&handle->lock);
	return 0;
}

int metrics_set_active_connections(MetricsHandle handle, const char *module,
								   uint32_t count)
{
	if (!handle || !module)
		return -1;

	pthread_mutex_lock(&handle->lock);
	if (strcmp(module, "router") == 0)
	{
		handle->snapshot.router.active_tcp_conns = count;
	}
	else if (strcmp(module, "viewer") == 0)
	{
		handle->snapshot.viewer.active_clients = count;
	}
	else
	{
		pthread_mutex_unlock(&handle->lock);
		return -1;
	}
	pthread_mutex_unlock(&handle->lock);
	return 0;
}

int metrics_reset(MetricsHandle handle)
{
	if (!handle)
		return -1;
	pthread_mutex_lock(&handle->lock);
	memset(&handle->snapshot, 0, sizeof(MetricsSnapshot));
	handle->snapshot.timestamp = time(NULL);
	pthread_mutex_unlock(&handle->lock);
	return 0;
}

int metrics_snapshot_to_json(const MetricsSnapshot *snap, char *buf,
							 size_t buf_size)
{
	if (!snap || !buf || buf_size == 0)
		return -1;

	int offset = 0;

#define APPEND(fmt, ...)                                                       \
	offset += snprintf(buf + offset, buf_size - offset, fmt, ##__VA_ARGS__);   \
	if (offset >= (int)buf_size - 1)                                           \
		return -1;

	APPEND("{\"timestamp\":%ld,\"snapshot_id\":%u,\"uptime_seconds\":%u,",
		   snap->timestamp, snap->snapshot_id, snap->uptime_seconds);

	APPEND("\"collector\":{");
	APPEND("\"events_processed\":%lu,", snap->collector.events_processed);
	APPEND("\"buffer_util_pct\":%u,", snap->collector.buffer_util_pct);
	APPEND("\"latency_avg_us\":%lu,", snap->collector.latency.avg_us);
	APPEND("\"latency_p99_us\":%lu", snap->collector.latency.p99_us);
	APPEND("},");

	APPEND("\"router\":{");
	APPEND("\"shm_reads_total\":%lu,", snap->router.shm_reads_total);
	APPEND("\"tcp_forwards_total\":%lu,", snap->router.tcp_forwards_total);
	APPEND("\"active_tcp_conns\":%u", snap->router.active_tcp_conns);
	APPEND("},");

	APPEND("\"viewer\":{");
	APPEND("\"messages_processed\":%lu,", snap->viewer.messages_processed);
	APPEND("\"tcp_bytes_received\":%lu,", snap->viewer.tcp_bytes_received);
	APPEND("\"active_clients\":%u", snap->viewer.active_clients);
	APPEND("},");

	APPEND("\"total_events\":%lu,", snap->total_events);
	APPEND("\"total_errors\":%lu}", snap->total_errors);

#undef APPEND

	return offset;
}

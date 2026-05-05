#ifndef METRICS_COLLECTOR_H
#define METRICS_COLLECTOR_H

#include <stdint.h>
#include <time.h>

/* ============================================================================
 * Metrics Collector Module
 *
 * Real-time metrics collection and aggregation
 * Each module tracks performance counters (thread-safe atomics)
 * ============================================================================
 */

typedef struct
{
	uint64_t min_us;
	uint64_t max_us;
	uint64_t avg_us;
	uint64_t p99_us;
	uint32_t sample_count;
} LatencyStats;

/* ============================================================================
 * Per-Module Metrics Structures
 * ============================================================================
 */

typedef struct
{
	uint64_t events_processed;
	uint64_t events_per_sec;
	uint64_t bytes_received;
	uint64_t shm_writes_ok;
	uint64_t shm_writes_failed;
	uint64_t mq_sends_ok;
	uint64_t mq_sends_failed;
	uint32_t buffer_util_pct;
	LatencyStats latency;
} MetricsCollector;

typedef struct
{
	uint64_t shm_reads_total;
	uint64_t shm_reads_failed;
	uint64_t tcp_forwards_total;
	uint64_t tcp_bytes_sent;
	uint32_t active_tcp_conns;
	uint32_t buffer_util_pct;
	int router_thread_alive;
	LatencyStats latency;
} MetricsRouter;

typedef struct
{
	uint64_t tcp_accepts_total;
	uint64_t tcp_bytes_received;
	uint64_t messages_processed;
	uint64_t output_count;
	uint32_t active_clients;
	uint32_t output_buffer_util_pct;
	LatencyStats latency;
} MetricsViewer;

/* ============================================================================
 * Unified Metrics Snapshot
 * ============================================================================
 */

typedef struct
{
	time_t timestamp;
	uint32_t snapshot_id;

	MetricsCollector collector;
	MetricsRouter router;
	MetricsViewer viewer;

	uint64_t total_events;
	uint64_t total_errors;
	uint32_t uptime_seconds;
} MetricsSnapshot;

/* ============================================================================
 * Metrics Handle and API
 * ============================================================================
 */

typedef struct MetricsCollectorState *MetricsHandle;

MetricsHandle metrics_init(void);
void metrics_close(MetricsHandle handle);
int metrics_snapshot(MetricsHandle handle, MetricsSnapshot *snap);

int metrics_update_collector(MetricsHandle handle, uint64_t latency_us,
							 int success);
int metrics_update_router(MetricsHandle handle, int shm_success,
						  uint64_t tcp_bytes, uint64_t latency_us);
int metrics_update_viewer(MetricsHandle handle, uint64_t bytes_rcvd,
						  uint64_t latency_us);

int metrics_set_buffer_util(MetricsHandle handle, const char *module,
							uint32_t util_pct);
int metrics_set_active_connections(MetricsHandle handle, const char *module,
								   uint32_t count);
int metrics_reset(MetricsHandle handle);

/* ============================================================================
 * Latency Statistics Helpers
 * ============================================================================
 */

void latency_record(LatencyStats *stats, uint64_t latency_us);
void latency_reset(LatencyStats *stats);

/* ============================================================================
 * Serialization
 * ============================================================================
 */

int metrics_snapshot_to_json(const MetricsSnapshot *snap, char *buf,
							 size_t buf_size);

#endif /* METRICS_COLLECTOR_H */

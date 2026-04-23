#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <pthread.h>
#include <stdio.h>
#include "metrics_collector.h"

/* ============================================================================
 * Metrics Collector Internal State
 * ============================================================================
 */

struct MetricsCollector {
    MetricsSnapshot snapshot;
    pthread_mutex_t lock;
    time_t startup_time;
    uint32_t next_snapshot_id;
};

/* ============================================================================
 * Latency Statistics Implementation
 * ============================================================================
 */

#define LATENCY_HISTORY_SIZE 100

typedef struct {
    uint64_t samples[LATENCY_HISTORY_SIZE];
    int write_idx;
    int sample_count;
} LatencyHistory;

static LatencyHistory g_pollio_history = {0};
static LatencyHistory g_msgrcv_history = {0};
static LatencyHistory g_udpkill_history = {0};

static int latency_compare_uint64(const void* a, const void* b)
{
    uint64_t ua = *(const uint64_t*)a;
    uint64_t ub = *(const uint64_t*)b;
    return (ua < ub) ? -1 : (ua > ub) ? 1 : 0;
}

static uint64_t latency_calculate_p99(LatencyHistory* hist)
{
    if (hist->sample_count == 0) return 0;

    /* Create sorted copy */
    uint64_t sorted[LATENCY_HISTORY_SIZE];
    memcpy(sorted, hist->samples, hist->sample_count * sizeof(uint64_t));
    qsort(sorted, hist->sample_count, sizeof(uint64_t), latency_compare_uint64);

    /* Return 99th percentile (or closest) */
    int p99_idx = (hist->sample_count * 99) / 100;
    if (p99_idx >= hist->sample_count) p99_idx = hist->sample_count - 1;

    return sorted[p99_idx];
}

void latency_record(LatencyStats* stats, uint64_t latency_us)
{
    if (!stats) return;

    /* Update min/max */
    if (stats->sample_count == 0) {
        stats->min_us = latency_us;
        stats->max_us = latency_us;
    } else {
        if (latency_us < stats->min_us) stats->min_us = latency_us;
        if (latency_us > stats->max_us) stats->max_us = latency_us;
    }

    /* Update average */
    stats->avg_us = (stats->avg_us * stats->sample_count + latency_us) /
                    (stats->sample_count + 1);

    stats->sample_count++;
}

void latency_reset(LatencyStats* stats)
{
    if (!stats) return;
    memset(stats, 0, sizeof(LatencyStats));
}

/* ============================================================================
 * Metrics Collector Implementation
 * ============================================================================
 */

MetricsHandle metrics_init(void)
{
    MetricsCollector* mc = (MetricsCollector*)malloc(sizeof(MetricsCollector));
    if (!mc) return NULL;

    memset(mc, 0, sizeof(MetricsCollector));
    pthread_mutex_init(&mc->lock, NULL);

    mc->startup_time = time(NULL);
    mc->snapshot.timestamp = mc->startup_time;
    mc->next_snapshot_id = 0;

    /* Initialize metrics to zero */
    memset(&mc->snapshot, 0, sizeof(MetricsSnapshot));

    return mc;
}

void metrics_close(MetricsHandle handle)
{
    if (!handle) return;

    pthread_mutex_destroy(&handle->lock);
    free(handle);
}

int metrics_snapshot(MetricsHandle handle, MetricsSnapshot* snap)
{
    if (!handle || !snap) return -1;

    pthread_mutex_lock(&handle->lock);

    /* Copy current snapshot */
    *snap = handle->snapshot;

    /* Update timestamp and ID */
    snap->timestamp = time(NULL);
    snap->snapshot_id = handle->next_snapshot_id++;
    snap->uptime_seconds = (uint32_t)(snap->timestamp - handle->startup_time);

    /* Calculate aggregates */
    snap->total_events = handle->snapshot.pollio.events_processed +
                         handle->snapshot.test_msgrcv.shm_reads_total +
                         handle->snapshot.test_udpkill.messages_processed;

    snap->total_errors = handle->snapshot.pollio.shm_writes_failed +
                         handle->snapshot.pollio.mq_sends_failed +
                         handle->snapshot.test_msgrcv.shm_reads_failed +
                         handle->snapshot.test_udpkill.tcp_accepts_total;

    pthread_mutex_unlock(&handle->lock);

    return 0;
}

int metrics_update_pollio(MetricsHandle handle, uint64_t latency_us, int success)
{
    if (!handle) return -1;

    pthread_mutex_lock(&handle->lock);

    handle->snapshot.pollio.events_processed++;

    if (success) {
        handle->snapshot.pollio.shm_writes_ok++;
        latency_record(&handle->snapshot.pollio.latency, latency_us);
    } else {
        handle->snapshot.pollio.shm_writes_failed++;
    }

    pthread_mutex_unlock(&handle->lock);

    return 0;
}

int metrics_update_msgrcv(MetricsHandle handle, int shm_success,
                          uint64_t tcp_bytes, uint64_t latency_us)
{
    if (!handle) return -1;

    pthread_mutex_lock(&handle->lock);

    handle->snapshot.test_msgrcv.shm_reads_total++;

    if (shm_success) {
        handle->snapshot.test_msgrcv.tcp_forwards_total++;
        handle->snapshot.test_msgrcv.tcp_bytes_sent += tcp_bytes;
        latency_record(&handle->snapshot.test_msgrcv.latency, latency_us);
    } else {
        handle->snapshot.test_msgrcv.shm_reads_failed++;
    }

    pthread_mutex_unlock(&handle->lock);

    return 0;
}

int metrics_update_udpkill(MetricsHandle handle, uint64_t bytes_rcvd,
                           uint64_t latency_us)
{
    if (!handle) return -1;

    pthread_mutex_lock(&handle->lock);

    handle->snapshot.test_udpkill.messages_processed++;
    handle->snapshot.test_udpkill.tcp_bytes_received += bytes_rcvd;
    latency_record(&handle->snapshot.test_udpkill.latency, latency_us);

    pthread_mutex_unlock(&handle->lock);

    return 0;
}

int metrics_set_buffer_util(MetricsHandle handle, const char* module,
                            uint32_t util_pct)
{
    if (!handle || !module) return -1;

    pthread_mutex_lock(&handle->lock);

    if (strcmp(module, "pollio") == 0) {
        handle->snapshot.pollio.buffer_util_pct = util_pct;
    } else if (strcmp(module, "msgrcv") == 0) {
        handle->snapshot.test_msgrcv.buffer_util_pct = util_pct;
    } else if (strcmp(module, "udpkill") == 0) {
        handle->snapshot.test_udpkill.output_buffer_util_pct = util_pct;
    } else {
        pthread_mutex_unlock(&handle->lock);
        return -1;
    }

    pthread_mutex_unlock(&handle->lock);

    return 0;
}

int metrics_set_active_connections(MetricsHandle handle, const char* module,
                                   uint32_t count)
{
    if (!handle || !module) return -1;

    pthread_mutex_lock(&handle->lock);

    if (strcmp(module, "msgrcv") == 0) {
        handle->snapshot.test_msgrcv.active_tcp_conns = count;
    } else if (strcmp(module, "udpkill") == 0) {
        handle->snapshot.test_udpkill.active_clients = count;
    } else {
        pthread_mutex_unlock(&handle->lock);
        return -1;
    }

    pthread_mutex_unlock(&handle->lock);

    return 0;
}

int metrics_reset(MetricsHandle handle)
{
    if (!handle) return -1;

    pthread_mutex_lock(&handle->lock);

    memset(&handle->snapshot, 0, sizeof(MetricsSnapshot));
    handle->snapshot.timestamp = time(NULL);

    pthread_mutex_unlock(&handle->lock);

    return 0;
}

/* ============================================================================
 * Serialization to JSON
 * ============================================================================
 */

int metrics_snapshot_to_json(const MetricsSnapshot* snap,
                              char* buf, size_t buf_size)
{
    if (!snap || !buf || buf_size == 0) return -1;

    int offset = 0;

    #define APPEND(fmt, ...) \
        offset += snprintf(buf + offset, buf_size - offset, fmt, ##__VA_ARGS__); \
        if (offset >= (int)buf_size - 1) return -1;

    APPEND("{\"timestamp\": %ld, \"snapshot_id\": %u, \"uptime_seconds\": %u, ",
           snap->timestamp, snap->snapshot_id, snap->uptime_seconds);

    APPEND("\"pollio\": {");
    APPEND("\"events_processed\": %lu, ", snap->pollio.events_processed);
    APPEND("\"buffer_util_pct\": %u, ", snap->pollio.buffer_util_pct);
    APPEND("\"latency_avg_us\": %lu, ", snap->pollio.latency.avg_us);
    APPEND("\"latency_p99_us\": %lu", snap->pollio.latency.p99_us);
    APPEND("},");

    APPEND("\"test_msgrcv\": {");
    APPEND("\"shm_reads_total\": %lu, ", snap->test_msgrcv.shm_reads_total);
    APPEND("\"tcp_forwards_total\": %lu, ", snap->test_msgrcv.tcp_forwards_total);
    APPEND("\"active_tcp_conns\": %u", snap->test_msgrcv.active_tcp_conns);
    APPEND("},");

    APPEND("\"test_udpkill\": {");
    APPEND("\"messages_processed\": %lu, ", snap->test_udpkill.messages_processed);
    APPEND("\"tcp_bytes_received\": %lu, ", snap->test_udpkill.tcp_bytes_received);
    APPEND("\"active_clients\": %u", snap->test_udpkill.active_clients);
    APPEND("},");

    APPEND("\"total_events\": %lu, ", snap->total_events);
    APPEND("\"total_errors\": %lu", snap->total_errors);
    APPEND("}");

    #undef APPEND

    return offset;
}

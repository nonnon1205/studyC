#ifndef METRICS_COLLECTOR_H
#define METRICS_COLLECTOR_H

#include <stdint.h>
#include <time.h>

/* ============================================================================
 * Metrics Collector Module
 *
 * Real-time metrics collection and aggregation
 * Each module tracks performance counters (thread-safe atomics)
 * Separate metrics_worker thread polls and aggregates into snapshots
 * ============================================================================
 */

/* ============================================================================
 * Per-Module Metrics Structures
 * ============================================================================
 */

/**
 * Generic latency statistics (histogram-like)
 * Tracks min/max/avg/p99 latencies
 */
typedef struct {
    uint64_t min_us;            /* Minimum latency observed */
    uint64_t max_us;            /* Maximum latency observed */
    uint64_t avg_us;            /* Average latency */
    uint64_t p99_us;            /* 99th percentile latency */
    uint32_t sample_count;      /* Number of samples */
} LatencyStats;

/**
 * PollIO Module Metrics
 */
typedef struct {
    uint64_t events_processed;     /* Total events processed */
    uint64_t events_per_sec;       /* Current rate (calculated) */
    uint64_t bytes_received;       /* Total bytes received */
    uint64_t shm_writes_ok;        /* SHM write successes */
    uint64_t shm_writes_failed;    /* SHM write failures */
    uint64_t mq_sends_ok;          /* MQ send successes */
    uint64_t mq_sends_failed;      /* MQ send failures */
    uint32_t buffer_util_pct;      /* Buffer utilization 0-100 */
    LatencyStats latency;          /* Event processing latency */
} MetricsPollio;

/**
 * TestMsgRcv Module Metrics
 */
typedef struct {
    uint64_t shm_reads_total;      /* Total SHM reads */
    uint64_t shm_reads_failed;     /* SHM read failures */
    uint64_t tcp_forwards_total;   /* Messages forwarded via TCP */
    uint64_t tcp_bytes_sent;       /* Bytes sent via TCP */
    uint32_t active_tcp_conns;     /* Current open connections */
    uint32_t buffer_util_pct;      /* SHM buffer utilization */
    int router_thread_alive;       /* Router worker thread status */
    LatencyStats latency;          /* Message routing latency */
} MetricsTestMsgRcv;

/**
 * TestUDPKill Module Metrics
 */
typedef struct {
    uint64_t tcp_accepts_total;    /* Total client connections */
    uint64_t tcp_bytes_received;   /* Total bytes received */
    uint64_t messages_processed;   /* Messages processed */
    uint64_t output_count;         /* Output messages printed */
    uint32_t active_clients;       /* Current client connections */
    uint32_t output_buffer_util_pct; /* Output buffer usage */
    LatencyStats latency;          /* Processing latency */
} MetricsTestUdpkill;

/* ============================================================================
 * Unified Metrics Snapshot
 * ============================================================================
 */

typedef struct {
    /* Snapshot metadata */
    time_t timestamp;              /* When this snapshot was taken */
    uint32_t snapshot_id;          /* Sequence number */

    /* Per-module metrics */
    MetricsPollio pollio;
    MetricsTestMsgRcv test_msgrcv;
    MetricsTestUdpkill test_udpkill;

    /* System-wide aggregates */
    uint64_t total_events;         /* Sum of all events */
    uint64_t total_errors;         /* Sum of all errors */
    uint32_t uptime_seconds;       /* Seconds since startup */
} MetricsSnapshot;

/* ============================================================================
 * Metrics Handle and API
 * ============================================================================
 */

typedef struct MetricsCollector* MetricsHandle;

/**
 * Initialize the metrics collector
 *
 * Must be called once at startup before any metric updates
 *
 * @return  Metrics handle on success, NULL on failure
 */
MetricsHandle metrics_init(void);

/**
 * Shutdown and free the metrics collector
 *
 * @param handle    Metrics handle
 */
void metrics_close(MetricsHandle handle);

/**
 * Get current metrics snapshot
 *
 * Returns a consistent snapshot of all metrics at a point in time.
 * Safe to call from any thread.
 *
 * @param handle    Metrics handle
 * @param snap      Output: metrics snapshot
 * @return          0 on success, -1 on error
 */
int metrics_snapshot(MetricsHandle handle, MetricsSnapshot* snap);

/**
 * Update PollIO metrics
 * Called from PollIO worker threads when significant events occur
 *
 * @param handle    Metrics handle
 * @param latency_us Event processing latency in microseconds
 * @param success    1 if event processed successfully, 0 otherwise
 * @return          0 on success, -1 on error
 */
int metrics_update_pollio(MetricsHandle handle, uint64_t latency_us, int success);

/**
 * Update TestMsgRcv metrics
 *
 * @param handle        Metrics handle
 * @param shm_success   1 if SHM read succeeded
 * @param tcp_bytes     Bytes sent via TCP (0 if not sent)
 * @param latency_us    Routing latency
 * @return              0 on success
 */
int metrics_update_msgrcv(MetricsHandle handle, int shm_success,
                          uint64_t tcp_bytes, uint64_t latency_us);

/**
 * Update TestUDPKill metrics
 *
 * @param handle        Metrics handle
 * @param bytes_rcvd    Bytes received in this update
 * @param latency_us    Processing latency
 * @return              0 on success
 */
int metrics_update_udpkill(MetricsHandle handle, uint64_t bytes_rcvd,
                           uint64_t latency_us);

/**
 * Set buffer utilization percentage (0-100)
 *
 * @param handle        Metrics handle
 * @param module        Module name ("pollio", "msgrcv", "udpkill")
 * @param util_pct      Utilization percentage
 * @return              0 on success, -1 if module not found
 */
int metrics_set_buffer_util(MetricsHandle handle, const char* module,
                            uint32_t util_pct);

/**
 * Set active connection count
 *
 * @param handle        Metrics handle
 * @param module        Module name
 * @param count         Number of active connections
 * @return              0 on success
 */
int metrics_set_active_connections(MetricsHandle handle, const char* module,
                                   uint32_t count);

/**
 * Reset all metrics to zero
 *
 * @param handle        Metrics handle
 * @return              0 on success
 */
int metrics_reset(MetricsHandle handle);

/* ============================================================================
 * Latency Statistics Helpers
 * ============================================================================
 */

/**
 * Record a latency measurement
 *
 * Updates min/max/avg/p99 statistics
 * Note: p99 calculation is approximate (tracks last 100 samples)
 *
 * @param stats         Latency stats structure
 * @param latency_us    Latency measurement in microseconds
 */
void latency_record(LatencyStats* stats, uint64_t latency_us);

/**
 * Reset latency statistics
 *
 * @param stats         Latency stats structure
 */
void latency_reset(LatencyStats* stats);

/* ============================================================================
 * Serialization (for View Dashboard)
 * ============================================================================
 */

/**
 * Serialize metrics snapshot to JSON
 *
 * Produces human-readable JSON format suitable for dashboards
 *
 * @param snap          Metrics snapshot
 * @param buf           Output buffer
 * @param buf_size      Size of output buffer
 * @return              Number of bytes written, -1 on error
 */
int metrics_snapshot_to_json(const MetricsSnapshot* snap,
                              char* buf, size_t buf_size);

#endif /* METRICS_COLLECTOR_H */

#ifndef UNIFIED_LOGGER_H
#define UNIFIED_LOGGER_H

#include <stdint.h>
#include <time.h>

/* ============================================================================
 * Unified Logging Module
 *
 * Centralized logging system for all modules
 * Combines syslog + optional file output with rich context information
 * Thread-safe and designed for performance
 * ============================================================================
 */

/* Log Levels (matching syslog levels) */
typedef enum {
    ULOG_LEVEL_TRACE   = 0,  /* Most verbose: function entry/exit, variable values */
    ULOG_LEVEL_DEBUG   = 1,  /* Debug information: state transitions, detailed flow */
    ULOG_LEVEL_INFO    = 2,  /* Informational: normal operations, milestones */
    ULOG_LEVEL_WARN    = 3,  /* Warning: potentially problematic situations */
    ULOG_LEVEL_ERROR   = 4,  /* Error: error conditions that need attention */
    ULOG_LEVEL_FATAL   = 5   /* Fatal: unrecoverable errors, program shutdown */
} UlogLevel;

/* ============================================================================
 * Logger Context (Opaque Handle)
 * ============================================================================
 */

typedef struct UlogContext* UlogHandle;

/* ============================================================================
 * Initialization and Configuration
 * ============================================================================
 */

/**
 * Initialize the unified logger
 *
 * Must be called once at program startup before any logging
 *
 * @param ident         Syslog identifier (e.g., "TestMsgRcv", "router_worker")
 * @param level         Minimum log level to output
 * @param flags         Configuration flags (0 for defaults)
 *                      0x01 = enable file output to /tmp/sutdyc.log
 *                      0x02 = include thread IDs
 *                      0x04 = include timestamps
 * @return              Logger handle on success, NULL on failure
 */
UlogHandle ulog_init(const char* ident, UlogLevel level, uint32_t flags);

/**
 * Shutdown the logger and free resources
 *
 * @param logger        Logger handle
 */
void ulog_close(UlogHandle logger);

/**
 * Set the minimum log level for output
 *
 * @param logger        Logger handle
 * @param level         New minimum level
 */
void ulog_set_level(UlogHandle logger, UlogLevel level);

/**
 * Get the current minimum log level
 *
 * @param logger        Logger handle
 * @return              Current level
 */
UlogLevel ulog_get_level(UlogHandle logger);

/* ============================================================================
 * Logging Functions (Variadic)
 * ============================================================================
 */

/**
 * Log a message at TRACE level
 * @param logger        Logger handle
 * @param fmt, ...      printf-style format string
 */
void ulog_trace(UlogHandle logger, const char* fmt, ...);

/**
 * Log a message at DEBUG level
 */
void ulog_debug(UlogHandle logger, const char* fmt, ...);

/**
 * Log a message at INFO level
 */
void ulog_info(UlogHandle logger, const char* fmt, ...);

/**
 * Log a message at WARN level
 */
void ulog_warn(UlogHandle logger, const char* fmt, ...);

/**
 * Log a message at ERROR level
 */
void ulog_error(UlogHandle logger, const char* fmt, ...);

/**
 * Log a message at FATAL level (does NOT exit)
 */
void ulog_fatal(UlogHandle logger, const char* fmt, ...);

/* ============================================================================
 * Generic Logging Function
 * ============================================================================
 */

/**
 * Log with explicit level
 *
 * @param logger        Logger handle
 * @param level         Log level
 * @param fmt, ...      printf-style format string
 */
void ulog_log(UlogHandle logger, UlogLevel level, const char* fmt, ...);

/* ============================================================================
 * Context-Aware Logging (With Location Info)
 * ============================================================================
 */

/**
 * Log with file:line information
 * Typically called via ULOG_*_AT macros
 *
 * @param logger        Logger handle
 * @param level         Log level
 * @param file          Source filename
 * @param line          Line number
 * @param func          Function name
 * @param fmt, ...      printf-style format string
 */
void ulog_log_at(UlogHandle logger, UlogLevel level,
                 const char* file, int line, const char* func,
                 const char* fmt, ...);

/* ============================================================================
 * Convenience Macros
 * ============================================================================
 */

/* Basic macros - use when logger is available */
#define ULOG_TRACE(log, fmt, ...) ulog_trace(log, fmt, ##__VA_ARGS__)
#define ULOG_DEBUG(log, fmt, ...) ulog_debug(log, fmt, ##__VA_ARGS__)
#define ULOG_INFO(log, fmt, ...)  ulog_info(log, fmt, ##__VA_ARGS__)
#define ULOG_WARN(log, fmt, ...)  ulog_warn(log, fmt, ##__VA_ARGS__)
#define ULOG_ERROR(log, fmt, ...) ulog_error(log, fmt, ##__VA_ARGS__)
#define ULOG_FATAL(log, fmt, ...) ulog_fatal(log, fmt, ##__VA_ARGS__)

/* Location-aware macros - include file:line:func */
#define ULOG_TRACE_AT(log, fmt, ...) \
    ulog_log_at(log, ULOG_LEVEL_TRACE, __FILE__, __LINE__, __func__, fmt, ##__VA_ARGS__)
#define ULOG_DEBUG_AT(log, fmt, ...) \
    ulog_log_at(log, ULOG_LEVEL_DEBUG, __FILE__, __LINE__, __func__, fmt, ##__VA_ARGS__)
#define ULOG_INFO_AT(log, fmt, ...) \
    ulog_log_at(log, ULOG_LEVEL_INFO, __FILE__, __LINE__, __func__, fmt, ##__VA_ARGS__)
#define ULOG_WARN_AT(log, fmt, ...) \
    ulog_log_at(log, ULOG_LEVEL_WARN, __FILE__, __LINE__, __func__, fmt, ##__VA_ARGS__)
#define ULOG_ERROR_AT(log, fmt, ...) \
    ulog_log_at(log, ULOG_LEVEL_ERROR, __FILE__, __LINE__, __func__, fmt, ##__VA_ARGS__)
#define ULOG_FATAL_AT(log, fmt, ...) \
    ulog_log_at(log, ULOG_LEVEL_FATAL, __FILE__, __LINE__, __func__, fmt, ##__VA_ARGS__)

/* ============================================================================
 * Statistics and Debugging
 * ============================================================================
 */

/**
 * Get logger statistics
 *
 * @param logger        Logger handle
 * @param total_logs    Output: total log messages
 * @param dropped_logs  Output: log messages dropped (e.g., due to level filtering)
 * @return              0 on success
 */
int ulog_stats(UlogHandle logger, uint64_t* total_logs, uint64_t* dropped_logs);

/**
 * Reset logger statistics
 *
 * @param logger        Logger handle
 * @return              0 on success
 */
int ulog_stats_reset(UlogHandle logger);

/**
 * Set a user-defined context tag (e.g., request ID)
 *
 * This tag will be included in all subsequent log messages from this logger
 *
 * @param logger        Logger handle
 * @param tag           Tag string (max 32 chars)
 * @return              0 on success
 */
int ulog_set_context_tag(UlogHandle logger, const char* tag);

/* ============================================================================
 * Backward Compatibility with log_wrapper
 * ============================================================================
 */

/**
 * Legacy function: log_init (wraps ulog_init)
 * Initializes a default global logger with syslog output
 */
void log_init(const char* ident);

/**
 * Legacy function: log_close
 */
void log_close(void);

/**
 * Legacy functions: log_info, log_warn, log_err
 * These use the default global logger
 */
void log_info(const char* fmt, ...);
void log_warn(const char* fmt, ...);
void log_err(const char* fmt, ...);

#endif /* UNIFIED_LOGGER_H */

#define _GNU_SOURCE
#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <time.h>
#include <pthread.h>
#include <syslog.h>
#include <unistd.h>
#include <sys/types.h>
#include "unified_logger.h"

struct UlogContext {
    char ident[64];              /* Syslog identifier */
    UlogLevel min_level;         /* Minimum level to output */
    uint32_t flags;              /* Configuration flags */
    int syslog_enabled;          /* Syslog output enabled */
    int file_enabled;            /* File output enabled */
    FILE* file_ptr;              /* File handle */
    uint64_t total_messages;     /* Statistics: total logged */
    uint64_t dropped_messages;   /* Statistics: dropped due to level */
    char context_tag[32];        /* User context tag */
    pthread_mutex_t lock;        /* Thread safety */
};

/* Global default logger */
static UlogHandle g_default_logger = NULL;

/* ============================================================================
 * Helper: Level to Syslog Priority
 * ============================================================================
 */

static int ulog_level_to_syslog_priority(UlogLevel level)
{
    switch (level) {
        case ULOG_LEVEL_TRACE:
        case ULOG_LEVEL_DEBUG:  return LOG_DEBUG;
        case ULOG_LEVEL_INFO:   return LOG_INFO;
        case ULOG_LEVEL_WARN:   return LOG_WARNING;
        case ULOG_LEVEL_ERROR:  return LOG_ERR;
        case ULOG_LEVEL_FATAL:  return LOG_CRIT;
        default:                return LOG_INFO;
    }
}

/* ============================================================================
 * Helper: Level to String
 * ============================================================================
 */

static const char* ulog_level_str(UlogLevel level)
{
    switch (level) {
        case ULOG_LEVEL_TRACE:  return "TRACE";
        case ULOG_LEVEL_DEBUG:  return "DEBUG";
        case ULOG_LEVEL_INFO:   return "INFO";
        case ULOG_LEVEL_WARN:   return "WARN";
        case ULOG_LEVEL_ERROR:  return "ERROR";
        case ULOG_LEVEL_FATAL:  return "FATAL";
        default:                return "UNKNOWN";
    }
}

/* ============================================================================
 * Internal: Format and Output Message
 * ============================================================================
 */

static void ulog_output(UlogHandle logger, UlogLevel level,
                        const char* file, int line, const char* func,
                        const char* fmt, va_list ap)
{
    if (!logger) return;

    pthread_mutex_lock(&logger->lock);

    /* Format message */
    char message[2048];
    int offset = 0;

    /* Add timestamp if enabled */
    if (logger->flags & 0x04) {
        time_t now = time(NULL);
        struct tm tm_buf;
        struct tm* tm_info = localtime_r(&now, &tm_buf);
        char time_str[32];
        strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", tm_info);
        offset += snprintf(message + offset, sizeof(message) - offset,
                          "[%s] ", time_str);
    }

    /* Add level */
    offset += snprintf(message + offset, sizeof(message) - offset,
                      "[%s] ", ulog_level_str(level));

    /* Add thread ID if enabled */
    if (logger->flags & 0x02) {
        offset += snprintf(message + offset, sizeof(message) - offset,
                          "[TID:%lu] ", (unsigned long)pthread_self());
    }

    /* Add context tag if set */
    if (logger->context_tag[0] != '\0') {
        offset += snprintf(message + offset, sizeof(message) - offset,
                          "[%s] ", logger->context_tag);
    }

    /* Add location if available */
    if (file && func) {
        offset += snprintf(message + offset, sizeof(message) - offset,
                          "[%s:%d:%s] ", file, line, func);
    }

    /* Add actual message */
    vsnprintf(message + offset, sizeof(message) - offset, fmt, ap);

    /* Output to syslog */
    if (logger->syslog_enabled) {
        syslog(ulog_level_to_syslog_priority(level), "%s", message);
    }

    /* Output to file */
    if (logger->file_enabled && logger->file_ptr) {
        fprintf(logger->file_ptr, "%s\n", message);
        fflush(logger->file_ptr);
    }

    logger->total_messages++;

    pthread_mutex_unlock(&logger->lock);
}

/* ============================================================================
 * Public API: Initialization
 * ============================================================================
 */

UlogHandle ulog_init(const char* ident, UlogLevel level, uint32_t flags)
{
    struct UlogContext* logger = (struct UlogContext*)malloc(sizeof(struct UlogContext));
    if (!logger) return NULL;

    memset(logger, 0, sizeof(struct UlogContext));

    if (ident) {
        strncpy(logger->ident, ident, sizeof(logger->ident) - 1);
    }

    logger->min_level = level;
    logger->flags = flags;
    logger->syslog_enabled = 1;
    logger->total_messages = 0;
    logger->dropped_messages = 0;
    logger->context_tag[0] = '\0';

    pthread_mutex_init(&logger->lock, NULL);

    /* Open syslog */
    openlog(logger->ident, LOG_PID | LOG_NDELAY, LOG_USER);

    /* Open file if enabled */
    if (flags & 0x01) {
        logger->file_ptr = fopen("/tmp/sutdyc.log", "a");
        logger->file_enabled = (logger->file_ptr != NULL);
    }

    return logger;
}

void ulog_close(UlogHandle logger)
{
    if (!logger) return;

    pthread_mutex_lock(&logger->lock);

    if (logger->file_ptr) {
        fclose(logger->file_ptr);
        logger->file_ptr = NULL;
    }

    pthread_mutex_unlock(&logger->lock);
    pthread_mutex_destroy(&logger->lock);

    closelog();
    free(logger);
}

void ulog_set_level(UlogHandle logger, UlogLevel level)
{
    if (!logger) return;
    pthread_mutex_lock(&logger->lock);
    logger->min_level = level;
    pthread_mutex_unlock(&logger->lock);
}

UlogLevel ulog_get_level(UlogHandle logger)
{
    if (!logger) return ULOG_LEVEL_INFO;
    pthread_mutex_lock(&logger->lock);
    UlogLevel level = logger->min_level;
    pthread_mutex_unlock(&logger->lock);
    return level;
}

/* ============================================================================
 * Public API: Logging Functions
 * ============================================================================
 */

void ulog_trace(UlogHandle logger, const char* fmt, ...)
{
    if (!logger || logger->min_level > ULOG_LEVEL_TRACE) {
        if (logger) {
            pthread_mutex_lock(&logger->lock);
            logger->dropped_messages++;
            pthread_mutex_unlock(&logger->lock);
        }
        return;
    }
    va_list ap;
    va_start(ap, fmt);
    ulog_output(logger, ULOG_LEVEL_TRACE, NULL, 0, NULL, fmt, ap);
    va_end(ap);
}

void ulog_debug(UlogHandle logger, const char* fmt, ...)
{
    if (!logger || logger->min_level > ULOG_LEVEL_DEBUG) {
        if (logger) {
            pthread_mutex_lock(&logger->lock);
            logger->dropped_messages++;
            pthread_mutex_unlock(&logger->lock);
        }
        return;
    }
    va_list ap;
    va_start(ap, fmt);
    ulog_output(logger, ULOG_LEVEL_DEBUG, NULL, 0, NULL, fmt, ap);
    va_end(ap);
}

void ulog_info(UlogHandle logger, const char* fmt, ...)
{
    if (!logger || logger->min_level > ULOG_LEVEL_INFO) {
        if (logger) {
            pthread_mutex_lock(&logger->lock);
            logger->dropped_messages++;
            pthread_mutex_unlock(&logger->lock);
        }
        return;
    }
    va_list ap;
    va_start(ap, fmt);
    ulog_output(logger, ULOG_LEVEL_INFO, NULL, 0, NULL, fmt, ap);
    va_end(ap);
}

void ulog_warn(UlogHandle logger, const char* fmt, ...)
{
    if (!logger || logger->min_level > ULOG_LEVEL_WARN) {
        if (logger) {
            pthread_mutex_lock(&logger->lock);
            logger->dropped_messages++;
            pthread_mutex_unlock(&logger->lock);
        }
        return;
    }
    va_list ap;
    va_start(ap, fmt);
    ulog_output(logger, ULOG_LEVEL_WARN, NULL, 0, NULL, fmt, ap);
    va_end(ap);
}

void ulog_error(UlogHandle logger, const char* fmt, ...)
{
    if (!logger || logger->min_level > ULOG_LEVEL_ERROR) {
        if (logger) {
            pthread_mutex_lock(&logger->lock);
            logger->dropped_messages++;
            pthread_mutex_unlock(&logger->lock);
        }
        return;
    }
    va_list ap;
    va_start(ap, fmt);
    ulog_output(logger, ULOG_LEVEL_ERROR, NULL, 0, NULL, fmt, ap);
    va_end(ap);
}

void ulog_fatal(UlogHandle logger, const char* fmt, ...)
{
    if (!logger) return;
    va_list ap;
    va_start(ap, fmt);
    ulog_output(logger, ULOG_LEVEL_FATAL, NULL, 0, NULL, fmt, ap);
    va_end(ap);
}

void ulog_log(UlogHandle logger, UlogLevel level, const char* fmt, ...)
{
    if (!logger || logger->min_level > level) {
        if (logger) {
            pthread_mutex_lock(&logger->lock);
            logger->dropped_messages++;
            pthread_mutex_unlock(&logger->lock);
        }
        return;
    }
    va_list ap;
    va_start(ap, fmt);
    ulog_output(logger, level, NULL, 0, NULL, fmt, ap);
    va_end(ap);
}

void ulog_log_at(UlogHandle logger, UlogLevel level,
                 const char* file, int line, const char* func,
                 const char* fmt, ...)
{
    if (!logger || logger->min_level > level) {
        if (logger) {
            pthread_mutex_lock(&logger->lock);
            logger->dropped_messages++;
            pthread_mutex_unlock(&logger->lock);
        }
        return;
    }
    va_list ap;
    va_start(ap, fmt);
    ulog_output(logger, level, file, line, func, fmt, ap);
    va_end(ap);
}

/* ============================================================================
 * Public API: Statistics
 * ============================================================================
 */

int ulog_stats(UlogHandle logger, uint64_t* total_logs, uint64_t* dropped_logs)
{
    if (!logger) return -1;

    pthread_mutex_lock(&logger->lock);
    if (total_logs) *total_logs = logger->total_messages + logger->dropped_messages;
    if (dropped_logs) *dropped_logs = logger->dropped_messages;
    pthread_mutex_unlock(&logger->lock);

    return 0;
}

int ulog_stats_reset(UlogHandle logger)
{
    if (!logger) return -1;

    pthread_mutex_lock(&logger->lock);
    logger->total_messages = 0;
    logger->dropped_messages = 0;
    pthread_mutex_unlock(&logger->lock);

    return 0;
}

int ulog_set_context_tag(UlogHandle logger, const char* tag)
{
    if (!logger) return -1;

    pthread_mutex_lock(&logger->lock);
    if (tag) {
        strncpy(logger->context_tag, tag, sizeof(logger->context_tag) - 1);
    } else {
        logger->context_tag[0] = '\0';
    }
    pthread_mutex_unlock(&logger->lock);

    return 0;
}

/* ============================================================================
 * Legacy API: Backward Compatibility with log_wrapper
 * ============================================================================
 */

UlogHandle log_get_handle(void)
{
    return g_default_logger;
}

void log_init(const char* ident)
{
    if (!g_default_logger) {
        g_default_logger = ulog_init(ident, ULOG_LEVEL_INFO, 0);
    }
}

void log_close(void)
{
    if (g_default_logger) {
        ulog_close(g_default_logger);
        g_default_logger = NULL;
    }
}

void log_info(const char* fmt, ...)
{
    if (!g_default_logger) {
        g_default_logger = ulog_init("unknown", ULOG_LEVEL_INFO, 0);
    }
    va_list ap;
    va_start(ap, fmt);
    ulog_output(g_default_logger, ULOG_LEVEL_INFO, NULL, 0, NULL, fmt, ap);
    va_end(ap);
}

void log_warn(const char* fmt, ...)
{
    if (!g_default_logger) {
        g_default_logger = ulog_init("unknown", ULOG_LEVEL_INFO, 0);
    }
    va_list ap;
    va_start(ap, fmt);
    ulog_output(g_default_logger, ULOG_LEVEL_WARN, NULL, 0, NULL, fmt, ap);
    va_end(ap);
}

void log_err(const char* fmt, ...)
{
    if (!g_default_logger) {
        g_default_logger = ulog_init("unknown", ULOG_LEVEL_INFO, 0);
    }
    va_list ap;
    va_start(ap, fmt);
    ulog_output(g_default_logger, ULOG_LEVEL_ERROR, NULL, 0, NULL, fmt, ap);
    va_end(ap);
}

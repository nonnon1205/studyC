#define _POSIX_C_SOURCE 200809L
#include "log_wrapper.h"
#include <stdarg.h>
#include <stdio.h>
#include <syslog.h>

void log_init(const char* ident) {
    openlog(ident, LOG_PID | LOG_NDELAY, LOG_USER);
}

void log_close(void) {
    closelog();
}

static void log_vmessage(int priority, const char* fmt, va_list ap) {
    char buf[1024];
    vsnprintf(buf, sizeof(buf), fmt, ap);
    syslog(priority, "%s", buf);
}

void log_info(const char* fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    log_vmessage(LOG_INFO, fmt, ap);
    va_end(ap);
}

void log_warn(const char* fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    log_vmessage(LOG_WARNING, fmt, ap);
    va_end(ap);
}

void log_err(const char* fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    log_vmessage(LOG_ERR, fmt, ap);
    va_end(ap);
}

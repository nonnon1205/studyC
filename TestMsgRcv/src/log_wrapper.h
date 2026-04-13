#ifndef LOG_WRAPPER_H
#define LOG_WRAPPER_H

#include <stdarg.h>

void log_init(const char* ident);
void log_close(void);
void log_info(const char* fmt, ...);
void log_warn(const char* fmt, ...);
void log_err(const char* fmt, ...);

#endif // LOG_WRAPPER_H

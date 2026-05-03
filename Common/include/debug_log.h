#ifndef DEBUG_LOG_H
#define DEBUG_LOG_H

#ifndef SAFE_STRERROR_DEFINED
#define SAFE_STRERROR_DEFINED
#include <string.h>
#include <stdio.h>
static inline const char* safe_strerror(int errnum) {
    static _Thread_local char errbuf[64];
#if defined(_GNU_SOURCE)
    const char* p = strerror_r(errnum, errbuf, sizeof(errbuf));
    if (p != errbuf)
        snprintf(errbuf, sizeof(errbuf), "%s", p);
#else
    if (strerror_r(errnum, errbuf, sizeof(errbuf)) != 0)
        snprintf(errbuf, sizeof(errbuf), "error %d", errnum);
#endif
    return errbuf;
}
#endif

#ifndef MODULE_NAME
  #define MODULE_NAME "Unknown"
#endif

#ifdef DEBUG
  #include <stdio.h>
  #define DBG(fmt, ...) \
      fprintf(stderr, "[DBG][" MODULE_NAME "] %s:%d (%s): " fmt "\n", \
              __FILE__, __LINE__, __func__, ##__VA_ARGS__)
#else
  #define DBG(fmt, ...) ((void)0)
#endif

#endif /* DEBUG_LOG_H */

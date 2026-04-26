#ifndef DEBUG_LOG_H
#define DEBUG_LOG_H

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

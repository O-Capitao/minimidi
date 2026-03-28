#ifndef MM_LOG_H
#define MM_LOG_H

#include <stdio.h>
#include <stdint.h>
#include <stdarg.h>

typedef enum {
  LOG_TRACE,
  LOG_DEBUG,
  LOG_INFO,
  LOG_WARN,
  LOG_ERROR,
  LOG_FATAL
} LogLevel;

// Function declarations
int log_init(const char *filename);
void log_deinit();
void log_log(LogLevel level, const char *fmt, ...);

// Convenience macros
#define log_trace(fmt, ...) log_log(LOG_TRACE, fmt, ##__VA_ARGS__)
#define log_debug(fmt, ...) log_log(LOG_DEBUG, fmt, ##__VA_ARGS__)
#define log_info(fmt, ...)  log_log(LOG_INFO, fmt, ##__VA_ARGS__)
#define log_warn(fmt, ...)  log_log(LOG_WARN, fmt, ##__VA_ARGS__)
#define log_error(fmt, ...) log_log(LOG_ERROR, fmt, ##__VA_ARGS__)
#define log_fatal(fmt, ...) log_log(LOG_FATAL, fmt, ##__VA_ARGS__)

#endif /* MM_LOG_H */
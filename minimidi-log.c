#define _POSIX_C_SOURCE 200809L
#include <stdlib.h>
#include <time.h>
#include <assert.h>
#include <stdarg.h>
#include <string.h>
#include "minimidi-log.h"

static struct {
    FILE *file;
    LogLevel level;
} L;

static const char *level_strings[] = {
  "TRACE", "DEBUG", "INFO", "WARN", "ERROR", "FATAL"
};

static const char *run_header = "\n\n"
    "**************************************************\n"
    "**************************************************\n"
    "***                                            ***\n"
    "***                  MINIMIDI                  ***\n"
    "***                                            ***\n"
    "**************************************************\n"
    "**************************************************\n";

int log_init(const char *filename) {
    L.file = fopen(filename, "a");
    if (L.file == NULL) {
        perror("Error opening log file");
        return 1;
    }
    
    // SET DEBUG LEVEL
    L.level = LOG_DEBUG;

    // Start today's logging
    fprintf(L.file, run_header);
    
    time_t now = time(NULL);
    char *date = ctime(&now);
    date[strlen(date) - 1] = '\0'; // Remove newline
    
    log_log(LOG_INFO, "Log initialized on %s", date);

    return 0;
}

void log_deinit() {
    if (L.file) {
        log_log(LOG_INFO, "Log de-initialized.");
        fclose(L.file);
    }
}

void log_log(LogLevel level, const char *fmt, ...) {
    if (level < L.level || !L.file) {
        return;
    }

    // Get current time
    // time_t now = time(NULL);
    // struct tm *t = localtime(&now);
    // char time_buf[20];
    // strftime(time_buf, sizeof(time_buf), "%Y-%m-%d %H:%M:%S", t);
    struct timespec ts;
    struct tm tm_info;

    clock_gettime(CLOCK_REALTIME, &ts);      // seconds + nanoseconds
    localtime_r(&ts.tv_sec, &tm_info);       // convert seconds part

    char tmp[64];
    strftime(tmp, sizeof(tmp), "%Y-%m-%d %H:%M:%S", &tm_info);

    int ms = ts.tv_nsec / 1000000;           // nanoseconds → milliseconds
    char time_buf[64];
    snprintf(time_buf, 64, "%s.%03d", tmp, ms);

    // Format log message
    char log_line[1024];
    va_list args;
    va_start(args, fmt);
    int msg_len = vsnprintf(log_line, sizeof(log_line) - 50, fmt, args); // Leave space for header
    va_end(args);

    if (msg_len < 0) {
        // Handle vsnprintf error if needed
        return;
    }

    // Prepend timestamp and log level
    fprintf(L.file, "[%s] %-5s: %s\n", time_buf, level_strings[level], log_line);
    
    // It's good practice to flush for important c
    if (level >= LOG_WARN) {
        fflush(L.file);
    }
}
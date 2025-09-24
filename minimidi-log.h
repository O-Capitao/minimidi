#ifndef MM_LOG_H
#define MM_LOG_H

#include <stdio.h>
#include <stdint.h>

#define LOG_LINE_MAX_LEN 512

extern char MM_Log_log_line[ LOG_LINE_MAX_LEN ];

int MM_Log_init();
int MM_Log_writeline();
int MM_Log_free();

#endif /* MM_LOG_H */
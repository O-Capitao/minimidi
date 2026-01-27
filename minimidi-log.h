#ifndef MM_LOG_H
#define MM_LOG_H

#include <stdio.h>
#include <stdint.h>

#define LOG_LINE_MAX_LEN 512
#define LOG_LINES_IN_BUFFER 128

extern char MM_Log_log_line[ LOG_LINE_MAX_LEN ];
// extern 

int MM_Log_init();
int MM_Log_writeline();
int MM_Log_flush();
int MM_Log_free();

// utilities
int MM_Log_dump_arr_of_floats( float *values, size_t len );

#endif /* MM_LOG_H */
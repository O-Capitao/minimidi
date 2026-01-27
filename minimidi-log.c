#include <stdlib.h>
#include <time.h>
#include <assert.h>
#include <stdarg.h>
#include <string.h>

#include "minimidi-log.h"


static const char *run_header = "\n\n"
    "**************************************************\n"
    "**************************************************\n"
    "***                                            ***\n"
    "***                  MINIMIDI                  ***\n"
    "***                                            ***\n"
    "**************************************************\n"
    "**************************************************\n";

static char date_time_header[100];
char MM_Log_log_line[ LOG_LINE_MAX_LEN ]; // extern
char MM_Log_log_buffer[ LOG_LINE_MAX_LEN * LOG_LINES_IN_BUFFER ];
char MM_Log_formated_log_line[ 2 * LOG_LINE_MAX_LEN ]; 
FILE *MM_Log_file;

size_t _buffer_ln_count = 0;

int MM_Log_init()
{
    MM_Log_file = fopen("minimidi.log","a");

    if (MM_Log_file == NULL) {
        perror("Error opening file");
        return 1;
    }

    // Start today's logging
    fprintf( MM_Log_file, run_header);
    
    // MM_Log_log_line = (char *)malloc( LOG_LINE_MAX_LEN * sizeof( char ));
    return 0;
}

// append right to file, screw performance and whatever
int MM_Log_writeline()
{
    if (_buffer_ln_count >= LOG_LINES_IN_BUFFER - 1 ){
        MM_Log_flush();
    }

    time_t now = time(NULL);
    struct tm *t = localtime(&now); 

    strftime(date_time_header, sizeof(date_time_header)-1, "[ %d/%m/%Y . %H:%M:%S ]", t);
    sprintf( MM_Log_formated_log_line, "%s : %s\n", date_time_header, MM_Log_log_line );
    strcat(MM_Log_log_buffer, MM_Log_formated_log_line);

    _buffer_ln_count ++;
    return 0;
}

int MM_Log_flush(){
     fprintf( MM_Log_file, MM_Log_log_buffer );
    _buffer_ln_count = 0;
}

int MM_Log_free()
{
    int err;
    // Close the file
    err = fclose( MM_Log_file );
    if (err !=0) return err;

    // free(MM_Log_log_line);

    return 0;
}

char _buff[128];
// Use this to debug what's in the buffer.
// for now, just print 10 values from the start, to see if something fishy is goind on
int MM_Log_dump_arr_of_floats( float *values, size_t len ){
    
    // MM_Log_log_line[0] = '\0';

     for (int i = 0; i < 10; i++ ){
        sprintf(_buff, "%.2e ,", values[i]);
        strcat(MM_Log_log_line, _buff);
    }
    // 
    strcat(MM_Log_log_line, "\0");
    MM_Log_writeline();
    return 0;
}

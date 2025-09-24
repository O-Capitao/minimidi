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
FILE *MM_Log_file;

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
    time_t now = time(NULL);
    struct tm *t = localtime(&now);

    strftime(date_time_header, sizeof(date_time_header)-1, "[ %d/%m/%Y . %H:%M:%S ]", t);
    fprintf( MM_Log_file, "%s : %s\n", date_time_header, MM_Log_log_line );

    return 0;
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

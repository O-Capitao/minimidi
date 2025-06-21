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
char MiniMidi_Log_log_line[ LOG_LINE_MAX_LEN ]; // extern
FILE *MiniMidi_Log_file;

int MiniMidi_Log_init()
{
    MiniMidi_Log_file = fopen("minimidi.log","a");

    if (MiniMidi_Log_file == NULL) {
        perror("Error opening file");
        return 1;
    }

    // Start today's logging
    fprintf( MiniMidi_Log_file, run_header);
    
    // MiniMidi_Log_log_line = (char *)malloc( LOG_LINE_MAX_LEN * sizeof( char ));
    return 0;
}

// append right to file, screw performance and whatever
int MiniMidi_Log_writeline()
{
    time_t now = time(NULL);
    struct tm *t = localtime(&now);

    strftime(date_time_header, sizeof(date_time_header)-1, "[ %d/%m/%Y . %H:%M:%S ]", t);
    fprintf( MiniMidi_Log_file, "%s : %s\n", date_time_header, MiniMidi_Log_log_line );

    return 0;
}

int MiniMidi_Log_free()
{
    int err;
    // Close the file
    err = fclose( MiniMidi_Log_file );
    if (err !=0) return err;

    // free(MiniMidi_Log_log_line);

    return 0;
}

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

// this one by chatgpt
char* MiniMidi_Log_format_string_static(const char *format, ...) {
    // Static buffer — not thread-safe, will be overwritten on each call
    static char buffer[1024];

    va_list args;
    va_start(args, format);
    vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);

    return buffer;
}

MiniMidi_Log *MiniMidi_Log_init()
{
    MiniMidi_Log *instance = (MiniMidi_Log *)malloc( sizeof( MiniMidi_Log ));
    instance->buffer_end =0;
    instance->file = fopen("minimidi.log","a");

    if (instance->file == NULL) {
        perror("Error opening file");
        // return EXIT_FAILURE;
        return NULL;
    }

    // Start today's logging
    fprintf( instance->file, run_header);

    return instance;
}

// append right to file, screw performance and whatever
int MiniMidi_Log_dumb_append( MiniMidi_Log *self, char *text )
{
    time_t now = time(NULL);
    struct tm *t = localtime(&now);

    strftime(date_time_header, sizeof(date_time_header)-1, "[ %d/%m/%Y . %H:%M:%S ]", t);
    fprintf( self->file, "%s : %s\n", date_time_header, text );

    return 0;
}

int MiniMidi_Log_append( MiniMidi_Log *self, char *text )
{
    return 1;
}

int MiniMidi_Log_flush( MiniMidi_Log *self )
{
    char aux[ self->buffer_end ];

    self->buffer[ self->buffer_end ] = '\0';

    strncpy( aux, self->buffer, BUFFER_SIZE );

    fprintf( self->file, aux );

    // free(aux);
    return 0;
}

int MiniMidi_Log_free( MiniMidi_Log *self )
{
    int err;
    // Close the file
    err = fclose(self->file);
    if (err !=0) return err;

    free(self);

    return 0;
}

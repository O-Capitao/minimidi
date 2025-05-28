#ifndef MINIMIDI_LOG_H
#define MINIMIDI_LOG_H

#include <stdio.h>
#include <stdint.h>


#define BUFFER_SIZE 1024

typedef struct MiniMidi_Log
{
    char buffer[ BUFFER_SIZE ];
    size_t buffer_end;

    FILE *file;

} MiniMidi_Log;

MiniMidi_Log *MiniMidi_Log_init();
int MiniMidi_Log_append( MiniMidi_Log *self, char *text);
int MiniMidi_Log_dumb_append( MiniMidi_Log *self, char *text);
int MiniMidi_Log_flush( MiniMidi_Log *self );
int MiniMidi_Log_free( MiniMidi_Log *self );

char* MiniMidi_Log_format_string_static(const char *format, ...);

#endif /* MINIMIDI_LOG_H */
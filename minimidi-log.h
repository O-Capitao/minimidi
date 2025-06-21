#ifndef MINIMIDI_LOG_H
#define MINIMIDI_LOG_H

#include <stdio.h>
#include <stdint.h>

#define LOG_LINE_MAX_LEN 512

extern char MiniMidi_Log_log_line[ LOG_LINE_MAX_LEN ];

int MiniMidi_Log_init();
int MiniMidi_Log_writeline();
int MiniMidi_Log_free();

#endif /* MINIMIDI_LOG_H */
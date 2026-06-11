#ifndef MM_H
#define MM_H

#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>

#include "globals.h"
#include "minimidi-log.h"

// size of a buffer used to bring events to a caller fn,
// e.g. by searching
#define MIDI_EVENTS_BUFFER_SIZE 128

/****************************************************************************************
*
*
*   -> Enums and Aux Structs
****************************************************************************************/
typedef enum {
    MIDI_NOTE_OFF        = 0x80,  // 128
    MIDI_NOTE_ON         = 0x90,  // 144
    MIDI_POLY_AFTERTOUCH = 0xA0,  // 160
    MIDI_CONTROL_CHANGE  = 0xB0,  // 176
    MIDI_PROGRAM_CHANGE  = 0xC0,  // 192
    MIDI_CHAN_AFTERTOUCH = 0xD0,  // 208
    MIDI_PITCH_BEND      = 0xE0,  // 224
    MIDI_SYSTEM          = 0xF0,  // 240 System Messages
    MIDI_INVALID         = 0x00   // Invalid status
} MidiStatusCode;

typedef enum Note {
    C  = 0,
    Cs = 1,
    D  = 2,
    Ds = 3,
    E  = 4,
    F  = 5,
    Fs = 6,
    G  = 7,
    Gs = 8,
    A  = 9,
    As = 10,
    B  = 11
} Note;

typedef struct {
    Note note;
    unsigned short octave;
} MidiNote;

/****************************************************************************************
*
*
*   -> Main Struct Defs
****************************************************************************************/
typedef struct MM_Header
{
    size_t   length;
    uint16_t format;
    uint16_t ntrks;
    uint16_t ppqn;

} MM_Header;

typedef struct MM_MidiEvent
{
    uint64_t       delta_ticks, 
                   abs_ticks;

    MidiStatusCode status_code;
    _Byte          evt_data[2];
    MidiNote       note;

    // if a sequence is implied, such as NOTE ON / OFF pair,
    // use this to hook up related events
    struct MM_MidiEvent *next, *prev;

} MM_MidiEvent;

typedef struct MM_MidiTrack
{
    size_t          length;
    size_t          n_events;
    MM_MidiEvent       *event_arr;
    size_t          total_ticks,
                    total_beats;
} MM_MidiTrack;

/****************************************************************************************
*
*
*   -> Aux Data Structures -> For lookup / state edit
****************************************************************************************/
typedef struct MM_MidiEvent_LList_Node
{
    MM_MidiEvent *value;
    struct MM_MidiEvent_LList_Node *next;

} MM_MidiEvent_LList_Node;

typedef struct MM_MidiEvent_LList
{   
    size_t length;
    MM_MidiEvent_LList_Node *first, *last;
   
} MM_MidiEvent_LList;

int             MM_MidiEvent_LList_init         ( MM_MidiEvent_LList *self );
int             MM_MidiEvent_LList_destroy      ( MM_MidiEvent_LList *self );

int             MM_MidiEvent_LList_from_array   ( MM_MidiEvent_LList *list, MM_MidiEvent *array, size_t n_events );
void            MM_MidiEvent_LList__print_to_str( MM_MidiEvent_LList *list, char *output );







/****************************************************************************************
*
*
*   -> Main Exposed Structure -> Midi File
****************************************************************************************/
typedef struct MM_Midi_File
{
    char               *filepath;
    MM_Header           header;
    MM_MidiTrack        track;
    size_t              length;
    MM_MidiEvent_LList  events; // for use by UI

} MM_Midi_File;

int            MM_File_init                 ( MM_Midi_File *file, char *file_path );
void           MM_File_free                 ( MM_Midi_File *file );
unsigned short MM_File_get_bpm              ( MM_Midi_File *file );
int            MM_File_get_events_in_range  ( MM_Midi_File *file, MM_MidiEvent_LList *list, int start_ticks, int end_ticks, int start_note, int end_note );

/* MM Utils*/
double       MM_Util_tick_to_s( unsigned int ticks, unsigned short bpm, unsigned int ppqn);
unsigned int MM_Util_s_to_tick( double t_s, unsigned short bpm, unsigned int ppqn);

#endif /* MM_H */

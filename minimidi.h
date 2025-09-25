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

typedef struct MM_Event
{
    uint64_t       delta_ticks, 
                   abs_ticks;

    MidiStatusCode status_code;
    _Byte          evt_data[2];
    MidiNote       note;

    // if a sequence is implied, such as NOTE ON / OFF pair,
    // use this to hook up related events
    struct MM_Event *next, *prev;

} MM_Event;

typedef struct MM_Track
{
    size_t          length;
    size_t          n_events;
    MM_Event *event_arr;
    size_t          total_ticks,
                    total_beats;
} MM_Track;

/****************************************************************************************
*
*
*   -> Aux Data Structures -> For lookup / state edit
****************************************************************************************/
typedef struct MM_Event_LList_Node
{
    MM_Event *value;
    
    struct MM_Event_LList_Node *next;

} MM_Event_LList_Node;

typedef struct MM_Event_LList
{   
    size_t length;
    
    MM_Event_LList_Node *first,
        *last;
   
} MM_Event_LList;

MM_Event_LList *MM_Event_LList_init         ();
int             MM_Event_LList_destroy      ( MM_Event_LList *self );

int             MM_Event_LList_from_array   ( MM_Event_LList *list, MM_Event *array, size_t n_events );
void            MM_Event_LList__print_to_str( MM_Event_LList *list, char *output );
// void MM_Event_to_string_log( MM_Event *me, char *str );







/****************************************************************************************
*
*
*   -> Main Exposed Structure -> Midi File
****************************************************************************************/
typedef struct MM_File
{
    char            *filepath;
    MM_Header       *header;
    MM_Track        *track;
    unsigned short   bpm;
    size_t           length;
    MM_Event_LList  *events;

} MM_File;

MM_File       *MM_File_init                 ( char *file_path );
void           MM_File_free                 ( MM_File *file );
unsigned short MM_File_get_bpm              ( MM_File *file );
int            MM_File_get_event_at_s       ( MM_File *file, MM_Event_LList *container, double s, double delta_t );
int            MM_File_get_events_in_range  ( MM_File *file, MM_Event_LList *list, int start_ticks, int end_ticks, int start_note, int end_note );

#endif /* MM_H */

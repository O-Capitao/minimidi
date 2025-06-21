#ifndef MINIMIDI_H
#define MINIMIDI_H

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
typedef struct MiniMidi_Header
{
    size_t   length;
    uint16_t format;
    uint16_t ntrks;
    uint16_t ppqn;

} MiniMidi_Header;

typedef struct MiniMidi_Event
{
    uint64_t       delta_ticks, 
                   abs_ticks;

    MidiStatusCode status_code;
    _Byte          evt_data[2];
    MidiNote       note;

    // if a sequence is implied, such as NOTE ON / OFF pair,
    // use this to hook up related events
    struct MiniMidi_Event *next, *prev;

} MiniMidi_Event;

typedef struct MiniMidi_Track
{
    size_t          length;
    size_t          n_events;
    MiniMidi_Event *event_arr;
    size_t          total_ticks,
                    total_beats;
} MiniMidi_Track;



/****************************************************************************************
*
*
*   -> Main Exposed Structure -> Midi File
****************************************************************************************/
typedef struct MiniMidi_File
{
    char                 *filepath;
    MiniMidi_Header      *header;
    MiniMidi_Track       *track;
    size_t               length;

} MiniMidi_File;

MiniMidi_File       *MiniMidi_File_init( char *file_path );
// void                MiniMidi_File_print( MiniMidi_File *file );
void                MiniMidi_File_free( MiniMidi_File *file );


/****************************************************************************************
*
*
*   -> Aux Data Structures -> For lookup / state edit
****************************************************************************************/
typedef struct MiniMidi_Event_List_Node
{
    MiniMidi_Event *value;
    
    struct MiniMidi_Event_List_Node *next;

} MiniMidi_Event_List_Node;

typedef struct MiniMidi_Event_List
{   
    size_t length;
    
    MiniMidi_Event_List_Node *first,
        *last;
   
} MiniMidi_Event_List;

MiniMidi_Event_List      *MiniMidi_Event_LList_init();

int MiniMidi_get_events_in_range( MiniMidi_File *self, MiniMidi_Event_List *list, int start_ticks, int end_ticks, int start_note, int end_note );
// void MiniMidi_Event_to_string_log( MiniMidi_Event *me, char *str );

#endif /* MINIMIDI_H */

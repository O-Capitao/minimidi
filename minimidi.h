#ifndef MINIMIDI_H
#define MINIMIDI_H

#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>

#include "globals.h"

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

// Utility  Functions
// MidiStatusCode
MidiStatusCode get_midi_status_code( _Byte *byte);
void           print_midi_status_code(MidiStatusCode status);
uint8_t        get_midi_data_byte_count(MidiStatusCode status);

MidiNote       event_data_bytes_to_note( _Byte event_data_byte);
int            midi_note_to_int( MidiNote *note );


bool compare_MidiNote( MidiNote *n1, MidiNote *n2 );

void           print_midi_note(MidiNote note);

// Byte stuff utilities
void   reverse_byte_array(_Byte* arr, size_t len);
void   extract_number_from_byte_array( void* tgt, _Byte* src, size_t start_ind, size_t len );
void   get_substring( _Byte *src_str, _Byte* tgt_str, size_t start_index, size_t n_elements_to_copy, bool add_null_termination );
size_t read_VLQ_delta_t( _Byte *bytes, size_t len, uint64_t *val_ptr);

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



/**
 * MAIN STRUCTURE
 */
typedef struct MiniMidi_File
{
    char *filepath;
    MiniMidi_Header      *header;
    MiniMidi_Track       *track;
    size_t               length;

} MiniMidi_File;

void                parse_track_events( MiniMidi_Track *track, _Byte *evts_chunk );


MiniMidi_Header     *MiniMidi_Header_read(_Byte *file_contents );
void                MiniMidi_Header_print( MiniMidi_Header *mh );
void                MiniMidi_Header_free( MiniMidi_Header *header );

MiniMidi_Track      *MiniMidi_Track_read( _Byte *file_content, size_t start_index, size_t total_chunk_len );
void                MiniMidi_Track_print( MiniMidi_Track *mt );
void                MiniMidi_Track_free( MiniMidi_Track *track );
void                MiniMidi_Event_print( MiniMidi_Event *me );

MiniMidi_File       *MiniMidi_File_read_from_file( char *file_path );
void                MiniMidi_File_print( MiniMidi_File *file );
void                MiniMidi_File_free( MiniMidi_File *file );




// AUX STRUCTURES
// use this to pass items into outside
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

// MiniMidi_Event_List_Node *MiniMidi_Event_LList_Node_init(MiniMidi_Event *v);
MiniMidi_Event_List      *MiniMidi_Event_LList_init();

int MiniMidi_Event_List_destroy(MiniMidi_Event_List*self);
int MiniMidi_Event_List_append(MiniMidi_Event_List*self, MiniMidi_Event *v);
// query events
int MiniMidi_get_events_in_tick_range( MiniMidi_File *self, MiniMidi_Event_List *container, int start_ticks, int end_ticks );
int MiniMidi_Event_List_filter_octave_range( MiniMidi_Event_List *self, int start_oct, int end_oct );


#endif /* MINIMIDI_H */
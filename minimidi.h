#ifndef MINIMIDI_H
#define MINIMIDI_H

#include <stdlib.h>
#include <stdint.h>

#include "globals.h"

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
    uint16_t division;
} MiniMidi_Header;

typedef struct MiniMidi_Event
{
    uint64_t       delta_ticks;
    MidiStatusCode status_code;
    _Byte          evt_data[2];
    MidiNote       note;
} MiniMidi_Event;

typedef struct MiniMidi_Track
{
    size_t          length;
    size_t          n_events;
    MiniMidi_Event *event_arr;
} MiniMidi_Track;

typedef struct MiniMidi_File
{
    char *filepath;
    MiniMidi_Header     *header;
    MiniMidi_Track      *track;
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

MiniMidi_File       *MiniMidi_File_read_from_file( char *file_path ); //, int path_len );
MiniMidi_File       *MiniMidi_File_read( _Byte *file_contents, size_t file_len );
void                MiniMidi_File_print( MiniMidi_File *file );
void                MiniMidi_File_free( MiniMidi_File *file );


/****************************************************************************************
*
*
*   -> Renderer - NCurses
****************************************************************************************/
void ui_loop();

#endif /* MINIMIDI_H */
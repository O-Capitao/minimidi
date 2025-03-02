#ifndef MINIMIDI_H
#define MINIMIDI_H

#include "stdlib.h"
#include "stdint.h" // For uint8_t
# include "globals.h"



// Enum definition for MIDI Status Codes
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
    short octave;
} MidiNote;

typedef struct MidiFileHeaderChunk {
    unsigned char chunkType[5];
    __uint32_t length;
    __uint16_t format;
    __uint16_t ntrks;
    __uint16_t division;
} MidiFileHeaderChunk;

typedef struct MidiFileTrackEvent {
    unsigned long delta_ticks;
    MidiStatusCode status_code;
    // _Byte evt_code[2];
    _Byte evt_data[2];
} MidiFileTrackEvent;

typedef struct MidiFileTrackChunk {
    unsigned char chunkType[5];
    __uint32_t length;
    _Byte* trackBinData;
    size_t n_events;
    struct MidiFileTrackEvent *event_arr;
} MidiFileTrackChunk;


MidiStatusCode getMidiStatusCode( _Byte *byte);
void printMidiStatusCode(MidiStatusCode status);
uint8_t getMidiDataByteCount(MidiStatusCode status);
MidiNote eventDataBytesToNote( _Byte eventDataByte);

// utility
void reverseByteArray(_Byte* arr, int len);
void extractNumberFromByteArray( void* tgt, _Byte* src, int start_ind, int len );
void getSubstring(_Byte *src_str, unsigned char* tgt_str, int start_index, int n_elements_to_copy, bool add_null_termination );
size_t readVLQ_DeltaT( _Byte *bytes, size_t len, unsigned long *val_ptr);
void parseEvents( /* struct MidiFileTrackEvent *evts,*/ _Byte *evts_chunk, size_t chunk_len  );

MidiFileHeaderChunk readHeaderChunk( _Byte *fileContents );
MidiFileTrackChunk readTrackChunk( _Byte *fileContent, size_t startIndex, size_t totalLen );


#endif /* MINIMIDI_H */
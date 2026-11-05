#ifndef MM_H
#define MM_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define MIDI_EVENTS_BUFFER_SIZE 128

typedef enum {
    MIDI_NOTE_OFF        = 0x80,
    MIDI_NOTE_ON         = 0x90,
    MIDI_POLY_AFTERTOUCH = 0xA0,
    MIDI_CONTROL_CHANGE  = 0xB0,
    MIDI_PROGRAM_CHANGE  = 0xC0,
    MIDI_CHAN_AFTERTOUCH = 0xD0,
    MIDI_PITCH_BEND      = 0xE0,
    MIDI_SYSTEM          = 0xF0,
    MIDI_INVALID         = 0x00
} MidiStatusCode;

typedef enum Note { C = 0, Cs, D, Ds, E, F, Fs, G, Gs, A, As, B } Note;

typedef struct {
    Note note;
    int8_t octave;
} MidiNote;

typedef struct MM_Header {
    uint32_t length;
    uint16_t format;
    uint16_t ntrks;
    uint16_t ppqn;
} MM_Header;

typedef struct MM_MidiEvent {
    uint64_t delta_ticks;
    uint64_t abs_ticks;
    MidiStatusCode status_code;
    uint8_t channel;
    uint8_t note_number;
    uint8_t evt_data[2];
    MidiNote note;
    struct MM_MidiEvent *next;
    struct MM_MidiEvent *prev;
} MM_MidiEvent;

typedef struct MM_MidiTrack {
    size_t length;
    size_t n_events;
    MM_MidiEvent *event_arr;
    uint64_t total_ticks;
    double total_beats;
} MM_MidiTrack;

typedef struct MM_MidiEvent_LList_Node {
    MM_MidiEvent *value;
    struct MM_MidiEvent_LList_Node *next;
} MM_MidiEvent_LList_Node;

typedef struct MM_MidiEvent_LList {
    size_t length;
    MM_MidiEvent_LList_Node *first;
    MM_MidiEvent_LList_Node *last;
} MM_MidiEvent_LList;

typedef struct MM_Midi_File {
    char *filepath;
    MM_Header header;
    MM_MidiTrack track; /* all SMF tracks flattened and sorted by tick */
    size_t length;
    MM_MidiEvent_LList events;
} MM_Midi_File;

int MM_File_init(MM_Midi_File *file, const char *file_path);
void MM_File_free(MM_Midi_File *file);
int MM_File_get_events_in_range(MM_Midi_File *file,
                                MM_MidiEvent_LList *list,
                                uint64_t start_ticks,
                                uint64_t end_ticks,
                                int start_note,
                                int end_note);

int MM_MidiEvent_LList_init(MM_MidiEvent_LList *self);
int MM_MidiEvent_LList_destroy(MM_MidiEvent_LList *self);
int MM_MidiEvent_LList_from_array(MM_MidiEvent_LList *list,
                                  MM_MidiEvent *array,
                                  size_t n_events);

double MM_Util_tick_to_s(uint64_t ticks, unsigned int bpm, unsigned int ppqn);
uint64_t MM_Util_s_to_tick(double seconds, unsigned int bpm, unsigned int ppqn);

#endif

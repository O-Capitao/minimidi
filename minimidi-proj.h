#ifndef MM_PROJ_H
#define MM_PROJ_H

#include "uthash.h"
#include "minimidi-proj-file.h"
#include "minimidi.h"

typedef struct {
    char *key;
    MM_Midi_File value;
    UT_hash_handle hh;
} MM_Mid_Map_Entry;

typedef struct {
    MM_File_TrackConfig *config_file;
    double gain;

    MM_Midi_File *active_file;
    MM_MidiEvent *curr_evt, *next_evt;

} MM_Track;

typedef struct {
    unsigned int length_beats;
    unsigned int loop_count;
} MM_Sequence;

typedef struct {

    unsigned int ppqn;
    MM_File_Project *file;

    MM_Mid_Map_Entry *midi_map;  // head of the hash table

    MM_Sequence *sequence_arr;
    size_t n_sequences;

    MM_Track *tracks_arr;
    size_t n_tracks;

} MM_Project;

/**
 * PUBLIC
 */
int MM_Project_init( MM_Project *p, const char* filepath );
void MM_Project_free( MM_Project *p );

#endif
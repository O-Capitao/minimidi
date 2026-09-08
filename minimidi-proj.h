#ifndef MM_PROJ_H
#define MM_PROJ_H

#include "minimidi-proj-file.h"
#include "minimidi.h"
#include "uthash.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct MM_Mid_Map_Entry {
    char *key;
    MM_Midi_File value;
    UT_hash_handle hh;
} MM_Mid_Map_Entry;

typedef struct MM_Track {
    MM_File_TrackConfig *config_file;
    const char *name;
    MM_WaveType wave;
    double gain;
} MM_Track;

typedef struct MM_Clip {
    MM_Midi_File *midi;
    uint64_t start_tick;
    uint64_t duration_ticks;
    uint64_t end_tick;
    uint64_t source_duration_ticks; /* rescaled to project PPQN */
    bool loop;
} MM_Clip;

typedef struct MM_SequenceTrack {
    MM_Clip *clips;
    size_t n_clips;
} MM_SequenceTrack;

typedef struct MM_Sequence {
    const char *name;
    uint64_t length_ticks;
    double length_beats;
    unsigned int n_repeats; /* total plays; zero means infinite */
    MM_SequenceTrack *tracks; /* n_tracks entries */
} MM_Sequence;

typedef struct MM_Project {
    unsigned int ppqn;
    MM_File_Project *file;
    MM_Mid_Map_Entry *midi_map;
    MM_Sequence *sequence_arr;
    size_t n_sequences;
    MM_Track *tracks_arr;
    size_t n_tracks;
} MM_Project;

int MM_Project_init(MM_Project *project, const char *filepath);
void MM_Project_free(MM_Project *project);

#endif

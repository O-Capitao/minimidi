#ifndef MM_FILE_H
#define MM_FILE_H

#include <stdbool.h>
#include <stddef.h>
#include <yaml.h>

typedef enum {
    MM_WAVE_SQUARE = 0,
    MM_WAVE_TRIANGLE,
    MM_WAVE_SIN,
} MM_WaveType;

const char* mm_wave_to_str(MM_WaveType type);
MM_WaveType mm_str_to_wave(const char* str);

typedef struct MM_File_Sequence_Track_Midi_Assignment {
    char* midi_path;
    char* start;
    bool loop;
    char* length;
} MM_File_Sequence_Track_Midi_Assignment;

typedef struct MM_File_Sequence_Track {
    struct MM_File_TrackConfig *config;
    char* track_name; // temporary during parsing, freed after linking
    MM_File_Sequence_Track_Midi_Assignment* assignments;
    size_t num_assignments;
} MM_File_Sequence_Track;

typedef struct MM_File_Sequence {
    char* name;
    char* length;
    int loop_count; // default 1 if omitted
    MM_File_Sequence_Track* tracks;
    size_t num_tracks;
} MM_File_Sequence;

typedef struct MM_File_TrackConfig {
    char* name;
    MM_WaveType wave;
    double gain;
} MM_File_TrackConfig;

typedef struct MM_File_Project {
    char* name;
    unsigned int tempo;
    unsigned int sample_rate;
    unsigned int ppqn;
    unsigned int beat_per_bar;
    MM_File_TrackConfig* track_configs;
    size_t num_track_configs;
    MM_File_Sequence* sequences;
    size_t num_sequences;
} MM_File_Project;

int  MM_Proj_File_read ( const char* filepath, MM_File_Project* project );
void MM_Proj_File_free ( MM_File_Project* project );
int  MM_Proj_File_write( const MM_File_Project* project, const char* filepath );

#endif
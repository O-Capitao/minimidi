#ifndef MM_PROJ_FILE_H
#define MM_PROJ_FILE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define MM_MAX_TRACKS 4

typedef enum {
    MM_WAVE_SQUARE = 0,
    MM_WAVE_TRIANGLE,
    MM_WAVE_SIN
} MM_WaveType;

const char *mm_wave_to_str(MM_WaveType type);
bool mm_wave_from_str(const char *text, MM_WaveType *type);

typedef struct MM_File_Sequence_Track_Midi_Assignment {
    char *midi_path;          /* spelling used in YAML */
    char *resolved_midi_path; /* relative to the project file */
    char *start;
    uint64_t start_ticks;
    double start_beats;
    bool loop;
    char *length;             /* NULL means natural MIDI duration */
    bool has_length;
    uint64_t length_ticks;
    double length_beats;
} MM_File_Sequence_Track_Midi_Assignment;

typedef struct MM_File_TrackConfig {
    char *name;
    MM_WaveType wave;
    double gain;
} MM_File_TrackConfig;

typedef struct MM_File_Sequence_Track {
    MM_File_TrackConfig *config;
    char *track_name;
    MM_File_Sequence_Track_Midi_Assignment *assignments;
    size_t num_assignments;
} MM_File_Sequence_Track;

typedef struct MM_File_Sequence {
    char *name;
    char *length;
    uint64_t length_ticks;
    double length_beats;
    unsigned int n_repeats; /* total plays; zero means infinite */
    MM_File_Sequence_Track *tracks;
    size_t num_tracks;
} MM_File_Sequence;

typedef struct MM_File_Project {
    char *name;
    char *filepath;
    unsigned int tempo;
    unsigned int sample_rate;
    unsigned int ppqn;
    unsigned int beat_per_bar;
    MM_File_TrackConfig *track_configs;
    size_t num_track_configs;
    MM_File_Sequence *sequences;
    size_t num_sequences;
} MM_File_Project;

int MM_Proj_File_parse_time(const char *text, unsigned int beats_per_bar,
                            unsigned int ppqn, uint64_t *ticks, double *beats);
int MM_Proj_File_read(const char *filepath, MM_File_Project *project);
void MM_Proj_File_free(MM_File_Project *project);
int MM_Proj_File_write(const MM_File_Project *project, const char *filepath);

#endif

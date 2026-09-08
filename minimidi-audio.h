#ifndef MM_AUDIO_H
#define MM_AUDIO_H

#include "minimidi-proj.h"
#include "minimidi-rb.h"
#include "minimidi-transport.h"

#include <portaudio.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define N_CHANNELS 1

typedef struct MM_Synth {
    double phase;
    double sample_rate;
    uint8_t active_note;
    bool note_on;
    MM_WaveType wave;
} MM_Synth;

typedef struct MM_AudioTrack {
    double gain;
    MM_Synth synth;
    MM_SequenceTrack *arrangement;
    size_t clip_index;
    size_t event_index;
    uint64_t cycle_index;
    const MM_Clip *active_clip;
} MM_AudioTrack;

typedef struct MM_AudioEngine {
    MM_Project *project;
    size_t n_tracks;
    MM_AudioTrack *tracks_arr;
    MM_Ring_Buffer *cmd_queue;
    PaStream *pa_st;
    bool pa_initialized;
    bool playing;
    bool finished;
    uint64_t total_samples;
    uint64_t sequence_samples;
    uint64_t sequence_length_samples;
    size_t active_sequence_index;
    unsigned int active_sequence_play;
    _Atomic(double) posted_audio_time;
    _Atomic(size_t) posted_sequence_index;
    _Atomic(uint64_t) posted_sequence_tick;
    _Atomic(bool) posted_playing;
} MM_AudioEngine;

int MM_AudioEngine_init(MM_AudioEngine *engine, MM_Project *project,
                        MM_Ring_Buffer *cmd_queue);
int MM_AudioEngine_init_offline(MM_AudioEngine *engine, MM_Project *project,
                                MM_Ring_Buffer *cmd_queue);
void MM_AudioEngine_render(MM_AudioEngine *engine, float *output,
                           size_t frames);
int MM_AudioEngine_destroy(MM_AudioEngine *engine);

#endif

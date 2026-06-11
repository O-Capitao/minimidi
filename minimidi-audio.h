#ifndef MM_TUI_AUDIO
#define MM_TUI_AUDIO

#include <stddef.h>
#include <math.h>
#include <ncurses.h>
#include <stdbool.h>
#include <assert.h>
#include <portaudio.h>
#include <stdatomic.h>     // C11

#include "minimidi-log.h"
#include "minimidi-rb.h"
#include "minimidi.h"
#include "minimidi-transport.h"
#include "minimidi-proj.h"
// #include "minimidi-project.h"

#define DEBUG 0

#define OSCILATORS_MAX 10
#define AUDIO_FRAMERATE 44000
#define N_CHANNELS 1
#define NOTE_RANGE 96
#define MAX_SIMULT_MIDI_EVENTS 3
/***
*  * MiniMidi Audio
* 
*   simple synth
*   to play the midi
*/
typedef struct {
    float theta,
        dtheta,
        phase,
        amp;
    bool is_active;
} MM_Oscillator;

int MM_Oscillator_produce( MM_Oscillator *self, float *output, size_t output_size );

typedef struct {
    MM_Oscillator oscillators[OSCILATORS_MAX];
    int n_oscillators;
    // MM_MidiEvent_LList *midi_evts;
    float tempered_freqs[NOTE_RANGE];

    MidiNote *active_note;
    bool     note_on;
} MM_Synth;

void MM_Synth_init(MM_Synth *self);
void MM_Synth_note_on (MM_Synth *self, MidiNote *note);
void MM_Synth_note_off(MM_Synth *self, MidiNote *note);

float MM_Synth_next_sample(MM_Synth *s, double t);

typedef struct {
    double gain;
    MM_Midi_File *active_midi_file;

    MM_Synth synth;

    MM_MidiEvent_LList_Node *active_node;
    MM_MidiEvent_LList_Node *nxt_node;

} MM_AudioTrack;

int MM_AudioTrack_init(         MM_AudioTrack *self, MM_Track *track_config );
int MM_AudioTrack_load(         MM_AudioTrack *self, MM_Midi_File *midi );
double MM_AudioTrack_produce(   MM_AudioTrack *self, double t_s );

typedef struct {

    MM_Project *project;
    
    size_t n_tracks;
    MM_AudioTrack *tracks_arr;


    double total_t;
    double delta_t;
    
    // UI → audio commands
    MM_Ring_Buffer *cmd_queue;
    PaStream *pa_st;

    bool playing;
    bool in_infinite_loop;


    double sequence_t;
    double next_sequence_break_t;
    MM_Sequence *active_sequence;
    unsigned int active_sequence_index;
    unsigned int active_sequence_loop_counter;

    // inform UI of what time we at
    _Atomic(double) posted_audio_time;

} MM_AudioEngine;

int MM_AudioEngine_init( MM_AudioEngine *self, MM_Project *project, MM_Ring_Buffer *cmd_queue );
int MM_AudioEngine_destroy( MM_AudioEngine *self );

#endif // MM_TUI_AUDIO
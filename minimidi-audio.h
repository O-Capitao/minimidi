#ifndef MM_TUI_AUDIO
#define MM_TUI_AUDIO

#include <ncurses.h>
#include <stdbool.h>
#include <assert.h>
#include <portaudio.h>

#include "minimidi-log.h"
#include "minimidi-rb.h"
#include "minimidi.h"
#include "minimidi-transport.h"

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
typedef struct MM_Oscillator {
    float theta,
        dtheta,
        phase,
        amp;
    bool is_active;
} MM_Oscillator;

int MM_Oscillator_produce( MM_Oscillator *self, float *output, size_t output_size );

typedef struct MM_Synth {
    MM_Oscillator oscillators[OSCILATORS_MAX];
    int n_oscillators;
    // MM_Event_LList *midi_evts;
    float tempered_freqs[NOTE_RANGE];

    MidiNote *active_note;
    bool     note_on;
} MM_Synth;

void MM_Synth_init(MM_Synth *self);
void MM_Synth_note_on (MM_Synth *self, MidiNote *note);
void MM_Synth_note_off(MM_Synth *self, MidiNote *note);

float MM_Synth_next_sample(MM_Synth *s, double t);


typedef struct MM_AudioEngine {
    MM_Synth synth;
    MM_File *midi_file;
    MM_Event *nxt_evt;
    double sample_rate;
    double audio_time;
    double delta_t;
    unsigned int bpm;
    bool playing;
    MM_Ring_Buffer *cmd_queue;   // UI → audio commands
    PaStream *pa_st;
} MM_AudioEngine;


int MM_AudioEngine_init( MM_AudioEngine *self, MM_Ring_Buffer *cmd_queue, MM_File *file );
int MM_AudioEngine_destroy( MM_AudioEngine *self );

#endif // MM_TUI_AUDIO
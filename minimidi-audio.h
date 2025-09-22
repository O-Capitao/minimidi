#ifndef MINIMIDI_TUI_AUDIO
#define MINIMIDI_TUI_AUDIO

#include <ncurses.h>
#include <stdbool.h>
#include <assert.h>
#include <portaudio.h>

#include "minimidi-log.h"
#include "minimidi-ring-buffer.h"
#include "minimidi.h"

#define DEBUG 0

#define OSCILATORS_MAX 10
// #define BUFF_SIZE 512
#define AUDIO_FRAMERATE 44000
#define N_CHANNELS 2
#define NOTE_RANGE 96
/***
*  * MiniMidi Audio
* 
*   simple synth
*   to play the midi
*/
typedef struct MiniMidi_Oscillator {
    float theta,
        dtheta,
        phase,
        amp;
    bool is_active;
} MiniMidi_Oscillator;

int MiniMidi_Oscillator_produce( MiniMidi_Oscillator *self, float *output, size_t output_size );

typedef struct MiniMidi_Synth {
    
    MiniMidi_Oscillator oscillators[ OSCILATORS_MAX ];
    int n_oscilators;
    double tempered_freqs[NOTE_RANGE];
    float last_processed_evt_time,
        current_processing_time,
        delta_t;

    MM_Ring_Buffer *rb;
    MiniMidi_Event *midi_evts_arr;

    PaStream *pa_stream;
} MiniMidi_Synth;

MiniMidi_Synth *MiniMidi_Synth_init( MiniMidi_Event *track_events );
int MiniMidi_Synth_destroy( MiniMidi_Synth *self );
int MiniMidi_Synth_step( MiniMidi_Synth *self );
int MiniMidi_Synth_play( MiniMidi_Synth *self );
int MiniMidi_Synth_stop( MiniMidi_Synth *self );

#endif // MINIMIDI_TUI_AUDIO
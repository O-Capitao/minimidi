#ifndef MM_TUI_AUDIO
#define MM_TUI_AUDIO

#include <ncurses.h>
#include <stdbool.h>
#include <assert.h>
#include <portaudio.h>

#include "minimidi-log.h"
#include "minimidi-ring-buffer.h"
#include "minimidi.h"

#define DEBUG 0

#define OSCILATORS_MAX 10
#define AUDIO_FRAMERATE 44000
#define N_CHANNELS 1
#define NOTE_RANGE 96
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
    
    MM_Oscillator oscillators[ OSCILATORS_MAX ];
    int n_oscilators;
    double tempered_freqs[NOTE_RANGE];

    // synth time in seconds
    double t, delta_t;

    MM_Ring_Buffer *rb;
    MM_Event *midi_evts_arr;

    PaStream *pa_stream;

    bool is_playing;
} MM_Synth;

MM_Synth *MM_Synth_init( MM_Event *track_events );
int MM_Synth_destroy( MM_Synth *self );
int MM_Synth_step( MM_Synth *self );
// int MM_Synth_play( MM_Synth *self );
// int MM_Synth_stop( MM_Synth *self );

#endif // MM_TUI_AUDIO
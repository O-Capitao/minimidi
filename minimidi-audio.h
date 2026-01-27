#ifndef MM_TUI_AUDIO
#define MM_TUI_AUDIO

#include <ncurses.h>
#include <stdbool.h>
#include <assert.h>
#include <portaudio.h>

#include "minimidi-log.h"
#include "minimidi-rb.h"
#include "minimidi.h"

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
    
    MM_Oscillator oscillators[ OSCILATORS_MAX ];
    int n_oscilators;
    float tempered_freqs[NOTE_RANGE];
    
    // synth time in seconds
    float t, delta_t;

    MM_Ring_Buffer *rb;
    PaStream *pa_stream;
    MidiNote *active_note;

    bool is_playing;

} MM_Synth;

MM_Synth *MM_Synth_init         ();
int       MM_Synth_destroy      ( MM_Synth *self );
int       MM_Synth_press_key    ( MM_Synth *s, MidiNote *n );
int       MM_Synth_release_key  ( MM_Synth *s, MidiNote *n );
int       MM_Synth_step         ( MM_Synth *self );

#endif // MM_TUI_AUDIO
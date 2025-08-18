#ifndef MINIMIDI_TUI_AUDIO
#define MINIMIDI_TUI_AUDIO

#include <ncurses.h>
#include <stdbool.h>
#include <assert.h>
#include <portaudio.h>

#include "minimidi-log.h"
#include "minimidi-ring-buffer.h"

#define DEBUG 0

#define OSCILATORS_MAX 10
#define BUFF_SIZE 512
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

typedef enum MiniMidi_Synth_Event_Type {
    SYNTH_KEY_PRESSED,
    SYNTH_KEY_RELEASED
} MiniMidi_Synth_Event_Type;

typedef struct MiniMidi_Synth_Event {
    float time_s;
    MiniMidi_Synth_Event_Type type;
} MiniMidi_Synth_Event;

typedef struct MiniMidi_Synth {
    
    MiniMidi_Oscillator oscillators[ OSCILATORS_MAX ];
    int n_oscilators;
    double tempered_freqs[NOTE_RANGE];
    float last_processed_evt_time,
        current_processing_time;

    MiniMidi_Ring_Buffer *rb;

    PaStream *pa_stream;
} MiniMidi_Synth;

MiniMidi_Synth *MiniMidi_Synth_init();
int MiniMidi_Synth_destroy( MiniMidi_Synth *self );

/**
 * The UI context will produce a sequence of MiniMidi_Synth_Event items
 * which will then be consumed and turned into floats for the audio buffer
 */
int MiniMidi_Synth_step( MiniMidi_Synth *self, MiniMidi_Synth_Event *cmd_buffer, int cmd_buffer_size );

int MiniMidi_Synth_play( MiniMidi_Synth *self );
int MiniMidi_Synth_stop( MiniMidi_Synth *self );

#endif /* MINIMIDI_TUI_AUDIO */
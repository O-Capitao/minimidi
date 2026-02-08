#include "minimidi-audio.h"
#include "minimidi-log.h"

#include <math.h>
#include <stdlib.h>

#define PI 3.14159265358979323846
#define BUFFER_SIZE 4096

// note_i is the number of the note, starting from C0
float _calc_tempered_freq( int note_i ){
    return 440.0 * pow( 2, ((float)note_i - 57.0) / 12.0 );
}

static int paStreamCallback( const void *inputBuffer,
                            void *outputBuffer,
                            unsigned long frames_per_buffer,
                            const PaStreamCallbackTimeInfo* timeInfo,
                            PaStreamCallbackFlags statusFlags,
                            void *_my_data ){

    // MM_Ring_Buffer *rb = (MM_Ring_Buffer*)_my_data;
    MM_Ring_Buffer *rb = (MM_Ring_Buffer *)_my_data;
    float *out = (float*)outputBuffer;
    (void) inputBuffer;

    MM_Ring_Buffer__pop_n( rb , out, frames_per_buffer );

    return 0;
}

MM_Synth *MM_Synth_init( MM_Event *track_events, size_t total_events ){

    log_debug("minimidi-audio.c > MM_Synth_init : entering.");

    MM_Synth* s = (MM_Synth*)malloc(sizeof(MM_Synth));

    // init memory
    s->n_oscilators = 1;
    s->t = 0;
    s->delta_t = 1.00 / (float)AUDIO_FRAMERATE;
    s->rb = MM_Ring_Buffer__init( BUFFER_SIZE );

    // init Portaudio
    if ( Pa_Initialize() != paNoError){    
        return NULL;
    }

    // dump PortAudio datagem,


    // open stream
    PaError e = Pa_OpenDefaultStream(
        &(s->pa_stream),
        0,
        N_CHANNELS,
        paFloat32,
        AUDIO_FRAMERATE,
        BUFFER_SIZE,
        paStreamCallback,
        s->rb
    );

    if (e != paNoError){
        return NULL;
    }

    e = Pa_StartStream(s->pa_stream);

    if (e != paNoError){
        return NULL;
    }

    // init note freqs
    for (int i = 0; i < NOTE_RANGE; i++){
        s->tempered_freqs[i] = _calc_tempered_freq( i );
    }

    // init oscillators
    // add multiple oscillator supp.
    for (int i = 0; i < s->n_oscilators; i++){
        s->oscillators[i].amp = 0.5;
        s->oscillators[i].theta = 0;
        s->oscillators[i].phase = 0;
        s->oscillators[i].is_active = false;
        s->oscillators[i].dtheta = (2* PI ) / ((float) AUDIO_FRAMERATE);
    }

    s->is_playing = false;
    s->active_note = NULL;


    const PaStreamInfo *sInfo = Pa_GetStreamInfo(s->pa_stream);
    if (!sInfo) {
        log_error("Error: Could not retrieve stream info.");
        return 0;
    }

    // Identify the device used by the stream (assuming output here)
    // Note: You must track which device index you used to open the stream
    PaDeviceIndex outDev = Pa_GetDefaultOutputDevice(); 
    const PaDeviceInfo *dInfo = Pa_GetDeviceInfo(outDev);
    const PaHostApiInfo *hInfo = Pa_GetHostApiInfo(dInfo->hostApi);

    log_info(
             "--- Diagnostic Data ---\n"
             "Device Name: %s\n"
             "Host API:    %s\n"
             "Sample Rate: %.0f Hz (Actual)\n"
             "Out Latency: %.4f ms\n"
             "In Latency:  %.4f ms",
             dInfo->name,
             hInfo->name,
             sInfo->sampleRate,
             sInfo->outputLatency * 1000.0,
             sInfo->inputLatency * 1000.0);

    log_debug("minimidi-audio.c > MM_Synth_init : exiting.");

    return s;
}



int MM_Synth_destroy( MM_Synth *s ){

    // todo: reenable after start / stop logic is in
    PaError e = Pa_StopStream(s->pa_stream);

    if (e != paNoError){
        return 1;
    }

    e = Pa_CloseStream(s->pa_stream);

    if (e != paNoError){
        return 1;
    }

    s->pa_stream = NULL;
    free(s);

    return 0; 
}

float _produce_val( MM_Synth *s ){
    if (s->active_note && s->is_playing){
        float freq = s->tempered_freqs[ 12 * s->active_note->octave + (int)s->active_note->note ];
        float period = 1.0 / freq;
        float t_in_period = fmodf( s->t, period );
        float retval = t_in_period / period > 0.5 ? 0 : 0.3333;

        return retval;
    }
    return 0;
}
// fill the output_arr with zeros
//
int _produce_values( MM_Synth *s, size_t n_to_produce, float *output_arr ){

    log_trace("minimidi-audio.c > _produce_values > producing %li values.", n_to_produce);

    float _val;
    for (size_t i = 0; i < n_to_produce; i++){
        _val = _produce_val( s );
        output_arr[i] = _val;
        // update state
        s->t += s->delta_t;
    }
    return 0;
}

// aux
float _SYNTH_BUFFER[BUFFER_SIZE];

int MM_Synth_step( MM_Synth *s ){

    // write to buffer
    size_t _space_in_buffer = MM_Ring_Buffer__get_free_space( s->rb );
    log_debug("minimidi-audio.c > MM_Synth_step > entering, free space is %li", _space_in_buffer);

    if (_space_in_buffer){

        assert(_space_in_buffer <= BUFFER_SIZE);

        _produce_values( s, _space_in_buffer, _SYNTH_BUFFER );
        
        for (int i = 0; i < _space_in_buffer; i+=100) {
            log_debug("_SYNTH_BUFFER[%d] = %f", i, _SYNTH_BUFFER[i]);
        }

        MM_Ring_Buffer__push_n(s->rb, _SYNTH_BUFFER, _space_in_buffer);
    }

    return 0;
}

int MM_Synth_press_key( MM_Synth *s, MidiNote *n ){
    if (!s->active_note){
        s->active_note = n;
        return 0;
    }
    if (n->note != s->active_note->note || n->octave != s->active_note->octave ){
        s->active_note = n;
    }

    return 0;
}

int MM_Synth_release_key( MM_Synth *s, MidiNote *n ){
    // if (n->note == s->active_note->note || n->octave != s->active_note->octave ){
        s->active_note = NULL;
    // }
    return 0;
}

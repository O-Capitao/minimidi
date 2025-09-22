#include "minimidi-audio.h"
#include "minimidi-log.h"

#include <math.h>
#include <stdlib.h>

#define PI 3.14159265358979323846
#define BUFFER_SIZE 512

// note_i is the number of the note, starting from C0
double _calc_tempered_freq( int note_i ){
    return 440.0 * pow( 2, ((double)note_i - 57.0) / 12.0 );
}

static int paStreamCallback( const void *inputBuffer,
                            void *outputBuffer,
                            unsigned long frames_per_buffer,
                            const PaStreamCallbackTimeInfo* timeInfo,
                            PaStreamCallbackFlags statusFlags,
                            void *_my_data ){

    // MiniMidi_Ring_Buffer *rb = (MiniMidi_Ring_Buffer*)_my_data;
    MM_Ring_Buffer *rb = (MM_Ring_Buffer *)_my_data;
    float *out = (float*)outputBuffer;
    (void) inputBuffer;

    sprintf(MiniMidi_Log_log_line, "callback");
    MiniMidi_Log_writeline();

    MM_Ring_Buffer__pop_n( rb , out, frames_per_buffer );

    return 0;
}

MiniMidi_Synth *MiniMidi_Synth_init( MiniMidi_Event *track_events ){

    sprintf( MiniMidi_Log_log_line, "minimidi-audio.c > MiniMidi_Synth_init : entering.");
    MiniMidi_Log_writeline();

    MiniMidi_Synth* s = (MiniMidi_Synth*)malloc(sizeof(MiniMidi_Synth));

    // init memory
    s->n_oscilators = 1;
    s->last_processed_evt_time = s->current_processing_time = 0;
    s->delta_t = 1 /AUDIO_FRAMERATE;
    s->rb = MM_Ring_Buffer__init( sizeof(float), BUFFER_SIZE );

    // init Portaudio
    if ( Pa_Initialize() != paNoError){    
        return NULL;
    }

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
    // TODO:
    // add multiple oscillator supp.
    for (int i = 0; i < s->n_oscilators; i++){
        s->oscillators[i].amp = 0.5;
        s->oscillators[i].theta = 0;
        s->oscillators[i].phase = 0;
        s->oscillators[i].is_active = false;
        s->oscillators[i].dtheta = (2* PI ) / ((float) AUDIO_FRAMERATE);
    }

    s->midi_evts_arr = track_events;

    sprintf( MiniMidi_Log_log_line, "minimidi-audio.c > MiniMidi_Synth_init : exiting.");
    MiniMidi_Log_writeline();

    return s;
}



int MiniMidi_Synth_destroy( MiniMidi_Synth *s ){
    
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

float _produce_val( float t ){
    
    float freq = 440 * 2;
    float period = 1 / freq;
    float t_in_period = fmodf( t, period ); 

    return t_in_period / period > 0.5 ? 0 : 0.3333;
}
// start from the current time, produce until whenever 
int _produce_values( MiniMidi_Synth *s, size_t n_to_produce, float *output_arr ){
    for (size_t i = 0; i < n_to_produce; i++){
        output_arr[i] = _produce_val( s->current_processing_time );
        s->current_processing_time += s->delta_t;
    }
    return 0;
}

int MiniMidi_Synth_step( MiniMidi_Synth *s ){
    size_t _space_in_buffer = MM_Ring_Buffer__get_free_space( s->rb );
    sprintf(MiniMidi_Log_log_line, "minimidi-audio.c > MiniMidi_Synth_step > entering, free space is %li", _space_in_buffer);
    MiniMidi_Log_writeline();

    if (_space_in_buffer > BUFFER_SIZE / 2){

        if (_space_in_buffer > BUFFER_SIZE){
            sprintf(MiniMidi_Log_log_line, "minimidi-audio.c > MiniMidi_Synth_step > oops");
            MiniMidi_Log_writeline();
        }
        // EXPENSIVE!
        float lilbuff[_space_in_buffer];
        _produce_values( s, _space_in_buffer, lilbuff );
        MM_Ring_Buffer__push_n(s->rb, lilbuff, _space_in_buffer);
        
        sprintf(MiniMidi_Log_log_line, "minimidi-audio.c > MiniMidi_Synth_step > dumping into ring buffer");
        MiniMidi_Log_writeline();
    }

    return 0;
}


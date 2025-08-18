#include "minimidi-audio.h"
#include "minimidi-log.h"
#include "math.h"
#include <stdlib.h>

#define PI 3.14159265358979323846

// note_i is the number of the note, starting from C0
double _calc_tempered_freq( int note_i ){
    return 440.0 * pow( 2, ((double)note_i - 57.0) / 12.0 );
}

static int paStreamCallback( const void *inputBuffer, void *outputBuffer,
                             unsigned long frames_per_buffer,
                             const PaStreamCallbackTimeInfo* timeInfo,
                             PaStreamCallbackFlags statusFlags,
                             void *_my_data ){

    MiniMidi_Ring_Buffer *rb = (MiniMidi_Ring_Buffer*)_my_data;
    float *out = (float*)outputBuffer;
    (void) inputBuffer;

    MiniMidi_Ring_Buffer__pop_n( rb, out, frames_per_buffer );

    return 0;
}



MiniMidi_Synth *MiniMidi_Synth_init(){

    sprintf( MiniMidi_Log_log_line, "minimidi-audio.c > MiniMidi_Synth_init : entering.");
    MiniMidi_Log_writeline();

    MiniMidi_Synth* s = (MiniMidi_Synth*)malloc(sizeof(MiniMidi_Synth));

    // init memory
    s->n_oscilators = 1;
    s->last_processed_evt_time = s->current_processing_time = 0;
    s->rb = MiniMidi_Ring_Buffer__init();

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
        BUFF_SIZE,
        paStreamCallback,
        s->rb
    );


    if (e != paNoError){
        return NULL;
    }

    // // start stream
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

    sprintf( MiniMidi_Log_log_line, "minimidi-audio.c > MiniMidi_Synth_init : exiting.");
    MiniMidi_Log_writeline();

    return s;
}



int MiniMidi_Synth_destroy( MiniMidi_Synth *s ){
    
    // todo: reenable after start / stop logic is in
    PaError e = Pa_StopStream(s->pa_stream);
    // PaError e = paNoError;

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

int MiniMidi_Synth_step( MiniMidi_Synth *s, MiniMidi_Synth_Event *cmd_buffer, int n_commands ){
    
    MiniMidi_Synth_Event *evt_cursor;
    // get / process incoming commands
    for (int i = 0; i < n_commands; i++) {
        evt_cursor = &(cmd_buffer[i]);
        
    
    }

    
    return 0;
}

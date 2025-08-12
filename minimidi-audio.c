#include "minimidi-audio.h"




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



int MiniMidi_Synth_init( MiniMidi_Synth *s ){

    // init memory
    s->n_oscilators = 0;
    s->rb = MiniMidi_Ring_Buffer__init();

    // init Portaudio
    if ( Pa_Initialize() != paNoError){
        // fail
        return 1;
    }

    // open stream
    PaError e = Pa_OpenDefaultStream(
        &s->pa_stream,
        0,
        N_CHANNELS,
        paFloat32,
        AUDIO_FRAMERATE,
        BUFF_SIZE,
        paStreamCallback,
        s->rb
    );


    if (e != paNoError){
        return 1;
    }

    // start stream
    e = Pa_StartStream(s->pa_stream);

    if (e != paNoError){
        return 1;
    }

    return 0;
}



int MiniMidi_Synth_destroy( MiniMidi_Synth *s ){
    PaError e = Pa_StopStream(s->pa_stream);

    if (e != paNoError){
        return 1;
    }

    e = Pa_CloseStream(s->pa_stream);



    if (e != paNoError){
        return 1;
    }

    s->pa_stream = NULL;

    return 0;
}



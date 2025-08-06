#include "minimidi-audio.h"




static int paStreamCallback( const void *in_buff, void *out_buff, unsigned long frames_per_buffer, const PaStreamCallbackTimeInfo* timeInfo, PaStreamCallbackFlags statusFlags, void *userData ){



    return 1;
}



int MiniMidi_Synth_init( MiniMidi_Synth *self ){

    // init memory
    self->n_oscilators = 0;
    self->n_samples_in_buffer = 0;


    // init Portaudio
    if ( Pa_Initialize() != paNoError){
        // fail
        return 1;
    }

    // open stream
    PaError e = Pa_OpenDefaultStream(
        &self->pa_stream,
        0,
        N_CHANNELS,
        paFloat32,
        AUDIO_FRAMERATE,
        BUFF_SIZE,
        paStreamCallback,
        self->buffer
    );


    if (e != paNoError){
        return 1;
    }

    // start stream
    e = Pa_StartStream(self->pa_stream);

    if (e != paNoError){
        return 1;
    }

    return 0;
}

int MiniMidi_Synth_destroy( MiniMidi_Synth *self ){
    PaError e = Pa_StopStream(self->pa_stream);

    if (e != paNoError){
        return 1;
    }

    e = Pa_CloseStream(self->pa_stream);



    if (e != paNoError){
        return 1;
    }

    self->pa_stream = NULL;

    return 0;
}
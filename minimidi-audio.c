#include "minimidi-audio.h"
#include "minimidi-log.h"

#include <math.h>
#include <stdlib.h>

#define PI 3.14159265358979323846
#define BUFFER_SIZE 1024

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
    
    MM_AudioEngine *e = (MM_AudioEngine*)_my_data;
    float *out = (float*)outputBuffer;

    // process Cmd Buff
    MM_AudioCommand _cmd;
    while (!MM_Ring_Buffer__is_empty(e->cmd_queue)){
        MM_Ring_Buffer__pop(e->cmd_queue, &_cmd);
        if (_cmd.cmd_type == MM_CMD_PLAY){
            e->playing = true;
        } else if(_cmd.cmd_type == MM_CMD_PAUSE){
            e->playing = false;
        } else if(_cmd.cmd_type == MM_CMD_STOP){
            e->playing = false;
        } else if(_cmd.cmd_type == MM_CMD_BACK_TO_BEGINING){
            e->audio_time = 0;
        } else {
            // not good.
        }
    }

    if (e->playing){
        // process midi
        // is the synth on note_on mode?
        // double _cycle_t = e->audio_time;
        for (size_t i = 0; i < BUFFER_SIZE; i++){
            
            // check if Synth state needs to change
            double _nxt_evt_t = MM_Util_tick_to_s(e->nxt_evt->abs_ticks, e->bpm, e->midi_file->header->ppqn);

            if (_nxt_evt_t <= e->audio_time) {
                // event has occured
                if (e->nxt_evt->status_code == MIDI_NOTE_OFF) {
                    e->synth.note_on = false;
                } else if (e->nxt_evt->status_code == MIDI_NOTE_ON){
                    e->synth.note_on = true;
                    e->synth.active_note = &(e->nxt_evt->note);
                }

                e->nxt_evt = e->nxt_evt->next;
            }

            out[i] = MM_Synth_next_sample( &(e->synth), e->audio_time);
            
            e->audio_time += e->delta_t;
        }
        


    } else {
        // fill buff with zeros and carry on
        for (size_t i = 0; i < BUFFER_SIZE; i++){
            out[i] = 0.0;
        }
    }


    return 0;
}

int MM_AudioEngine_init(MM_AudioEngine *s, MM_Ring_Buffer *cmd_queue, MM_File *file ){
    log_debug("MM_AudioEngine_init: entering");
    
    s->sample_rate = AUDIO_FRAMERATE;
    s->audio_time = 0;
    s->delta_t = 1.0 / AUDIO_FRAMERATE;
    s->cmd_queue = cmd_queue;
    s->midi_file = file;
    s->nxt_evt = &(file->track->event_arr[0]);
    s->playing = false;

    MM_Synth_init(&(s->synth));
    
    // init portaudio
    if ( Pa_Initialize() != paNoError){    
        return 1;
    }
    PaError e = Pa_OpenDefaultStream(
        &(s->pa_st),
        0,
        N_CHANNELS,
        paFloat32,
        AUDIO_FRAMERATE,
        BUFFER_SIZE,
        paStreamCallback,
        s
    );

    if (e != paNoError){
        return 1;
    }

    e = Pa_StartStream(s->pa_st);

    if (e != paNoError){
        return 1;
    }

    const PaStreamInfo *sInfo = Pa_GetStreamInfo(s->pa_st);
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
    return 0;
}

int MM_AudioEngine_destroy( MM_AudioEngine *self ) {
    PaError e = Pa_StopStream(self->pa_st);

    if (e != paNoError){
        return 1;
    }

    e = Pa_CloseStream(self->pa_st);

    if (e != paNoError){
        return 1;
    }

    self->pa_st = NULL;
    return 0;
}
void MM_Synth_init(MM_Synth *s){

    log_info("MM_Synth_init: Entering.");
    s->n_oscillators = 1;
        // init note freqs
    for (int i = 0; i < NOTE_RANGE; i++){
        s->tempered_freqs[i] = _calc_tempered_freq( i );
    }

    s->note_on = false;
    s->active_note = NULL;

    // just 1 for now, more to come for more fx / tones
    for (int i = 0; i < s->n_oscillators; i++){
        s->oscillators[i].amp = 0.5;
        s->oscillators[i].theta = 0;
        s->oscillators[i].phase = 0;
        s->oscillators[i].is_active = false;
        s->oscillators[i].dtheta = (2* PI ) / ((float) AUDIO_FRAMERATE);
    }


}
void MM_Synth_note_on (MM_Synth *s, MidiNote *_n){
    s->active_note = _n;
    s->note_on = true;
}
void MM_Synth_note_off(MM_Synth *s,  MidiNote *_n){
    s->note_on = false;
}
float MM_Synth_next_sample(MM_Synth *s, double t){
    if (s->note_on){
        float freq = s->tempered_freqs[ 12 * s->active_note->octave + (int)s->active_note->note ];
        float period = 1.0 / freq;
        float t_in_period = fmodf( t, period );
        float retval = t_in_period / period > 0.5 ? 0 : 0.3333;

        return retval;
    }
    return 0;
}

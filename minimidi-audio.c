#include "minimidi-audio.h"

#include "minimidi-log.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define PI 3.14159265358979323846
#define BUFFER_SIZE 512

static uint64_t scale_ticks(uint64_t ticks, unsigned int from_ppqn,
                            unsigned int to_ppqn)
{
    uint64_t whole = ticks / from_ppqn;
    uint64_t remainder = ticks % from_ppqn;
    return whole * to_ppqn + (remainder * to_ppqn + from_ppqn / 2) / from_ppqn;
}

static uint64_t ticks_to_samples(const MM_AudioEngine *engine, uint64_t ticks)
{
    long double numerator = (long double)ticks * 60.0L * engine->project->file->sample_rate;
    long double denominator = (long double)engine->project->ppqn * engine->project->file->tempo;
    return (uint64_t)ceill(numerator / denominator);
}

static uint64_t samples_to_ticks(const MM_AudioEngine *engine, uint64_t samples)
{
    long double numerator = (long double)samples * engine->project->file->tempo
                          * engine->project->ppqn;
    long double denominator = 60.0L * engine->project->file->sample_rate;
    return (uint64_t)(numerator / denominator);
}

static void synth_reset(MM_Synth *synth)
{
    synth->phase = 0.0;
    synth->note_on = false;
}

static void synth_note_on(MM_Synth *synth, uint8_t note)
{
    synth->active_note = note;
    synth->note_on = true;
}

static void synth_note_off(MM_Synth *synth, uint8_t note)
{
    if (synth->note_on && synth->active_note == note) synth->note_on = false;
}

static double synth_sample(MM_Synth *synth)
{
    double sample;
    double frequency;
    if (!synth->note_on) return 0.0;
    frequency = 440.0 * pow(2.0, ((int)synth->active_note - 69) / 12.0);
    switch (synth->wave) {
        case MM_WAVE_SQUARE:
            sample = synth->phase < 0.5 ? 1.0 : -1.0;
            break;
        case MM_WAVE_TRIANGLE:
            sample = 1.0 - 4.0 * fabs(synth->phase - 0.5);
            break;
        case MM_WAVE_SIN:
            sample = sin(2.0 * PI * synth->phase);
            break;
        default:
            sample = 0.0;
            break;
    }
    synth->phase += frequency / synth->sample_rate;
    synth->phase -= floor(synth->phase);
    return sample;
}

static void audio_track_reset(MM_AudioTrack *track, MM_SequenceTrack *arrangement)
{
    track->arrangement = arrangement;
    track->clip_index = 0;
    track->event_index = 0;
    track->cycle_index = 0;
    track->active_clip = NULL;
    synth_reset(&track->synth);
}

static void engine_load_sequence(MM_AudioEngine *engine, size_t index)
{
    size_t i;
    MM_Sequence *sequence = &engine->project->sequence_arr[index];
    engine->active_sequence_index = index;
    engine->sequence_samples = 0;
    engine->sequence_length_samples = ticks_to_samples(engine, sequence->length_ticks);
    for (i = 0; i < engine->n_tracks; i++)
        audio_track_reset(&engine->tracks_arr[i], &sequence->tracks[i]);
    atomic_store_explicit(&engine->posted_sequence_index, index, memory_order_relaxed);
    atomic_store_explicit(&engine->posted_sequence_tick, 0, memory_order_relaxed);
}

static void engine_rewind(MM_AudioEngine *engine)
{
    engine->total_samples = 0;
    engine->finished = false;
    engine->active_sequence_play = 1;
    engine_load_sequence(engine, 0);
    atomic_store_explicit(&engine->posted_audio_time, 0.0, memory_order_relaxed);
}

static double audio_track_sample(MM_AudioTrack *track, uint64_t sequence_tick,
                                 unsigned int project_ppqn)
{
    MM_SequenceTrack *arrangement = track->arrangement;
    const MM_Clip *clip;
    uint64_t local_tick;
    uint64_t cycle;
    uint64_t source_position;

    while (track->clip_index < arrangement->n_clips
           && sequence_tick >= arrangement->clips[track->clip_index].end_tick) {
        synth_reset(&track->synth);
        track->clip_index++;
        track->active_clip = NULL;
        track->event_index = 0;
        track->cycle_index = 0;
    }
    if (track->clip_index >= arrangement->n_clips) return 0.0;
    clip = &arrangement->clips[track->clip_index];
    if (sequence_tick < clip->start_tick) return 0.0;

    if (track->active_clip != clip) {
        track->active_clip = clip;
        track->event_index = 0;
        track->cycle_index = 0;
        synth_reset(&track->synth);
    }
    local_tick = sequence_tick - clip->start_tick;
    if (!clip->loop && local_tick >= clip->source_duration_ticks) {
        synth_reset(&track->synth);
        return 0.0;
    }
    cycle = clip->loop ? local_tick / clip->source_duration_ticks : 0;
    source_position = clip->loop ? local_tick % clip->source_duration_ticks : local_tick;
    if (cycle != track->cycle_index) {
        track->cycle_index = cycle;
        track->event_index = 0;
        synth_reset(&track->synth);
    }

    while (track->event_index < clip->midi->track.n_events) {
        MM_MidiEvent *event = &clip->midi->track.event_arr[track->event_index];
        uint64_t event_tick = scale_ticks(event->abs_ticks, clip->midi->header.ppqn,
                                          project_ppqn);
        if (event_tick > source_position) break;
        if (event->status_code == MIDI_NOTE_ON) synth_note_on(&track->synth, event->note_number);
        else if (event->status_code == MIDI_NOTE_OFF) synth_note_off(&track->synth, event->note_number);
        track->event_index++;
    }
    return synth_sample(&track->synth) * track->gain;
}

static void process_commands(MM_AudioEngine *engine)
{
    MM_AudioCommand command;
    while (MM_Ring_Buffer__pop(engine->cmd_queue, &command)) {
        switch (command.cmd_type) {
            case MM_CMD_PLAY:
                if (engine->finished) engine_rewind(engine);
                engine->playing = true;
                break;
            case MM_CMD_PAUSE:
                engine->playing = false;
                break;
            case MM_CMD_STOP:
                engine->playing = false;
                engine_rewind(engine);
                break;
            case MM_CMD_BACK_TO_BEGINNING:
                engine_rewind(engine);
                break;
        }
    }
    atomic_store_explicit(&engine->posted_playing, engine->playing, memory_order_relaxed);
}

static void advance_sequence(MM_AudioEngine *engine)
{
    MM_Sequence *sequence = &engine->project->sequence_arr[engine->active_sequence_index];
    if (sequence->n_repeats == 0 || engine->active_sequence_play < sequence->n_repeats) {
        if (sequence->n_repeats != 0) engine->active_sequence_play++;
        engine_load_sequence(engine, engine->active_sequence_index);
    } else if (engine->active_sequence_index + 1 < engine->project->n_sequences) {
        engine->active_sequence_play = 1;
        engine_load_sequence(engine, engine->active_sequence_index + 1);
    } else {
        size_t i;
        engine->playing = false;
        engine->finished = true;
        for (i = 0; i < engine->n_tracks; i++) synth_reset(&engine->tracks_arr[i].synth);
        atomic_store_explicit(&engine->posted_playing, false, memory_order_relaxed);
    }
}

static int pa_stream_callback(const void *input, void *output,
                              unsigned long frames_per_buffer,
                              const PaStreamCallbackTimeInfo *time_info,
                              PaStreamCallbackFlags flags, void *user_data)
{
    MM_AudioEngine *engine = user_data;
    (void)input;
    (void)time_info;
    (void)flags;
    MM_AudioEngine_render(engine, output, frames_per_buffer);
    return paContinue;
}

void MM_AudioEngine_render(MM_AudioEngine *engine, float *out, size_t frames)
{
    size_t frame;
    if (!engine || !out) return;
    process_commands(engine);
    for (frame = 0; frame < frames; frame++) {
        double mixed = 0.0;
        if (engine->playing) {
            uint64_t tick = samples_to_ticks(engine, engine->sequence_samples);
            size_t track_index;
            for (track_index = 0; track_index < engine->n_tracks; track_index++)
                mixed += audio_track_sample(&engine->tracks_arr[track_index], tick,
                                            engine->project->ppqn);
            if (mixed > 1.0) mixed = 1.0;
            else if (mixed < -1.0) mixed = -1.0;
            engine->sequence_samples++;
            engine->total_samples++;
            atomic_store_explicit(&engine->posted_sequence_tick, tick, memory_order_relaxed);
            if (engine->sequence_samples >= engine->sequence_length_samples)
                advance_sequence(engine);
        }
        out[frame] = (float)mixed;
    }
    atomic_store_explicit(&engine->posted_audio_time,
        (double)engine->total_samples / engine->project->file->sample_rate,
        memory_order_relaxed);
}

int MM_AudioEngine_init_offline(MM_AudioEngine *engine, MM_Project *project,
                                MM_Ring_Buffer *cmd_queue)
{
    size_t i;
    if (!engine || !project || !cmd_queue || project->n_sequences == 0) return -1;
    memset(engine, 0, sizeof(*engine));
    engine->project = project;
    engine->cmd_queue = cmd_queue;
    engine->n_tracks = project->n_tracks;
    engine->tracks_arr = calloc(engine->n_tracks, sizeof(*engine->tracks_arr));
    if (!engine->tracks_arr) return -1;
    for (i = 0; i < engine->n_tracks; i++) {
        engine->tracks_arr[i].gain = project->tracks_arr[i].gain;
        engine->tracks_arr[i].synth.wave = project->tracks_arr[i].wave;
        engine->tracks_arr[i].synth.sample_rate = project->file->sample_rate;
    }
    atomic_init(&engine->posted_audio_time, 0.0);
    atomic_init(&engine->posted_sequence_index, 0);
    atomic_init(&engine->posted_sequence_tick, 0);
    atomic_init(&engine->posted_playing, false);
    engine_rewind(engine);
    return 0;
}

int MM_AudioEngine_init(MM_AudioEngine *engine, MM_Project *project,
                        MM_Ring_Buffer *cmd_queue)
{
    PaError error;
    if (MM_AudioEngine_init_offline(engine, project, cmd_queue) != 0) return -1;

    error = Pa_Initialize();
    if (error != paNoError) {
        log_error("PortAudio initialization failed: %s", Pa_GetErrorText(error));
        fprintf(stderr, "PortAudio initialization failed: %s\n", Pa_GetErrorText(error));
        MM_AudioEngine_destroy(engine);
        return -1;
    }
    engine->pa_initialized = true;
    error = Pa_OpenDefaultStream(&engine->pa_st, 0, N_CHANNELS, paFloat32,
                                 project->file->sample_rate, BUFFER_SIZE,
                                 pa_stream_callback, engine);
    if (error != paNoError) {
        log_error("cannot open audio output at %u Hz: %s",
                  project->file->sample_rate, Pa_GetErrorText(error));
        fprintf(stderr, "cannot open audio output at %u Hz: %s\n",
                project->file->sample_rate, Pa_GetErrorText(error));
        MM_AudioEngine_destroy(engine);
        return -1;
    }
    error = Pa_StartStream(engine->pa_st);
    if (error != paNoError) {
        log_error("cannot start audio stream: %s", Pa_GetErrorText(error));
        fprintf(stderr, "cannot start audio stream: %s\n", Pa_GetErrorText(error));
        MM_AudioEngine_destroy(engine);
        return -1;
    }
    return 0;
}

int MM_AudioEngine_destroy(MM_AudioEngine *engine)
{
    int result = 0;
    if (!engine) return -1;
    if (engine->pa_st) {
        PaError error;
        if (Pa_IsStreamActive(engine->pa_st) == 1) {
            error = Pa_StopStream(engine->pa_st);
            if (error != paNoError) result = -1;
        }
        error = Pa_CloseStream(engine->pa_st);
        if (error != paNoError) result = -1;
        engine->pa_st = NULL;
    }
    if (engine->pa_initialized) {
        if (Pa_Terminate() != paNoError) result = -1;
        engine->pa_initialized = false;
    }
    free(engine->tracks_arr);
    engine->tracks_arr = NULL;
    engine->n_tracks = 0;
    return result;
}

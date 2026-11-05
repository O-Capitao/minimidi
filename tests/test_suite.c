#define _POSIX_C_SOURCE 200809L

#include "minimidi-audio.h"
#include "minimidi-proj-file.h"
#include "minimidi-proj.h"
#include "minimidi-rb.h"
#include "minimidi.h"
#include "minimidi-log.h"

#include <math.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sched.h>
#include <dirent.h>
#include <unistd.h>

static unsigned int checks;
static unsigned int failures;
static char temp_dir[] = "/tmp/minimidi-test-XXXXXX";
static char midi_path[256];
static char midi_simple_path[256];

#define CHECK(condition) do { \
    checks++; \
    if (!(condition)) { \
        failures++; \
        fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #condition); \
    } \
} while (0)

#define CHECK_EQ(actual, expected) CHECK((actual) == (expected))

static void put_be16(FILE *file, uint16_t value)
{
    fputc((value >> 8) & 0xff, file);
    fputc(value & 0xff, file);
}

static void put_be32(FILE *file, uint32_t value)
{
    fputc((value >> 24) & 0xff, file);
    fputc((value >> 16) & 0xff, file);
    fputc((value >> 8) & 0xff, file);
    fputc(value & 0xff, file);
}

static int write_smf(const char *path, uint16_t format, uint16_t ppqn,
                     const uint8_t *const tracks[], const size_t lengths[],
                     size_t count)
{
    FILE *file = fopen(path, "wb");
    size_t i;
    if (!file) return -1;
    if (fwrite("MThd", 1, 4, file) != 4) goto error;
    put_be32(file, 6);
    put_be16(file, format);
    put_be16(file, (uint16_t)count);
    put_be16(file, ppqn);
    for (i = 0; i < count; i++) {
        if (fwrite("MTrk", 1, 4, file) != 4) goto error;
        put_be32(file, (uint32_t)lengths[i]);
        if (fwrite(tracks[i], 1, lengths[i], file) != lengths[i]) goto error;
    }
    return fclose(file);
error:
    fclose(file);
    return -1;
}

static int write_text(const char *path, const char *text)
{
    FILE *file = fopen(path, "w");
    int result;
    if (!file) return -1;
    result = fputs(text, file) < 0 ? -1 : 0;
    if (fclose(file) != 0) result = -1;
    return result;
}

static void fixture_paths(char *project, size_t size, const char *name)
{
    snprintf(project, size, "%s/%s", temp_dir, name);
}

static const char *basic_yaml =
    "name: Test Project\n"
    "tempo: 120\n"
    "sample_rate: 8000\n"
    "ppqn: 96\n"
    "beat_per_bar: 4\n"
    "track-config:\n"
    "  tone:\n"
    "    wave: SQUARE\n"
    "    gain: 0.5\n"
    "sequence:\n"
    "  - name: one\n"
    "    length: 1B\n"
    "    n_repeats: 1\n"
    "    tracks:\n"
    "      tone:\n"
    "        - midi: notes.mid\n"
    "          loop: false\n"
    "          length: 1B\n";

static void create_fixtures(void)
{
    static const uint8_t track_multi_a[] = {
        0x00, 0x90, 60, 100,
        0x30, 0x80, 60, 0,
        0x00, 0x91, 64, 90,
        0x30, 64, 0,
        0x00, 0xff, 0x2f, 0x00
    };
    static const uint8_t track_multi_b[] = {
        0x00, 0xff, 0x03, 0x04, 't', 'e', 's', 't',
        0x30, 0x90, 67, 80,
        0x60, 0x80, 67, 0,
        0x00, 0xff, 0x2f, 0x00
    };
    static const uint8_t track_simple[] = {
        0x00, 0x90, 60, 100,
        0x30, 0x80, 60, 0,
        0x81, 0x20, 0xff, 0x2f, 0x00
    };
    static const uint8_t *multi_tracks[] = {track_multi_a, track_multi_b};
    static const size_t multi_lengths[] = {sizeof(track_multi_a), sizeof(track_multi_b)};
    static const uint8_t *simple_tracks[] = {track_simple};
    static const size_t simple_lengths[] = {sizeof(track_simple)};

    snprintf(midi_path, sizeof(midi_path), "%s/notes.mid", temp_dir);
    snprintf(midi_simple_path, sizeof(midi_simple_path), "%s/simple.mid", temp_dir);
    CHECK_EQ(write_smf(midi_path, 1, 48, multi_tracks, multi_lengths, 2), 0);
    CHECK_EQ(write_smf(midi_simple_path, 0, 48, simple_tracks, simple_lengths, 1), 0);
}

static void test_time_and_wave_helpers(void)
{
    uint64_t ticks = 0;
    double beats = -1.0;
    MM_WaveType wave = MM_WAVE_SIN;

    CHECK_EQ(MM_Proj_File_parse_time("2B3b4t", 4, 96, &ticks, &beats), 0);
    CHECK_EQ(ticks, 1060U);
    CHECK(fabs(beats - (1060.0 / 96.0)) < 0.000001);
    CHECK_EQ(MM_Proj_File_parse_time("0", 4, 96, &ticks, &beats), 0);
    CHECK_EQ(ticks, 0U);
    CHECK_EQ(beats, 0.0);
    CHECK_EQ(MM_Proj_File_parse_time("4B", 4, 96, &ticks, &beats), 0);
    CHECK_EQ(ticks, 1536U);
    CHECK(MM_Proj_File_parse_time(NULL, 4, 96, &ticks, &beats) != 0);
    CHECK(MM_Proj_File_parse_time("", 4, 96, &ticks, &beats) != 0);
    CHECK(MM_Proj_File_parse_time("1b1B", 4, 96, &ticks, &beats) != 0);
    CHECK(MM_Proj_File_parse_time("1B1B", 4, 96, &ticks, &beats) != 0);
    CHECK(MM_Proj_File_parse_time("1x", 4, 96, &ticks, &beats) != 0);
    CHECK(MM_Proj_File_parse_time("12", 4, 96, &ticks, &beats) != 0);
    CHECK(MM_Proj_File_parse_time("18446744073709551616t", 4, 96,
                                  &ticks, &beats) != 0);
    CHECK(MM_Proj_File_parse_time("1t", 0, 96, &ticks, &beats) != 0);
    CHECK(MM_Proj_File_parse_time("1t", 4, 0, &ticks, &beats) != 0);

    CHECK(mm_wave_from_str("SQUARE", &wave));
    CHECK_EQ(wave, MM_WAVE_SQUARE);
    CHECK(mm_wave_from_str("TRIANGLE", &wave));
    CHECK_EQ(wave, MM_WAVE_TRIANGLE);
    CHECK(mm_wave_from_str("SIN", &wave));
    CHECK_EQ(wave, MM_WAVE_SIN);
    CHECK(!mm_wave_from_str("square", &wave));
    CHECK(!mm_wave_from_str(NULL, &wave));
    CHECK(!mm_wave_from_str("SIN", NULL));
    CHECK(strcmp(mm_wave_to_str(MM_WAVE_SQUARE), "SQUARE") == 0);
    CHECK(strcmp(mm_wave_to_str(MM_WAVE_TRIANGLE), "TRIANGLE") == 0);
    CHECK(strcmp(mm_wave_to_str(MM_WAVE_SIN), "SIN") == 0);
}

static void test_midi_parser(void)
{
    MM_Midi_File midi;
    MM_MidiEvent_LList range;
    MM_MidiEvent *events;

    memset(&midi, 0, sizeof(midi));
    CHECK_EQ(MM_File_init(&midi, midi_path), 0);
    CHECK_EQ(midi.header.format, 1);
    CHECK_EQ(midi.header.ntrks, 2);
    CHECK_EQ(midi.header.ppqn, 48);
    CHECK_EQ(midi.track.n_events, 6U);
    CHECK_EQ(midi.track.total_ticks, 144U);
    CHECK(fabs(midi.track.total_beats - 3.0) < 0.000001);
    events = midi.track.event_arr;
    CHECK_EQ(events[0].abs_ticks, 0U);
    CHECK_EQ(events[0].status_code, MIDI_NOTE_ON);
    CHECK_EQ(events[0].note_number, 60);
    CHECK_EQ(events[0].note.note, C);
    CHECK_EQ(events[0].note.octave, 4);
    CHECK(events[0].next != NULL);
    CHECK_EQ(events[0].next->status_code, MIDI_NOTE_OFF);
    CHECK_EQ(events[0].next->abs_ticks, 48U);
    CHECK(events[0].next->prev == &events[0]);
    CHECK_EQ(events[1].abs_ticks, 48U);
    CHECK_EQ(events[1].status_code, MIDI_NOTE_OFF);

    CHECK_EQ(MM_MidiEvent_LList_init(&range), 0);
    CHECK_EQ(MM_File_get_events_in_range(&midi, &range, 48, 96, 60, 67), 0);
    CHECK_EQ(range.length, 4U);
    CHECK(range.first != NULL && range.last != NULL);
    CHECK(MM_File_get_events_in_range(&midi, &range, 97, 96, 0, 127) != 0);
    CHECK_EQ(MM_File_get_events_in_range(&midi, &range, 0, 0, 61, 60), -1);
    CHECK_EQ(range.length, 4U);
    CHECK_EQ(MM_MidiEvent_LList_destroy(&range), 0);
    CHECK_EQ(MM_MidiEvent_LList_destroy(&range), 0);
    MM_File_free(&midi);
    MM_File_free(&midi);
    CHECK_EQ(midi.track.n_events, 0U);
    CHECK_EQ(MM_File_init(NULL, midi_path), -1);
    CHECK_EQ(MM_File_init(&midi, "/no/such/midi/file.mid"), -1);
}

static void test_midi_rejections(void)
{
    char path[256];
    static const uint8_t malformed_vlq[] = {0x80, 0x80, 0x80, 0x80, 0x80};
    static const uint8_t empty_track[] = {0x00, 0xff, 0x2f, 0x00};
    static const uint8_t *tracks[] = {malformed_vlq};
    size_t lengths[] = {sizeof(malformed_vlq)};
    MM_Midi_File midi;
    FILE *file;

    fixture_paths(path, sizeof(path), "bad-vlq.mid");
    CHECK_EQ(write_smf(path, 0, 96, tracks, lengths, 1), 0);
    CHECK_EQ(MM_File_init(&midi, path), -1);
    tracks[0] = empty_track;
    lengths[0] = sizeof(empty_track);
    fixture_paths(path, sizeof(path), "bad-format.mid");
    CHECK_EQ(write_smf(path, 2, 96, tracks, lengths, 1), 0);
    CHECK_EQ(MM_File_init(&midi, path), -1);
    fixture_paths(path, sizeof(path), "bad-division.mid");
    CHECK_EQ(write_smf(path, 0, 0x8001, tracks, lengths, 1), 0);
    CHECK_EQ(MM_File_init(&midi, path), -1);

    fixture_paths(path, sizeof(path), "truncated.mid");
    file = fopen(path, "wb");
    CHECK(file != NULL);
    if (file) {
        fwrite("MThd", 1, 4, file);
        fclose(file);
    }
    CHECK_EQ(MM_File_init(&midi, path), -1);
}

static void test_project_file_and_roundtrip(void)
{
    MM_File_Project file_project;
    MM_File_Project roundtrip;
    char path[256];
    char output[256];

    fixture_paths(path, sizeof(path), "basic.yaml");
    fixture_paths(output, sizeof(output), "roundtrip.yaml");
    CHECK_EQ(write_text(path, basic_yaml), 0);
    memset(&file_project, 0, sizeof(file_project));
    CHECK_EQ(MM_Proj_File_read(path, &file_project), 0);
    CHECK_EQ(file_project.tempo, 120U);
    CHECK_EQ(file_project.sample_rate, 8000U);
    CHECK_EQ(file_project.ppqn, 96U);
    CHECK_EQ(file_project.num_track_configs, 1U);
    CHECK_EQ(file_project.num_sequences, 1U);
    CHECK_EQ(file_project.sequences[0].length_ticks, 384U);
    CHECK_EQ(file_project.sequences[0].n_repeats, 1U);
    CHECK_EQ(file_project.sequences[0].tracks[0].num_assignments, 1U);
    CHECK_EQ(file_project.sequences[0].tracks[0].assignments[0].length_ticks, 384U);
    CHECK(strcmp(file_project.sequences[0].tracks[0].assignments[0].resolved_midi_path,
                 midi_path) == 0);
    CHECK_EQ(MM_Proj_File_write(&file_project, output), 0);
    memset(&roundtrip, 0, sizeof(roundtrip));
    CHECK_EQ(MM_Proj_File_read(output, &roundtrip), 0);
    CHECK(strcmp(roundtrip.name, file_project.name) == 0);
    CHECK_EQ(roundtrip.sequences[0].n_repeats, 1U);
    CHECK(strcmp(roundtrip.sequences[0].tracks[0].assignments[0].midi_path,
                 "notes.mid") == 0);
    MM_Proj_File_free(&roundtrip);
    MM_Proj_File_free(&file_project);
    MM_Proj_File_free(&file_project);
}

static void test_project_file_rejections(void)
{
    static const char *invalid[] = {
        "name: x\ntempo: 0\nsample_rate: 8000\nppqn: 96\nbeat_per_bar: 4\ntrack-config: {}\nsequence: []\n",
        "name: x\ntempo: 120\nsample_rate: 7999\nppqn: 96\nbeat_per_bar: 4\ntrack-config: {}\nsequence: []\n",
        "name: x\ntempo: 120\nsample_rate: 8000\nppqn: 32768\nbeat_per_bar: 4\ntrack-config: {}\nsequence: []\n",
        "name: x\ntempo: 120\nsample_rate: 8000\nppqn: 96\nbeat_per_bar: 4\nunknown: true\ntrack-config: {}\nsequence: []\n",
        "name: x\ntempo: 120\nsample_rate: 8000\nppqn: 96\nbeat_per_bar: 4\ntrack-config:\n  t:\n    wave: NOPE\n    gain: 1\nsequence: []\n",
        "name: x\ntempo: 120\nsample_rate: 8000\nppqn: 96\nbeat_per_bar: 4\ntrack-config:\n  t:\n    wave: SIN\n    gain: nan\nsequence: []\n",
        "name: x\ntempo: 120\nsample_rate: 8000\nppqn: 96\nbeat_per_bar: 4\ntrack-config:\n  t:\n    wave: SIN\n    gain: 1\nsequence:\n  - name: s\n    length: 1x\n    tracks: {}\n"
    };
    char path[256];
    size_t i;
    MM_File_Project project;

    for (i = 0; i < sizeof(invalid) / sizeof(invalid[0]); i++) {
        snprintf(path, sizeof(path), "%s/invalid-%zu.yaml", temp_dir, i);
        CHECK_EQ(write_text(path, invalid[i]), 0);
        CHECK_EQ(MM_Proj_File_read(path, &project), -1);
    }
    CHECK_EQ(MM_Proj_File_read(NULL, &project), -1);
    CHECK_EQ(MM_Proj_File_read("/no/such/project.yaml", &project), -1);
}

static void test_project_compilation(void)
{
    MM_Project project;
    char path[256];

    fixture_paths(path, sizeof(path), "basic.yaml");
    CHECK_EQ(MM_Project_init(&project, path), 0);
    CHECK_EQ(project.ppqn, 96U);
    CHECK_EQ(project.n_tracks, 1U);
    CHECK_EQ(project.n_sequences, 1U);
    CHECK(strcmp(project.tracks_arr[0].name, "tone") == 0);
    CHECK_EQ(project.tracks_arr[0].wave, MM_WAVE_SQUARE);
    CHECK(fabs(project.tracks_arr[0].gain - 0.5) < 0.000001);
    CHECK_EQ(project.sequence_arr[0].length_ticks, 384U);
    CHECK_EQ(project.sequence_arr[0].tracks[0].n_clips, 1U);
    CHECK_EQ(project.sequence_arr[0].tracks[0].clips[0].source_duration_ticks, 288U);
    CHECK_EQ(project.sequence_arr[0].tracks[0].clips[0].duration_ticks, 384U);
    CHECK_EQ(project.sequence_arr[0].tracks[0].clips[0].end_tick, 384U);
    CHECK(project.sequence_arr[0].tracks[0].clips[0].midi->track.n_events == 6);
    MM_Project_free(&project);
    MM_Project_free(&project);

    fixture_paths(path, sizeof(path), "missing-midi.yaml");
    CHECK_EQ(write_text(path,
        "name: x\ntempo: 120\nsample_rate: 8000\nppqn: 96\nbeat_per_bar: 4\n"
        "track-config:\n  t:\n    wave: SIN\n    gain: 1\nsequence:\n"
        "  - name: s\n    length: 1B\n    tracks:\n      t:\n"
        "        - midi: does-not-exist.mid\n          length: 1B\n"), 0);
    CHECK_EQ(MM_Project_init(&project, path), -1);

    fixture_paths(path, sizeof(path), "overflow-clip.yaml");
    CHECK_EQ(write_text(path,
        "name: x\ntempo: 120\nsample_rate: 8000\nppqn: 96\nbeat_per_bar: 4\n"
        "track-config:\n  t:\n    wave: SIN\n    gain: 1\nsequence:\n"
        "  - name: s\n    length: 1B\n    tracks:\n      t:\n"
        "        - midi: notes.mid\n          start: 1B\n          length: 1B\n"), 0);
    CHECK_EQ(MM_Project_init(&project, path), -1);

    fixture_paths(path, sizeof(path), "overlap.yaml");
    CHECK_EQ(write_text(path,
        "name: x\ntempo: 120\nsample_rate: 8000\nppqn: 96\nbeat_per_bar: 4\n"
        "track-config:\n  t:\n    wave: SIN\n    gain: 1\nsequence:\n"
        "  - name: s\n    length: 1B\n    tracks:\n      t:\n"
        "        - midi: notes.mid\n          start: 0\n          length: 1b\n"
        "        - midi: notes.mid\n          start: 0t\n          length: 1b\n"), 0);
    CHECK_EQ(MM_Project_init(&project, path), -1);
}

static void test_multitrack_project(void)
{
    static const char *yaml =
        "name: Multitrack\ntempo: 140\nsample_rate: 22050\nppqn: 96\nbeat_per_bar: 4\n"
        "track-config:\n  lead_one:\n    wave: SQUARE\n    gain: 0.3\n"
        "  lead_two:\n    wave: TRIANGLE\n    gain: 0.25\nsequence:\n"
        "  - name: forever\n    length: 2B\n    n_repeats: 0\n    tracks:\n"
        "      lead_one:\n        - midi: notes.mid\n          start: 1b\n          length: 1b\n"
        "        - midi: notes.mid\n          start: 0\n          length: 1b\n"
        "      lead_two:\n        - midi: notes.mid\n          start: 0\n          loop: true\n          length: 2B\n";
    MM_Project project;
    char path[256];

    fixture_paths(path, sizeof(path), "multitrack.yaml");
    CHECK_EQ(write_text(path, yaml), 0);
    if (MM_Project_init(&project, path) != 0) {
        CHECK(false);
        return;
    }
    CHECK_EQ(project.n_tracks, 2U);
    CHECK_EQ(project.n_sequences, 1U);
    CHECK(strcmp(project.tracks_arr[0].name, "lead_one") == 0);
    CHECK(strcmp(project.tracks_arr[1].name, "lead_two") == 0);
    CHECK_EQ(project.sequence_arr[0].n_repeats, 0U);
    CHECK_EQ(project.sequence_arr[0].tracks[0].n_clips, 2U);
    CHECK_EQ(project.sequence_arr[0].tracks[1].n_clips, 1U);
    CHECK_EQ(project.sequence_arr[0].tracks[0].clips[0].start_tick, 0U);
    CHECK_EQ(project.sequence_arr[0].tracks[0].clips[1].start_tick, 96U);
    CHECK(project.sequence_arr[0].tracks[0].clips[0].midi
          == project.sequence_arr[0].tracks[1].clips[0].midi);
    CHECK(project.sequence_arr[0].tracks[0].clips[0].midi
          == project.sequence_arr[0].tracks[0].clips[1].midi);
    CHECK(project.sequence_arr[0].tracks[1].clips[0].loop);
    MM_Project_free(&project);
}

static void test_tick_conversions(void)
{
    CHECK(fabs(MM_Util_tick_to_s(96, 120, 96) - 0.5) < 0.000001);
    CHECK(fabs(MM_Util_tick_to_s(0, 120, 96)) < 0.000001);
    CHECK_EQ(MM_Util_tick_to_s(100, 0, 96), 0.0);
    CHECK_EQ(MM_Util_tick_to_s(100, 120, 0), 0.0);
    CHECK_EQ(MM_Util_s_to_tick(0.5, 120, 96), 96U);
    CHECK_EQ(MM_Util_s_to_tick(0.499, 120, 96), 95U);
    CHECK_EQ(MM_Util_s_to_tick(-1.0, 120, 96), 0U);
    CHECK_EQ(MM_Util_s_to_tick(1.0, 0, 96), 0U);
    CHECK_EQ(MM_Util_s_to_tick(1.0, 120, 0), 0U);
}

static void test_ring_buffer(void)
{
    MM_Ring_Buffer *rb = MM_Ring_Buffer__init(2, sizeof(int));
    int a = 11, b = 22, c = 33, value = 0;

    CHECK(rb != NULL);
    CHECK_EQ(MM_Ring_Buffer__capacity(rb), 2U);
    CHECK(MM_Ring_Buffer__is_empty(rb));
    CHECK(!MM_Ring_Buffer__pop(rb, &value));
    CHECK(MM_Ring_Buffer__push(rb, &a));
    CHECK(MM_Ring_Buffer__push(rb, &b));
    CHECK(!MM_Ring_Buffer__push(rb, &c));
    CHECK(!MM_Ring_Buffer__is_empty(rb));
    CHECK(MM_Ring_Buffer__pop(rb, &value));
    CHECK_EQ(value, a);
    CHECK(MM_Ring_Buffer__push(rb, &c));
    CHECK(MM_Ring_Buffer__pop(rb, &value));
    CHECK_EQ(value, b);
    CHECK(MM_Ring_Buffer__pop(rb, &value));
    CHECK_EQ(value, c);
    CHECK(MM_Ring_Buffer__is_empty(rb));
    CHECK(!MM_Ring_Buffer__push(NULL, &a));
    CHECK(!MM_Ring_Buffer__push(rb, NULL));
    CHECK(!MM_Ring_Buffer__pop(NULL, &value));
    CHECK(!MM_Ring_Buffer__pop(rb, NULL));
    MM_Ring_Buffer__free(rb);
    CHECK(MM_Ring_Buffer__init(0, sizeof(int)) == NULL);
    CHECK(MM_Ring_Buffer__init(1, 0) == NULL);
}

typedef struct {
    MM_Ring_Buffer *buffer;
    int count;
} RingThreadData;

static void *ring_producer(void *context)
{
    RingThreadData *data = context;
    int i;
    for (i = 0; i < data->count; i++)
        while (!MM_Ring_Buffer__push(data->buffer, &i)) sched_yield();
    return NULL;
}

static void *ring_consumer(void *context)
{
    RingThreadData *data = context;
    int expected;
    for (expected = 0; expected < data->count; expected++) {
        int actual;
        while (!MM_Ring_Buffer__pop(data->buffer, &actual)) sched_yield();
        if (actual != expected) {
            failures++;
            fprintf(stderr, "FAIL ring order: expected %d, got %d\n", expected, actual);
        }
        checks++;
    }
    return NULL;
}

static void test_ring_buffer_concurrency(void)
{
    MM_Ring_Buffer *rb = MM_Ring_Buffer__init(31, sizeof(int));
    RingThreadData data = {rb, 20000};
    pthread_t producer;
    pthread_t consumer;

    CHECK(rb != NULL);
    if (!rb) return;
    CHECK_EQ(pthread_create(&producer, NULL, ring_producer, &data), 0);
    CHECK_EQ(pthread_create(&consumer, NULL, ring_consumer, &data), 0);
    CHECK_EQ(pthread_join(producer, NULL), 0);
    CHECK_EQ(pthread_join(consumer, NULL), 0);
    CHECK(MM_Ring_Buffer__is_empty(rb));
    MM_Ring_Buffer__free(rb);
}

static void test_offline_audio(void)
{
    MM_Project project;
    MM_AudioEngine engine;
    MM_Ring_Buffer *commands;
    MM_AudioCommand command;
    float *output;
    char path[256];
    size_t i;
    bool heard_signal = false;

    fixture_paths(path, sizeof(path), "basic.yaml");
    CHECK_EQ(write_text(path,
        "name: Audio Test\ntempo: 120\nsample_rate: 8000\nppqn: 96\nbeat_per_bar: 4\n"
        "track-config:\n  tone:\n    wave: SQUARE\n    gain: 0.5\nsequence:\n"
        "  - name: one\n    length: 1B\n    n_repeats: 1\n    tracks:\n"
        "      tone:\n        - midi: simple.mid\n          loop: false\n          length: 1B\n"), 0);
    if (MM_Project_init(&project, path) != 0) {
        CHECK(false);
        return;
    }
    commands = MM_Ring_Buffer__init(8, sizeof(command));
    CHECK(commands != NULL);
    if (!commands) {
        MM_Project_free(&project);
        return;
    }
    if (MM_AudioEngine_init_offline(&engine, &project, commands) != 0) {
        CHECK(false);
        MM_Ring_Buffer__free(commands);
        MM_Project_free(&project);
        return;
    }
    output = calloc(4002, sizeof(*output));
    CHECK(output != NULL);
    if (!output) goto cleanup_engine;

    command.cmd_type = MM_CMD_PLAY;
    CHECK(MM_Ring_Buffer__push(commands, &command));
    MM_AudioEngine_render(&engine, output, 4001);
    for (i = 0; i < 4000; i++) if (fabsf(output[i]) > 0.01f) heard_signal = true;
    CHECK(heard_signal);
    CHECK(fabsf(output[4000]) < 0.0001f);
    CHECK(atomic_load(&engine.posted_playing));
    CHECK_EQ(atomic_load(&engine.posted_sequence_index), 0U);
    CHECK(atomic_load(&engine.posted_sequence_tick) >= 96U);
    CHECK(fabs(atomic_load(&engine.posted_audio_time) - (4001.0 / 8000.0)) < 0.00001);
    CHECK(atomic_load(&engine.posted_output_peak) > 0.0);

    command.cmd_type = MM_CMD_PAUSE;
    CHECK(MM_Ring_Buffer__push(commands, &command));
    MM_AudioEngine_render(&engine, output, 2);
    CHECK(!atomic_load(&engine.posted_playing));
    CHECK_EQ(output[0], 0.0f);
    CHECK_EQ(output[1], 0.0f);

    command.cmd_type = MM_CMD_STOP;
    CHECK(MM_Ring_Buffer__push(commands, &command));
    MM_AudioEngine_render(&engine, output, 1);
    CHECK_EQ(engine.total_samples, 0U);
    CHECK_EQ(atomic_load(&engine.posted_sequence_tick), 0U);
    CHECK_EQ(atomic_load(&engine.posted_audio_time), 0.0);
    free(output);
cleanup_engine:
    CHECK_EQ(MM_AudioEngine_destroy(&engine), 0);
    MM_Ring_Buffer__free(commands);
    MM_Project_free(&project);
}

static void test_audio_sequence_transitions(void)
{
    static const char *finite_yaml =
        "name: Sequence Test\ntempo: 120\nsample_rate: 8000\nppqn: 96\nbeat_per_bar: 4\n"
        "track-config:\n  tone:\n    wave: SIN\n    gain: 0.25\nsequence:\n"
        "  - name: intro\n    length: 1b\n    n_repeats: 2\n    tracks:\n"
        "      tone:\n        - midi: simple.mid\n          loop: true\n          length: 1b\n"
        "  - name: ending\n    length: 1b\n    n_repeats: 1\n    tracks:\n"
        "      tone:\n        - midi: simple.mid\n          loop: true\n          length: 1b\n";
    static const char *infinite_yaml =
        "name: Infinite Test\ntempo: 120\nsample_rate: 8000\nppqn: 96\nbeat_per_bar: 4\n"
        "track-config:\n  tone:\n    wave: TRIANGLE\n    gain: 0.25\nsequence:\n"
        "  - name: loop\n    length: 1b\n    n_repeats: 0\n    tracks:\n"
        "      tone:\n        - midi: simple.mid\n          loop: true\n          length: 1b\n";
    char path[256];
    MM_Project project;
    MM_AudioEngine engine;
    MM_Ring_Buffer *commands;
    MM_AudioCommand command = {MM_CMD_PLAY};
    float block[12000];

    fixture_paths(path, sizeof(path), "finite-sequences.yaml");
    CHECK_EQ(write_text(path, finite_yaml), 0);
    if (MM_Project_init(&project, path) != 0) {
        CHECK(false);
        return;
    }
    commands = MM_Ring_Buffer__init(4, sizeof(command));
    CHECK(commands != NULL);
    if (!commands) {
        MM_Project_free(&project);
        return;
    }
    if (MM_AudioEngine_init_offline(&engine, &project, commands) != 0) {
        CHECK(false);
        MM_Ring_Buffer__free(commands);
        MM_Project_free(&project);
        return;
    }
    CHECK(MM_Ring_Buffer__push(commands, &command));
    MM_AudioEngine_render(&engine, block, 4000);
    CHECK_EQ(engine.active_sequence_index, 0U);
    CHECK_EQ(engine.active_sequence_play, 2U);
    MM_AudioEngine_render(&engine, block, 4000);
    CHECK_EQ(engine.active_sequence_index, 1U);
    CHECK_EQ(engine.active_sequence_play, 1U);
    MM_AudioEngine_render(&engine, block, 4000);
    CHECK(engine.finished);
    CHECK(!atomic_load(&engine.posted_playing));

    command.cmd_type = MM_CMD_PLAY;
    CHECK(MM_Ring_Buffer__push(commands, &command));
    MM_AudioEngine_render(&engine, block, 1);
    CHECK(!engine.finished);
    CHECK_EQ(engine.active_sequence_index, 0U);
    CHECK(atomic_load(&engine.posted_playing));
    CHECK_EQ(MM_AudioEngine_destroy(&engine), 0);
    MM_Ring_Buffer__free(commands);
    MM_Project_free(&project);

    fixture_paths(path, sizeof(path), "infinite-sequence.yaml");
    CHECK_EQ(write_text(path, infinite_yaml), 0);
    if (MM_Project_init(&project, path) != 0) {
        CHECK(false);
        return;
    }
    commands = MM_Ring_Buffer__init(4, sizeof(command));
    CHECK(commands != NULL);
    if (!commands) {
        MM_Project_free(&project);
        return;
    }
    if (MM_AudioEngine_init_offline(&engine, &project, commands) != 0) {
        CHECK(false);
        MM_Ring_Buffer__free(commands);
        MM_Project_free(&project);
        return;
    }
    command.cmd_type = MM_CMD_PLAY;
    CHECK(MM_Ring_Buffer__push(commands, &command));
    MM_AudioEngine_render(&engine, block, 12000);
    CHECK(!engine.finished);
    CHECK_EQ(engine.active_sequence_index, 0U);
    CHECK_EQ(engine.active_sequence_play, 1U);
    CHECK(atomic_load(&engine.posted_playing));
    CHECK_EQ(MM_AudioEngine_destroy(&engine), 0);
    MM_Ring_Buffer__free(commands);
    MM_Project_free(&project);
}

static void cleanup_temp_directory(void)
{
    DIR *directory = opendir(temp_dir);
    struct dirent *entry;
    if (!directory) return;
    while ((entry = readdir(directory)) != NULL) {
        char path[512];
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
            continue;
        snprintf(path, sizeof(path), "%s/%s", temp_dir, entry->d_name);
        unlink(path);
    }
    closedir(directory);
    rmdir(temp_dir);
}

int main(void)
{
    char log_path[256];
    if (!mkdtemp(temp_dir)) {
        perror("mkdtemp");
        return 2;
    }
    snprintf(log_path, sizeof(log_path), "%s/test.log", temp_dir);
    if (log_init(log_path) != 0) {
        fprintf(stderr, "cannot initialize test log\n");
        return 2;
    }

    create_fixtures();
    test_time_and_wave_helpers();
    test_midi_parser();
    test_midi_rejections();
    test_project_file_and_roundtrip();
    test_project_file_rejections();
    test_project_compilation();
    test_multitrack_project();
    test_tick_conversions();
    test_ring_buffer();
    test_ring_buffer_concurrency();
    test_offline_audio();
    test_audio_sequence_transitions();

    log_deinit();
    printf("%u checks, %u failures\n", checks, failures);
    cleanup_temp_directory();
    return failures == 0 ? 0 : 1;
}

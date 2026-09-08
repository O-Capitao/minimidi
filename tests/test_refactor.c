#include "minimidi-proj-file.h"
#include "minimidi-proj.h"
#include "minimidi-audio.h"
#include "minimidi-rb.h"
#include "minimidi.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(condition) do { \
    if (!(condition)) { \
        fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #condition); \
        return -1; \
    } \
} while (0)

static int write_bytes(const char *path, const unsigned char *data, size_t size)
{
    FILE *stream = fopen(path, "wb");
    if (!stream) return -1;
    if (fwrite(data, 1, size, stream) != size || fclose(stream) != 0) return -1;
    return 0;
}

static int write_text(const char *path, const char *text)
{
    return write_bytes(path, (const unsigned char *)text, strlen(text));
}

static int test_time_parser(void)
{
    uint64_t ticks;
    double beats;
    CHECK(MM_Proj_File_parse_time("0B1b2t", 4, 960, &ticks, &beats) == 0);
    CHECK(ticks == 962 && beats > 1.002 && beats < 1.003);
    CHECK(MM_Proj_File_parse_time("2B", 3, 480, &ticks, &beats) == 0);
    CHECK(ticks == 2880 && beats == 6.0);
    CHECK(MM_Proj_File_parse_time("4b", 4, 960, &ticks, &beats) == 0);
    CHECK(ticks == 3840 && beats == 4.0);
    CHECK(MM_Proj_File_parse_time("0", 4, 960, &ticks, &beats) == 0 && ticks == 0);
    CHECK(MM_Proj_File_parse_time("1b1B", 4, 960, &ticks, &beats) != 0);
    CHECK(MM_Proj_File_parse_time("1B2B", 4, 960, &ticks, &beats) != 0);
    CHECK(MM_Proj_File_parse_time("1.5b", 4, 960, &ticks, &beats) != 0);
    CHECK(MM_Proj_File_parse_time("1", 4, 960, &ticks, &beats) != 0);
    return 0;
}

static int test_project_file(void)
{
    MM_File_Project project;
    MM_File_Project copy;
    CHECK(MM_Proj_File_read("test_proj.yaml", &project) == 0);
    CHECK(project.num_track_configs == 2);
    CHECK(project.num_sequences == 2);
    CHECK(project.sequences[0].length_ticks == 8u * project.ppqn);
    CHECK(project.sequences[0].tracks[0].config == &project.track_configs[0]);
    CHECK(project.sequences[1].n_repeats == 4);
    CHECK(MM_Proj_File_write(&project, "tests/roundtrip.yaml") == 0);
    CHECK(MM_Proj_File_read("tests/roundtrip.yaml", &copy) == 0);
    CHECK(strcmp(copy.name, project.name) == 0);
    CHECK(copy.num_track_configs == project.num_track_configs);
    CHECK(copy.num_sequences == project.num_sequences);
    CHECK(copy.sequences[1].n_repeats == project.sequences[1].n_repeats);
    CHECK(copy.sequences[0].tracks[0].assignments[0].start_ticks
          == project.sequences[0].tracks[0].assignments[0].start_ticks);
    MM_Proj_File_free(&copy);
    MM_Proj_File_free(&project);
    remove("tests/roundtrip.yaml");
    return 0;
}

static int test_runtime_project(void)
{
    MM_Project project;
    CHECK(MM_Project_init(&project, "test_proj.yaml") == 0);
    CHECK(project.n_tracks == 2 && project.n_sequences == 2);
    CHECK(HASH_COUNT(project.midi_map) == 3);
    CHECK(project.sequence_arr[0].tracks[0].n_clips == 2);
    CHECK(project.sequence_arr[0].tracks[0].clips[0].end_tick
          == project.sequence_arr[0].tracks[0].clips[1].start_tick);
    CHECK(project.sequence_arr[0].tracks[0].clips[0].midi
          == project.sequence_arr[0].tracks[0].clips[1].midi);
    MM_Project_free(&project);
    return 0;
}

static int test_offline_sequencer(void)
{
    MM_Project project;
    MM_AudioEngine engine;
    MM_Ring_Buffer *queue;
    MM_AudioCommand command = {MM_CMD_PLAY};
    float *output;
    size_t i;
    bool heard_audio = false;
    const size_t intro_samples = 176000;
    const size_t verse_samples = 352000;

    CHECK(MM_Project_init(&project, "test_proj.yaml") == 0);
    queue = MM_Ring_Buffer__init(8, sizeof(command));
    CHECK(queue != NULL);
    CHECK(MM_AudioEngine_init_offline(&engine, &project, queue) == 0);
    output = malloc(verse_samples * sizeof(*output));
    CHECK(output != NULL);
    CHECK(MM_Ring_Buffer__push(queue, &command));
    MM_AudioEngine_render(&engine, output, intro_samples);
    CHECK(engine.active_sequence_index == 1 && engine.active_sequence_play == 1);
    for (i = 0; i < intro_samples; i++)
        if (output[i] != 0.0f) { heard_audio = true; break; }
    CHECK(heard_audio);
    for (i = 0; i < 4; i++) MM_AudioEngine_render(&engine, output, verse_samples);
    CHECK(engine.finished && !engine.playing);
    CHECK(engine.total_samples == intro_samples + 4 * verse_samples);
    CHECK(MM_Ring_Buffer__push(queue, &command));
    MM_AudioEngine_render(&engine, output, intro_samples + 100);
    CHECK(engine.active_sequence_index == 1 && engine.sequence_samples == 100);
    CHECK(engine.total_samples == intro_samples + 100 && engine.playing);
    free(output);
    MM_AudioEngine_destroy(&engine);
    MM_Ring_Buffer__free(queue);
    MM_Project_free(&project);
    return 0;
}

static int test_infinite_sequence(void)
{
    MM_Project project;
    MM_AudioEngine engine;
    MM_Ring_Buffer *queue;
    MM_AudioCommand command = {MM_CMD_PLAY};
    float *output;
    const size_t intro_samples = 176000;
    CHECK(MM_Project_init(&project, "test_proj.yaml") == 0);
    project.sequence_arr[0].n_repeats = 0;
    queue = MM_Ring_Buffer__init(4, sizeof(command));
    CHECK(queue != NULL);
    CHECK(MM_AudioEngine_init_offline(&engine, &project, queue) == 0);
    output = malloc((intro_samples + 100) * sizeof(*output));
    CHECK(output != NULL && MM_Ring_Buffer__push(queue, &command));
    MM_AudioEngine_render(&engine, output, intro_samples + 100);
    CHECK(engine.playing && engine.active_sequence_index == 0);
    CHECK(engine.active_sequence_play == 1 && engine.sequence_samples == 100);
    free(output);
    MM_AudioEngine_destroy(&engine);
    MM_Ring_Buffer__free(queue);
    MM_Project_free(&project);
    return 0;
}

static int test_format_one_and_natural_length(void)
{
    static const unsigned char midi[] = {
        'M','T','h','d', 0,0,0,6, 0,1, 0,2, 0,96,
        'M','T','r','k', 0,0,0,12,
        0x00,0x90,0x3c,0x40, 0x60,0x80,0x3c,0x00, 0x00,0xff,0x2f,0x00,
        'M','T','r','k', 0,0,0,12,
        0x00,0x90,0x40,0x40, 0x30,0x80,0x40,0x00, 0x00,0xff,0x2f,0x00
    };
    static const char yaml[] =
        "name: natural\n"
        "tempo: 120\n"
        "sample_rate: 44100\n"
        "ppqn: 192\n"
        "beat_per_bar: 4\n"
        "track-config:\n"
        "  lead: {wave: SIN, gain: 0.5}\n"
        "sequence:\n"
        "  - name: only\n"
        "    length: 2b\n"
        "    n_repeats: 1\n"
        "    tracks:\n"
        "      lead:\n"
        "        - {midi: type1.mid, start: 0, loop: false}\n";
    MM_Midi_File file;
    MM_Project project;
    CHECK(write_bytes("tests/type1.mid", midi, sizeof(midi)) == 0);
    CHECK(write_text("tests/natural.yaml", yaml) == 0);
    CHECK(MM_File_init(&file, "tests/type1.mid") == 0);
    CHECK(file.header.format == 1 && file.header.ntrks == 2);
    CHECK(file.track.n_events == 4 && file.track.total_ticks == 96);
    MM_File_free(&file);
    CHECK(MM_Project_init(&project, "tests/natural.yaml") == 0);
    CHECK(project.sequence_arr[0].tracks[0].clips[0].duration_ticks == 192);
    MM_Project_free(&project);
    CHECK(write_bytes("tests/type1.mid", midi, 10) == 0);
    CHECK(MM_File_init(&file, "tests/type1.mid") != 0);
    remove("tests/natural.yaml");
    remove("tests/type1.mid");
    return 0;
}

static int test_invalid_projects(void)
{
    static const char unknown_track[] =
        "name: bad\ntempo: 120\nsample_rate: 44100\nppqn: 96\nbeat_per_bar: 4\n"
        "track-config:\n  one: {wave: SIN, gain: 0.5}\n"
        "sequence:\n  - name: s\n    length: 1B\n    tracks:\n"
        "      missing:\n        - {midi: x.mid}\n";
    static const char too_many_tracks[] =
        "name: bad\ntempo: 120\nsample_rate: 44100\nppqn: 96\nbeat_per_bar: 4\n"
        "track-config:\n"
        "  one: {wave: SIN, gain: 0.5}\n  two: {wave: SIN, gain: 0.5}\n"
        "  three: {wave: SIN, gain: 0.5}\n  four: {wave: SIN, gain: 0.5}\n"
        "  five: {wave: SIN, gain: 0.5}\n"
        "sequence:\n  - name: s\n    length: 1B\n    tracks: {}\n";
    static const char overlap[] =
        "name: bad\ntempo: 120\nsample_rate: 44100\nppqn: 960\nbeat_per_bar: 4\n"
        "track-config:\n  one: {wave: SIN, gain: 0.5}\n"
        "sequence:\n  - name: s\n    length: 1B\n    tracks:\n"
        "      one:\n"
        "        - {midi: ../midi_test_002.MID, start: 0, length: 2b}\n"
        "        - {midi: ../midi_test_002.MID, start: 1b, length: 2b}\n";
    MM_File_Project project;
    MM_Project runtime;
    CHECK(write_text("tests/invalid.yaml", unknown_track) == 0);
    CHECK(MM_Proj_File_read("tests/invalid.yaml", &project) != 0);
    CHECK(write_text("tests/invalid.yaml", too_many_tracks) == 0);
    CHECK(MM_Proj_File_read("tests/invalid.yaml", &project) != 0);
    CHECK(write_text("tests/invalid.yaml", overlap) == 0);
    CHECK(MM_Project_init(&runtime, "tests/invalid.yaml") != 0);
    remove("tests/invalid.yaml");
    return 0;
}

int main(void)
{
    if (test_time_parser() != 0 || test_project_file() != 0
        || test_runtime_project() != 0
        || test_offline_sequencer() != 0
        || test_infinite_sequence() != 0
        || test_format_one_and_natural_length() != 0
        || test_invalid_projects() != 0) return 1;
    puts("all refactor tests passed");
    return 0;
}

#define _POSIX_C_SOURCE 200809L
#include "minimidi-proj.h"

#include "minimidi-log.h"

#include <stdio.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

static uint64_t scale_event_tick(uint64_t tick, unsigned int from_ppqn,
                                 unsigned int to_ppqn)
{
    uint64_t quotient = tick / from_ppqn;
    uint64_t remainder = tick % from_ppqn;
    uint64_t scaled;

    if (quotient > UINT64_MAX / to_ppqn) return UINT64_MAX;
    quotient *= to_ppqn;
    if (remainder > (UINT64_MAX - from_ppqn / 2) / to_ppqn) return UINT64_MAX;
    scaled = (remainder * to_ppqn + from_ppqn / 2) / from_ppqn;
    if (UINT64_MAX - quotient < scaled) return UINT64_MAX;
    return quotient + scaled;
}

static MM_Mid_Map_Entry *midi_map_load(MM_Mid_Map_Entry **map,
                                        const char *path,
                                        unsigned int project_ppqn)
{
    MM_Mid_Map_Entry *entry;
    MM_Midi_File file;
    char *canonical = realpath(path, NULL);
    const char *key = canonical ? canonical : path;
    size_t i;

    entry = NULL;
    HASH_FIND_STR(*map, key, entry);
    if (entry) {
        free(canonical);
        return entry;
    }
    if (MM_File_init(&file, key) != 0) {
        free(canonical);
        return NULL;
    }
    entry = calloc(1, sizeof(*entry));
    if (!entry) {
        MM_File_free(&file);
        free(canonical);
        return NULL;
    }
    entry->key = strdup(key);
    if (!entry->key) goto error;
    if (file.track.n_events != 0) {
        if (file.track.n_events > SIZE_MAX / sizeof(*entry->project_event_ticks))
            goto error;
        entry->project_event_ticks = malloc(file.track.n_events
                                          * sizeof(*entry->project_event_ticks));
        if (!entry->project_event_ticks) goto error;
        for (i = 0; i < file.track.n_events; i++) {
            entry->project_event_ticks[i] = scale_event_tick(
                file.track.event_arr[i].abs_ticks, file.header.ppqn, project_ppqn);
            if (entry->project_event_ticks[i] == UINT64_MAX) goto error;
        }
    }
    entry->value = file;
    HASH_ADD_KEYPTR(hh, *map, entry->key, strlen(entry->key), entry);
    free(canonical);
    return entry;

error:
    free(entry->project_event_ticks);
    free(entry->key);
    free(entry);
    MM_File_free(&file);
    free(canonical);
    return NULL;
}

static void midi_map_free_all(MM_Mid_Map_Entry **map)
{
    MM_Mid_Map_Entry *current, *temporary;
    HASH_ITER(hh, *map, current, temporary) {
        HASH_DEL(*map, current);
        MM_File_free(&current->value);
        free(current->project_event_ticks);
        free(current->key);
        free(current);
    }
    *map = NULL;
}

static size_t track_index(const MM_Project *project,
                          const MM_File_TrackConfig *config)
{
    return (size_t)(config - project->file->track_configs);
}

static double wave_expr_sample(const MM_WaveExpr *expr, double phase)
{
    size_t i;
    double value;
    if (expr->kind == MM_WAVE_EXPR_SUM || expr->kind == MM_WAVE_EXPR_MULTIPLY) {
        value = expr->kind == MM_WAVE_EXPR_SUM ? 0.0 : 1.0;
        for (i = 0; i < expr->n_children; i++) {
            double child = wave_expr_sample(expr->children[i], phase);
            if (expr->kind == MM_WAVE_EXPR_SUM) value += child;
            else value *= child;
        }
        return value;
    }
    phase = fmod(phase * expr->frequency_multiplier + expr->phase / 360.0, 1.0);
    if (phase < 0.0) phase += 1.0;
    switch (expr->primitive) {
        case MM_WAVE_SIN: value = sin(2.0 * 3.14159265358979323846 * phase); break;
        case MM_WAVE_TRIANGLE: {
            double peak = (expr->tilt + 1.0) * 0.5;
            if (peak <= 0.0) value = 1.0 - 2.0 * phase;
            else if (peak >= 1.0) value = -1.0 + 2.0 * phase;
            else if (phase < peak) value = -1.0 + 2.0 * phase / peak;
            else value = 1.0 - 2.0 * (phase - peak) / (1.0 - peak);
            break;
        }
        case MM_WAVE_SQUARE:
            if (expr->lean > 0.0 && phase < expr->lean)
                value = -1.0 + 2.0 * phase / expr->lean;
            else value = phase < expr->split ? 1.0 : -1.0;
            break;
        default: value = 0.0; break;
    }
    return value * expr->amplitude;
}

static int compile_wave_tables(MM_Project *project)
{
    size_t i, j;
    project->n_wave_tables = project->file->num_wave_shapes;
    project->wave_tables = calloc(project->n_wave_tables, sizeof(*project->wave_tables));
    if (project->n_wave_tables && !project->wave_tables) return -1;
    for (i = 0; i < project->n_wave_tables; i++) {
        project->wave_tables[i].name = project->file->wave_shapes[i].name;
        for (j = 0; j < MM_WAVETABLE_SIZE; j++) {
            double sample = wave_expr_sample(project->file->wave_shapes[i].expression,
                                             (double)j / MM_WAVETABLE_SIZE);
            if (!isfinite(sample)) return -1;
            project->wave_tables[i].samples[j] = sample;
        }
    }
    for (i = 0; i < project->n_tracks; i++) {
        size_t j;
        const char *name = project->file->track_configs[i].wave_name;
        if (!name) continue;
        for (j = 0; j < project->n_wave_tables; j++) {
            if (strcmp(name, project->wave_tables[j].name) == 0) {
                project->tracks_arr[i].wavetable = &project->wave_tables[j];
                break;
            }
        }
        if (!project->tracks_arr[i].wavetable) return -1;
    }
    return 0;
}

static uint64_t scale_duration_ticks(uint64_t ticks, unsigned int from_ppqn,
                                     unsigned int to_ppqn)
{
    uint64_t quotient = ticks / from_ppqn;
    uint64_t remainder = ticks % from_ppqn;
    if (quotient > UINT64_MAX / to_ppqn) return UINT64_MAX;
    quotient *= to_ppqn;
    if (remainder != 0) {
        uint64_t scaled;
        if (remainder > (UINT64_MAX - (from_ppqn - 1)) / to_ppqn) return UINT64_MAX;
        scaled = (remainder * to_ppqn + from_ppqn - 1) / from_ppqn;
        if (UINT64_MAX - quotient < scaled) return UINT64_MAX;
        quotient += scaled;
    }
    return quotient;
}

static int compare_clips(const void *left, const void *right)
{
    const MM_Clip *a = left;
    const MM_Clip *b = right;
    if (a->start_tick < b->start_tick) return -1;
    if (a->start_tick > b->start_tick) return 1;
    return 0;
}

static int compile_sequence(MM_Project *project, size_t sequence_index)
{
    MM_File_Sequence *source = &project->file->sequences[sequence_index];
    MM_Sequence *target = &project->sequence_arr[sequence_index];
    size_t i, j;

    target->name = source->name;
    target->length_ticks = source->length_ticks;
    target->length_beats = source->length_beats;
    target->n_repeats = source->n_repeats;
    target->tracks = calloc(project->n_tracks, sizeof(*target->tracks));
    if (!target->tracks) return -1;

    for (i = 0; i < source->num_tracks; i++) {
        MM_File_Sequence_Track *source_track = &source->tracks[i];
        size_t index = track_index(project, source_track->config);
        MM_SequenceTrack *target_track = &target->tracks[index];
        target_track->n_clips = source_track->num_assignments;
        target_track->clips = calloc(target_track->n_clips, sizeof(*target_track->clips));
        if (!target_track->clips) return -1;

        for (j = 0; j < target_track->n_clips; j++) {
            MM_File_Sequence_Track_Midi_Assignment *assignment = &source_track->assignments[j];
            MM_Clip *clip = &target_track->clips[j];
            MM_Mid_Map_Entry *midi_entry = midi_map_load(&project->midi_map,
                assignment->resolved_midi_path, project->ppqn);
            if (!midi_entry) {
                fprintf(stderr, "%s: sequence '%s', track '%s': cannot load MIDI '%s'\n",
                        project->file->filepath, source->name, source_track->track_name,
                        assignment->midi_path);
                return -1;
            }
            clip->midi = &midi_entry->value;
            clip->project_event_ticks = midi_entry->project_event_ticks;
            clip->source_duration_ticks = scale_duration_ticks(
                clip->midi->track.total_ticks, clip->midi->header.ppqn,
                project->ppqn);
            if (clip->source_duration_ticks == 0 || clip->source_duration_ticks == UINT64_MAX)
                return -1;
            clip->start_tick = assignment->start_ticks;
            clip->duration_ticks = assignment->has_length
                ? assignment->length_ticks : clip->source_duration_ticks;
            clip->loop = assignment->loop;
            if (clip->duration_ticks > UINT64_MAX - clip->start_tick) return -1;
            clip->end_tick = clip->start_tick + clip->duration_ticks;
            if (clip->end_tick > target->length_ticks) {
                fprintf(stderr, "%s: sequence '%s', track '%s': clip exceeds sequence length\n",
                        project->file->filepath, source->name, source_track->track_name);
                return -1;
            }
        }
        qsort(target_track->clips, target_track->n_clips,
              sizeof(*target_track->clips), compare_clips);
        for (j = 1; j < target_track->n_clips; j++) {
            if (target_track->clips[j].start_tick < target_track->clips[j - 1].end_tick) {
                fprintf(stderr, "%s: sequence '%s', track '%s': assignments overlap\n",
                        project->file->filepath, source->name, source_track->track_name);
                return -1;
            }
        }
    }
    return 0;
}

int MM_Project_init(MM_Project *project, const char *filepath)
{
    size_t i;
    if (!project || !filepath) return -1;
    memset(project, 0, sizeof(*project));
    project->file = calloc(1, sizeof(*project->file));
    if (!project->file || MM_Proj_File_read(filepath, project->file) != 0) goto error;
    if (project->file->num_track_configs > MM_MAX_TRACKS) goto error;

    project->ppqn = project->file->ppqn;
    project->n_tracks = project->file->num_track_configs;
    project->n_sequences = project->file->num_sequences;
    project->tracks_arr = calloc(project->n_tracks, sizeof(*project->tracks_arr));
    project->sequence_arr = calloc(project->n_sequences, sizeof(*project->sequence_arr));
    if (!project->tracks_arr || !project->sequence_arr) goto error;

    for (i = 0; i < project->n_tracks; i++) {
        project->tracks_arr[i].config_file = &project->file->track_configs[i];
        project->tracks_arr[i].name = project->file->track_configs[i].name;
        project->tracks_arr[i].wave = project->file->track_configs[i].wave;
        project->tracks_arr[i].gain = project->file->track_configs[i].gain;
    }
    if (compile_wave_tables(project) != 0) goto error;
    for (i = 0; i < project->n_sequences; i++)
        if (compile_sequence(project, i) != 0) goto error;
    return 0;

error:
    MM_Project_free(project);
    return -1;
}

void MM_Project_free(MM_Project *project)
{
    size_t i, j;
    if (!project) return;
    for (i = 0; i < project->n_sequences; i++) {
        if (!project->sequence_arr) break;
        for (j = 0; j < project->n_tracks; j++)
            free(project->sequence_arr[i].tracks
                 ? project->sequence_arr[i].tracks[j].clips : NULL);
        free(project->sequence_arr[i].tracks);
    }
    free(project->sequence_arr);
    free(project->tracks_arr);
    free(project->wave_tables);
    midi_map_free_all(&project->midi_map);
    if (project->file) {
        MM_Proj_File_free(project->file);
        free(project->file);
    }
    memset(project, 0, sizeof(*project));
}

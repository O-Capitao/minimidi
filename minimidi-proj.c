#define _POSIX_C_SOURCE 200809L
#include "minimidi-proj.h"

#include "minimidi-log.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static MM_Midi_File *midi_map_load(MM_Mid_Map_Entry **map, const char *path)
{
    MM_Mid_Map_Entry *entry;
    MM_Midi_File file;
    char *canonical = realpath(path, NULL);
    const char *key = canonical ? canonical : path;

    entry = NULL;
    HASH_FIND_STR(*map, key, entry);
    if (entry) {
        free(canonical);
        return &entry->value;
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
    if (!entry->key) {
        MM_File_free(&file);
        free(entry);
        free(canonical);
        return NULL;
    }
    entry->value = file;
    HASH_ADD_KEYPTR(hh, *map, entry->key, strlen(entry->key), entry);
    free(canonical);
    return &entry->value;
}

static void midi_map_free_all(MM_Mid_Map_Entry **map)
{
    MM_Mid_Map_Entry *current, *temporary;
    HASH_ITER(hh, *map, current, temporary) {
        HASH_DEL(*map, current);
        MM_File_free(&current->value);
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

static uint64_t scale_ticks(uint64_t ticks, unsigned int from_ppqn,
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
            clip->midi = midi_map_load(&project->midi_map, assignment->resolved_midi_path);
            if (!clip->midi) {
                fprintf(stderr, "%s: sequence '%s', track '%s': cannot load MIDI '%s'\n",
                        project->file->filepath, source->name, source_track->track_name,
                        assignment->midi_path);
                return -1;
            }
            clip->source_duration_ticks = scale_ticks(clip->midi->track.total_ticks,
                                                       clip->midi->header.ppqn,
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
    midi_map_free_all(&project->midi_map);
    if (project->file) {
        MM_Proj_File_free(project->file);
        free(project->file);
    }
    memset(project, 0, sizeof(*project));
}

#define _POSIX_C_SOURCE 200809L
#include "minimidi-proj-file.h"

#include "minimidi-log.h"

#include <ctype.h>
#include <errno.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <yaml.h>

const char *mm_wave_to_str(MM_WaveType type)
{
    switch (type) {
        case MM_WAVE_SQUARE: return "SQUARE";
        case MM_WAVE_TRIANGLE: return "TRIANGLE";
        case MM_WAVE_SIN: return "SIN";
    }
    return "INVALID";
}

bool mm_wave_from_str(const char *text, MM_WaveType *type)
{
    if (!text || !type) return false;
    if (strcmp(text, "SQUARE") == 0) *type = MM_WAVE_SQUARE;
    else if (strcmp(text, "TRIANGLE") == 0) *type = MM_WAVE_TRIANGLE;
    else if (strcmp(text, "SIN") == 0) *type = MM_WAVE_SIN;
    else return false;
    return true;
}

static int fail(const char *path, const char *message)
{
    fprintf(stderr, "%s: %s\n", path ? path : "project", message);
    log_error("%s: %s", path ? path : "project", message);
    return -1;
}

static const char *scalar(const yaml_node_t *node)
{
    return node && node->type == YAML_SCALAR_NODE
        ? (const char *)node->data.scalar.value : NULL;
}

static char *duplicate_scalar(const yaml_node_t *node)
{
    const char *value = scalar(node);
    return value ? strdup(value) : NULL;
}

static int parse_uint(const yaml_node_t *node, unsigned int *value)
{
    const char *text = scalar(node);
    char *end;
    unsigned long parsed;
    if (!text || !*text || *text == '-') return -1;
    errno = 0;
    parsed = strtoul(text, &end, 10);
    if (errno || *end != '\0' || parsed > UINT32_MAX) return -1;
    *value = (unsigned int)parsed;
    return 0;
}

static int parse_gain(const yaml_node_t *node, double *value)
{
    const char *text = scalar(node);
    char *end;
    double parsed;
    if (!text || !*text) return -1;
    errno = 0;
    parsed = strtod(text, &end);
    if (errno || *end != '\0' || !isfinite(parsed)) return -1;
    *value = parsed;
    return 0;
}

static int parse_bool(const yaml_node_t *node, bool *value)
{
    const char *text = scalar(node);
    if (!text) return -1;
    if (strcasecmp(text, "true") == 0 || strcasecmp(text, "yes") == 0
        || strcmp(text, "1") == 0) *value = true;
    else if (strcasecmp(text, "false") == 0 || strcasecmp(text, "no") == 0
             || strcmp(text, "0") == 0) *value = false;
    else return -1;
    return 0;
}

static int checked_add(uint64_t *total, uint64_t value)
{
    if (UINT64_MAX - *total < value) return -1;
    *total += value;
    return 0;
}

int MM_Proj_File_parse_time(const char *text, unsigned int beats_per_bar,
                            unsigned int ppqn, uint64_t *ticks, double *beats)
{
    const char *cursor = text;
    int previous_unit = -1;
    uint64_t total = 0;

    if (!text || !ticks || !beats || beats_per_bar == 0 || ppqn == 0) return -1;
    if (strcmp(text, "0") == 0) {
        *ticks = 0;
        *beats = 0.0;
        return 0;
    }
    if (!*text) return -1;

    while (*cursor) {
        uint64_t number = 0;
        uint64_t multiplier;
        int unit;
        if (!isdigit((unsigned char)*cursor)) return -1;
        do {
            unsigned int digit = (unsigned int)(*cursor - '0');
            if (number > (UINT64_MAX - digit) / 10) return -1;
            number = number * 10 + digit;
            cursor++;
        } while (isdigit((unsigned char)*cursor));

        if (*cursor == 'B') unit = 0;
        else if (*cursor == 'b') unit = 1;
        else if (*cursor == 't') unit = 2;
        else return -1;
        if (unit <= previous_unit) return -1;
        previous_unit = unit;
        cursor++;

        if (unit == 0) {
            if (beats_per_bar > UINT64_MAX / ppqn) return -1;
            multiplier = (uint64_t)beats_per_bar * ppqn;
        } else if (unit == 1) multiplier = ppqn;
        else multiplier = 1;
        if (number != 0 && multiplier > UINT64_MAX / number) return -1;
        if (checked_add(&total, number * multiplier) != 0) return -1;
    }
    *ticks = total;
    *beats = (double)total / ppqn;
    return 0;
}

static int grow(void **array, size_t *count, size_t item_size)
{
    void *result;
    if (*count == SIZE_MAX || (*count + 1) > SIZE_MAX / item_size) return -1;
    result = realloc(*array, (*count + 1) * item_size);
    if (!result) return -1;
    *array = result;
    memset((char *)result + *count * item_size, 0, item_size);
    (*count)++;
    return 0;
}

static char *resolve_path(const char *project_path, const char *midi_path)
{
    const char *slash;
    size_t directory_length;
    size_t midi_length;
    char *result;
    if (!midi_path) return NULL;
    if (midi_path[0] == '/') return strdup(midi_path);
    slash = strrchr(project_path, '/');
    directory_length = slash ? (size_t)(slash - project_path) : 1;
    midi_length = strlen(midi_path);
    result = malloc(directory_length + 1 + midi_length + 1);
    if (!result) return NULL;
    if (slash) memcpy(result, project_path, directory_length);
    else result[0] = '.';
    result[directory_length] = '/';
    memcpy(result + directory_length + 1, midi_path, midi_length + 1);
    return result;
}

static MM_File_TrackConfig *find_config(MM_File_Project *project, const char *name)
{
    size_t i;
    for (i = 0; i < project->num_track_configs; i++)
        if (strcmp(project->track_configs[i].name, name) == 0)
            return &project->track_configs[i];
    return NULL;
}

static int parse_track_configs(yaml_document_t *document, yaml_node_t *mapping,
                               MM_File_Project *project, const char *path)
{
    yaml_node_pair_t *pair;
    if (!mapping || mapping->type != YAML_MAPPING_NODE)
        return fail(path, "track-config must be a mapping");
    for (pair = mapping->data.mapping.pairs.start;
         pair < mapping->data.mapping.pairs.top; pair++) {
        const char *name = scalar(yaml_document_get_node(document, pair->key));
        yaml_node_t *value = yaml_document_get_node(document, pair->value);
        MM_File_TrackConfig *config;
        yaml_node_pair_t *field;
        bool saw_wave = false, saw_gain = false;
        if (!name || !*name || !value || value->type != YAML_MAPPING_NODE)
            return fail(path, "each track-config entry must be a named mapping");
        if (find_config(project, name)) return fail(path, "duplicate track name");
        if (project->num_track_configs >= MM_MAX_TRACKS)
            return fail(path, "project exceeds the four-track limit");
        if (grow((void **)&project->track_configs, &project->num_track_configs,
                 sizeof(*project->track_configs)) != 0) return fail(path, "out of memory");
        config = &project->track_configs[project->num_track_configs - 1];
        config->name = strdup(name);
        if (!config->name) return fail(path, "out of memory");
        for (field = value->data.mapping.pairs.start;
             field < value->data.mapping.pairs.top; field++) {
            const char *key = scalar(yaml_document_get_node(document, field->key));
            yaml_node_t *node = yaml_document_get_node(document, field->value);
            if (!key) return fail(path, "track-config keys must be scalars");
            if (strcmp(key, "wave") == 0) {
                if (saw_wave || !mm_wave_from_str(scalar(node), &config->wave))
                    return fail(path, "invalid or duplicate track wave");
                saw_wave = true;
            } else if (strcmp(key, "gain") == 0) {
                if (saw_gain || parse_gain(node, &config->gain) != 0
                    || config->gain < 0.0 || config->gain > 1.0)
                    return fail(path, "track gain must be between 0 and 1");
                saw_gain = true;
            } else return fail(path, "unknown track-config property");
        }
        if (!saw_wave || !saw_gain) return fail(path, "track wave and gain are required");
    }
    return 0;
}

static int parse_assignment(yaml_document_t *document, yaml_node_t *mapping,
                            MM_File_Sequence_Track *track,
                            MM_File_Project *project, const char *path)
{
    MM_File_Sequence_Track_Midi_Assignment *assignment;
    yaml_node_pair_t *field;
    bool saw_midi = false, saw_start = false, saw_loop = false, saw_length = false;
    if (!mapping || mapping->type != YAML_MAPPING_NODE)
        return fail(path, "MIDI assignment must be a mapping");
    if (grow((void **)&track->assignments, &track->num_assignments,
             sizeof(*track->assignments)) != 0) return fail(path, "out of memory");
    assignment = &track->assignments[track->num_assignments - 1];
    for (field = mapping->data.mapping.pairs.start;
         field < mapping->data.mapping.pairs.top; field++) {
        const char *key = scalar(yaml_document_get_node(document, field->key));
        yaml_node_t *node = yaml_document_get_node(document, field->value);
        if (!key) return fail(path, "assignment keys must be scalars");
        if (strcmp(key, "midi") == 0) {
            if (saw_midi || !(assignment->midi_path = duplicate_scalar(node))
                || !*assignment->midi_path) return fail(path, "assignment midi must be a path");
            saw_midi = true;
        } else if (strcmp(key, "start") == 0) {
            if (saw_start || !(assignment->start = duplicate_scalar(node)))
                return fail(path, "assignment start must be a time string");
            saw_start = true;
        } else if (strcmp(key, "loop") == 0) {
            if (saw_loop || parse_bool(node, &assignment->loop) != 0)
                return fail(path, "assignment loop must be boolean");
            saw_loop = true;
        } else if (strcmp(key, "length") == 0) {
            if (saw_length || !(assignment->length = duplicate_scalar(node)))
                return fail(path, "assignment length must be a time string");
            assignment->has_length = true;
            saw_length = true;
        } else return fail(path, "unknown assignment property");
    }
    if (!saw_midi) return fail(path, "assignment midi is required");
    if (!assignment->start && !(assignment->start = strdup("0"))) return fail(path, "out of memory");
    if (MM_Proj_File_parse_time(assignment->start, project->beat_per_bar,
                                project->ppqn, &assignment->start_ticks,
                                &assignment->start_beats) != 0)
        return fail(path, "invalid assignment start");
    if (assignment->has_length
        && (MM_Proj_File_parse_time(assignment->length, project->beat_per_bar,
                                    project->ppqn, &assignment->length_ticks,
                                    &assignment->length_beats) != 0
            || assignment->length_ticks == 0))
        return fail(path, "assignment length must be a positive time");
    assignment->resolved_midi_path = resolve_path(project->filepath, assignment->midi_path);
    if (!assignment->resolved_midi_path) return fail(path, "out of memory");
    return 0;
}

static int parse_sequence_tracks(yaml_document_t *document, yaml_node_t *mapping,
                                 MM_File_Sequence *sequence,
                                 MM_File_Project *project, const char *path)
{
    yaml_node_pair_t *pair;
    if (!mapping || mapping->type != YAML_MAPPING_NODE)
        return fail(path, "sequence tracks must be a mapping");
    for (pair = mapping->data.mapping.pairs.start;
         pair < mapping->data.mapping.pairs.top; pair++) {
        const char *name = scalar(yaml_document_get_node(document, pair->key));
        yaml_node_t *items = yaml_document_get_node(document, pair->value);
        MM_File_Sequence_Track *track;
        yaml_node_item_t *item;
        size_t i;
        if (!name || !*name || !items || items->type != YAML_SEQUENCE_NODE)
            return fail(path, "sequence track must contain an assignment list");
        for (i = 0; i < sequence->num_tracks; i++)
            if (strcmp(sequence->tracks[i].track_name, name) == 0)
                return fail(path, "duplicate track in sequence");
        if (grow((void **)&sequence->tracks, &sequence->num_tracks,
                 sizeof(*sequence->tracks)) != 0) return fail(path, "out of memory");
        track = &sequence->tracks[sequence->num_tracks - 1];
        track->track_name = strdup(name);
        track->config = find_config(project, name);
        if (!track->track_name || !track->config)
            return fail(path, "sequence references an unknown track");
        for (item = items->data.sequence.items.start;
             item < items->data.sequence.items.top; item++)
            if (parse_assignment(document, yaml_document_get_node(document, *item),
                                 track, project, path) != 0) return -1;
        if (track->num_assignments == 0)
            return fail(path, "sequence track assignment list cannot be empty");
    }
    return 0;
}

static int parse_sequences(yaml_document_t *document, yaml_node_t *items,
                           MM_File_Project *project, const char *path)
{
    yaml_node_item_t *item;
    if (!items || items->type != YAML_SEQUENCE_NODE)
        return fail(path, "sequence must be a list");
    for (item = items->data.sequence.items.start;
         item < items->data.sequence.items.top; item++) {
        yaml_node_t *mapping = yaml_document_get_node(document, *item);
        MM_File_Sequence *sequence;
        yaml_node_pair_t *field;
        bool saw_name = false, saw_length = false, saw_repeats = false, saw_tracks = false;
        if (!mapping || mapping->type != YAML_MAPPING_NODE)
            return fail(path, "each sequence must be a mapping");
        if (grow((void **)&project->sequences, &project->num_sequences,
                 sizeof(*project->sequences)) != 0) return fail(path, "out of memory");
        sequence = &project->sequences[project->num_sequences - 1];
        sequence->n_repeats = 1;
        for (field = mapping->data.mapping.pairs.start;
             field < mapping->data.mapping.pairs.top; field++) {
            const char *key = scalar(yaml_document_get_node(document, field->key));
            yaml_node_t *node = yaml_document_get_node(document, field->value);
            if (!key) return fail(path, "sequence keys must be scalars");
            if (strcmp(key, "name") == 0) {
                if (saw_name || !(sequence->name = duplicate_scalar(node)) || !*sequence->name)
                    return fail(path, "sequence name is required");
                saw_name = true;
            } else if (strcmp(key, "length") == 0) {
                if (saw_length || !(sequence->length = duplicate_scalar(node)))
                    return fail(path, "sequence length must be a time string");
                saw_length = true;
            } else if (strcmp(key, "n_repeats") == 0 || strcmp(key, "loop") == 0) {
                if (saw_repeats || parse_uint(node, &sequence->n_repeats) != 0)
                    return fail(path, "invalid or duplicate sequence n_repeats");
                saw_repeats = true;
            } else if (strcmp(key, "tracks") == 0) {
                if (saw_tracks || parse_sequence_tracks(document, node, sequence,
                                                        project, path) != 0) return -1;
                saw_tracks = true;
            } else return fail(path, "unknown sequence property");
        }
        if (!saw_name || !saw_length || !saw_tracks)
            return fail(path, "sequence requires name, length, and tracks");
        {
            size_t previous;
            for (previous = 0; previous + 1 < project->num_sequences; previous++)
                if (strcmp(project->sequences[previous].name, sequence->name) == 0)
                    return fail(path, "duplicate sequence name");
        }
        if (MM_Proj_File_parse_time(sequence->length, project->beat_per_bar,
                                    project->ppqn, &sequence->length_ticks,
                                    &sequence->length_beats) != 0
            || sequence->length_ticks == 0)
            return fail(path, "sequence length must be a positive time");
    }
    return 0;
}

int MM_Proj_File_read(const char *filepath, MM_File_Project *project)
{
    FILE *stream = NULL;
    yaml_parser_t parser;
    yaml_document_t document;
    yaml_node_t *root;
    yaml_node_t *track_configs_node = NULL, *sequences_node = NULL;
    yaml_node_pair_t *pair;
    bool parser_ready = false, document_ready = false;
    unsigned int seen = 0;
    int result = -1;

    if (!filepath || !project) return -1;
    memset(project, 0, sizeof(*project));
    project->filepath = strdup(filepath);
    if (!project->filepath) return fail(filepath, "out of memory");
    stream = fopen(filepath, "rb");
    if (!stream) { fail(filepath, strerror(errno)); goto done; }
    if (!yaml_parser_initialize(&parser)) { fail(filepath, "cannot initialize YAML parser"); goto done; }
    parser_ready = true;
    yaml_parser_set_input_file(&parser, stream);
    if (!yaml_parser_load(&parser, &document)) {
        fail(filepath, parser.problem ? parser.problem : "invalid YAML");
        goto done;
    }
    document_ready = true;
    root = yaml_document_get_root_node(&document);
    if (!root || root->type != YAML_MAPPING_NODE) { fail(filepath, "root must be a mapping"); goto done; }

    for (pair = root->data.mapping.pairs.start; pair < root->data.mapping.pairs.top; pair++) {
        const char *key = scalar(yaml_document_get_node(&document, pair->key));
        yaml_node_t *node = yaml_document_get_node(&document, pair->value);
        unsigned int bit;
        if (!key) { fail(filepath, "project keys must be scalars"); goto done; }
        if (strcmp(key, "name") == 0) bit = 1u << 0;
        else if (strcmp(key, "tempo") == 0) bit = 1u << 1;
        else if (strcmp(key, "sample_rate") == 0) bit = 1u << 2;
        else if (strcmp(key, "ppqn") == 0) bit = 1u << 3;
        else if (strcmp(key, "beat_per_bar") == 0) bit = 1u << 4;
        else if (strcmp(key, "track-config") == 0) bit = 1u << 5;
        else if (strcmp(key, "sequence") == 0) bit = 1u << 6;
        else { fail(filepath, "unknown project property"); goto done; }
        if (seen & bit) { fail(filepath, "duplicate project property"); goto done; }
        seen |= bit;
        if (bit == (1u << 0)) project->name = duplicate_scalar(node);
        else if (bit == (1u << 1) && parse_uint(node, &project->tempo) != 0) goto invalid_scalar;
        else if (bit == (1u << 2) && parse_uint(node, &project->sample_rate) != 0) goto invalid_scalar;
        else if (bit == (1u << 3) && parse_uint(node, &project->ppqn) != 0) goto invalid_scalar;
        else if (bit == (1u << 4) && parse_uint(node, &project->beat_per_bar) != 0) goto invalid_scalar;
        else if (bit == (1u << 5)) track_configs_node = node;
        else if (bit == (1u << 6)) sequences_node = node;
        continue;
invalid_scalar:
        fail(filepath, "invalid numeric project property"); goto done;
    }
    if (seen != 0x7f || !project->name || !*project->name)
        { fail(filepath, "all project fields are required"); goto done; }
    if (project->tempo == 0 || project->tempo > 500 || project->sample_rate < 8000
        || project->sample_rate > 384000 || project->ppqn == 0 || project->ppqn > 32767
        || project->beat_per_bar == 0 || project->beat_per_bar > 32)
        { fail(filepath, "project tempo/rate/PPQN/time signature is out of range"); goto done; }
    if (parse_track_configs(&document, track_configs_node, project, filepath) != 0
        || project->num_track_configs == 0
        || parse_sequences(&document, sequences_node, project, filepath) != 0
        || project->num_sequences == 0) goto done;
    result = 0;

done:
    if (document_ready) yaml_document_delete(&document);
    if (parser_ready) yaml_parser_delete(&parser);
    if (stream) fclose(stream);
    if (result != 0) MM_Proj_File_free(project);
    return result;
}

void MM_Proj_File_free(MM_File_Project *project)
{
    size_t i, j, k;
    if (!project) return;
    free(project->name);
    free(project->filepath);
    for (i = 0; i < project->num_track_configs; i++) free(project->track_configs[i].name);
    free(project->track_configs);
    for (i = 0; i < project->num_sequences; i++) {
        MM_File_Sequence *sequence = &project->sequences[i];
        free(sequence->name);
        free(sequence->length);
        for (j = 0; j < sequence->num_tracks; j++) {
            MM_File_Sequence_Track *track = &sequence->tracks[j];
            free(track->track_name);
            for (k = 0; k < track->num_assignments; k++) {
                MM_File_Sequence_Track_Midi_Assignment *assignment = &track->assignments[k];
                free(assignment->midi_path);
                free(assignment->resolved_midi_path);
                free(assignment->start);
                free(assignment->length);
            }
            free(track->assignments);
        }
        free(sequence->tracks);
    }
    free(project->sequences);
    memset(project, 0, sizeof(*project));
}

static int emit_scalar(yaml_emitter_t *emitter, const char *value,
                       yaml_scalar_style_t style)
{
    yaml_event_t event;
    if (!value || !yaml_scalar_event_initialize(&event, NULL, NULL,
            (yaml_char_t *)value, (int)strlen(value), 1, 1, style)) return 0;
    return yaml_emitter_emit(emitter, &event);
}

static int emit_pair(yaml_emitter_t *emitter, const char *key, const char *value)
{
    return emit_scalar(emitter, key, YAML_PLAIN_SCALAR_STYLE)
        && emit_scalar(emitter, value, YAML_DOUBLE_QUOTED_SCALAR_STYLE);
}

static int emit_mapping(yaml_emitter_t *emitter, bool start)
{
    yaml_event_t event;
    if (start) {
        if (!yaml_mapping_start_event_initialize(&event, NULL, NULL, 1,
                                                  YAML_BLOCK_MAPPING_STYLE)) return 0;
    } else if (!yaml_mapping_end_event_initialize(&event)) return 0;
    return yaml_emitter_emit(emitter, &event);
}

static int emit_sequence(yaml_emitter_t *emitter, bool start)
{
    yaml_event_t event;
    if (start) {
        if (!yaml_sequence_start_event_initialize(&event, NULL, NULL, 1,
                                                   YAML_BLOCK_SEQUENCE_STYLE)) return 0;
    } else if (!yaml_sequence_end_event_initialize(&event)) return 0;
    return yaml_emitter_emit(emitter, &event);
}

int MM_Proj_File_write(const MM_File_Project *project, const char *filepath)
{
    FILE *stream;
    yaml_emitter_t emitter;
    yaml_event_t event;
    char number[64];
    size_t i, j, k;
    int ok = 1;
    if (!project || !filepath) return -1;
    stream = fopen(filepath, "wb");
    if (!stream) return fail(filepath, strerror(errno));
    if (!yaml_emitter_initialize(&emitter)) { fclose(stream); return -1; }
    yaml_emitter_set_output_file(&emitter, stream);
#define EMIT(expression) do { if (ok && !(expression)) ok = 0; } while (0)
    EMIT(yaml_stream_start_event_initialize(&event, YAML_UTF8_ENCODING)
         && yaml_emitter_emit(&emitter, &event));
    EMIT(yaml_document_start_event_initialize(&event, NULL, NULL, NULL, 0)
         && yaml_emitter_emit(&emitter, &event));
    EMIT(emit_mapping(&emitter, true));
    EMIT(emit_pair(&emitter, "name", project->name));
    snprintf(number, sizeof(number), "%u", project->tempo); EMIT(emit_pair(&emitter, "tempo", number));
    snprintf(number, sizeof(number), "%u", project->sample_rate); EMIT(emit_pair(&emitter, "sample_rate", number));
    snprintf(number, sizeof(number), "%u", project->ppqn); EMIT(emit_pair(&emitter, "ppqn", number));
    snprintf(number, sizeof(number), "%u", project->beat_per_bar); EMIT(emit_pair(&emitter, "beat_per_bar", number));
    EMIT(emit_scalar(&emitter, "track-config", YAML_PLAIN_SCALAR_STYLE));
    EMIT(emit_mapping(&emitter, true));
    for (i = 0; i < project->num_track_configs; i++) {
        const MM_File_TrackConfig *config = &project->track_configs[i];
        EMIT(emit_scalar(&emitter, config->name, YAML_PLAIN_SCALAR_STYLE));
        EMIT(emit_mapping(&emitter, true));
        EMIT(emit_pair(&emitter, "wave", mm_wave_to_str(config->wave)));
        snprintf(number, sizeof(number), "%.17g", config->gain); EMIT(emit_pair(&emitter, "gain", number));
        EMIT(emit_mapping(&emitter, false));
    }
    EMIT(emit_mapping(&emitter, false));
    EMIT(emit_scalar(&emitter, "sequence", YAML_PLAIN_SCALAR_STYLE));
    EMIT(emit_sequence(&emitter, true));
    for (i = 0; i < project->num_sequences; i++) {
        const MM_File_Sequence *sequence = &project->sequences[i];
        EMIT(emit_mapping(&emitter, true));
        EMIT(emit_pair(&emitter, "name", sequence->name));
        EMIT(emit_pair(&emitter, "length", sequence->length));
        snprintf(number, sizeof(number), "%u", sequence->n_repeats); EMIT(emit_pair(&emitter, "n_repeats", number));
        EMIT(emit_scalar(&emitter, "tracks", YAML_PLAIN_SCALAR_STYLE));
        EMIT(emit_mapping(&emitter, true));
        for (j = 0; j < sequence->num_tracks; j++) {
            const MM_File_Sequence_Track *track = &sequence->tracks[j];
            EMIT(emit_scalar(&emitter, track->track_name, YAML_PLAIN_SCALAR_STYLE));
            EMIT(emit_sequence(&emitter, true));
            for (k = 0; k < track->num_assignments; k++) {
                const MM_File_Sequence_Track_Midi_Assignment *assignment = &track->assignments[k];
                EMIT(emit_mapping(&emitter, true));
                EMIT(emit_pair(&emitter, "midi", assignment->midi_path));
                EMIT(emit_pair(&emitter, "start", assignment->start));
                EMIT(emit_pair(&emitter, "loop", assignment->loop ? "true" : "false"));
                if (assignment->has_length) EMIT(emit_pair(&emitter, "length", assignment->length));
                EMIT(emit_mapping(&emitter, false));
            }
            EMIT(emit_sequence(&emitter, false));
        }
        EMIT(emit_mapping(&emitter, false));
        EMIT(emit_mapping(&emitter, false));
    }
    EMIT(emit_sequence(&emitter, false));
    EMIT(emit_mapping(&emitter, false));
    EMIT(yaml_document_end_event_initialize(&event, 1) && yaml_emitter_emit(&emitter, &event));
    EMIT(yaml_stream_end_event_initialize(&event) && yaml_emitter_emit(&emitter, &event));
#undef EMIT
    yaml_emitter_delete(&emitter);
    if (fclose(stream) != 0) ok = 0;
    if (!ok) return fail(filepath, "failed to write YAML project");
    return 0;
}

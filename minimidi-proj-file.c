#include "minimidi-proj-file.h"
#include "minimidi-log.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

const char* mm_wave_to_str(MM_WaveType type) {
    switch (type) {
        case MM_WAVE_SQUARE: return "SQUARE";
        case MM_WAVE_TRIANGLE: return "TRIANGLE";
        case MM_WAVE_SIN: return "SIN";
        default: return "SQUARE";
    }
}

MM_WaveType mm_str_to_wave(const char* str) {
    if (!str) return MM_WAVE_SQUARE;
    if (strcmp(str, "SQUARE") == 0) return MM_WAVE_SQUARE;
    if (strcmp(str, "TRIANGLE") == 0) return MM_WAVE_TRIANGLE;
    if (strcmp(str, "SIN") == 0) return MM_WAVE_SIN;
    fprintf(stderr, "Unknown wave type: %s (defaulting to SQUARE)\n", str);
    return MM_WAVE_SQUARE;
}

// Helper: duplicate scalar value safely
static char* dup_scalar(const yaml_node_t* node) {
    if (node && node->type == YAML_SCALAR_NODE) {
        return strdup((const char*)node->data.scalar.value);
    }
    return NULL;
}

// Helper: scalar to unsigned int
static unsigned int scalar_to_uint(const yaml_node_t* node) {
    if (node && node->type == YAML_SCALAR_NODE) {
        return (unsigned int)strtoul((const char*)node->data.scalar.value, NULL, 10);
    }
    return 0;
}

// Helper: scalar to int
static int scalar_to_int(const yaml_node_t* node) {
    if (node && node->type == YAML_SCALAR_NODE) {
        return atoi((const char*)node->data.scalar.value);
    }
    return 0;
}

// Helper: scalar to double
static double scalar_to_double(const yaml_node_t* node) {
    if (node && node->type == YAML_SCALAR_NODE) {
        return atof((const char*)node->data.scalar.value);
    }
    return 0.0;
}

// Helper: scalar to bool (YAML true/false/yes/1)
static bool scalar_to_bool(const yaml_node_t* node) {
    if (!node || node->type != YAML_SCALAR_NODE) return false;
    const char* v = (const char*)node->data.scalar.value;
    return (strcmp(v, "true") == 0 || strcmp(v, "1") == 0 || strcmp(v, "yes") == 0);
}

// Dynamic append helpers
static void append_track_config(MM_File_Project* proj, MM_File_TrackConfig tc) {
    proj->track_configs = realloc(proj->track_configs, (proj->num_track_configs + 1) * sizeof(MM_File_TrackConfig));
    proj->track_configs[proj->num_track_configs++] = tc;
}

static void append_sequence(MM_File_Project* proj, MM_File_Sequence seq) {
    proj->sequences = realloc(proj->sequences, (proj->num_sequences + 1) * sizeof(MM_File_Sequence));
    proj->sequences[proj->num_sequences++] = seq;
}

static void append_sequence_track(MM_File_Sequence* seq, MM_File_Sequence_Track strack) {
    seq->tracks = realloc(seq->tracks, (seq->num_tracks + 1) * sizeof(MM_File_Sequence_Track));
    seq->tracks[seq->num_tracks++] = strack;
}

static void append_assignment(MM_File_Sequence_Track* strack, MM_File_Sequence_Track_Midi_Assignment ass) {
    strack->assignments = realloc(strack->assignments, (strack->num_assignments + 1) * sizeof(MM_File_Sequence_Track_Midi_Assignment));
    strack->assignments[strack->num_assignments++] = ass;
}

// Find config by name (for linking after parsing)
static MM_File_TrackConfig* find_track_config(MM_File_Project* proj, const char* name) {
    if (!name) return NULL;
    for (size_t i = 0; i < proj->num_track_configs; i++) {
        if (strcmp(proj->track_configs[i].name, name) == 0) {
            return &proj->track_configs[i];
        }
    }
    fprintf(stderr, "Error: Track name '%s' not found in track-config (required by spec)\n", name);
    return NULL;
}

// Link tracks to configs (and free temporary track_name)
static void link_sequence_tracks(MM_File_Project* proj) {
    for (size_t i = 0; i < proj->num_sequences; i++) {
        MM_File_Sequence* s = &proj->sequences[i];
        for (size_t j = 0; j < s->num_tracks; j++) {
            MM_File_Sequence_Track* st = &s->tracks[j];
            if (st->track_name) {
                st->config = find_track_config(proj, st->track_name);
                // free(st->track_name);
                // st->track_name = NULL;
                if (!st->config) {
                    // Error case - caller should check return of MM_File_read
                }
            }
        }
    }
}

// Parse track-config mapping
static void parse_track_configs(yaml_document_t* doc, yaml_node_t* map_node, MM_File_Project* proj) {
    if (!map_node || map_node->type != YAML_MAPPING_NODE) return;
    for (yaml_node_pair_t* p = map_node->data.mapping.pairs.start; p < map_node->data.mapping.pairs.top; p++) {
        yaml_node_t* k = yaml_document_get_node(doc, p->key);
        yaml_node_t* v = yaml_document_get_node(doc, p->value);
        char* tname = dup_scalar(k);
        if (!tname) continue;
        if (v && v->type == YAML_MAPPING_NODE) {
            MM_File_TrackConfig tc = {0};
            tc.name = tname;
            tc.wave = MM_WAVE_SQUARE;
            tc.gain = 0.0;
            for (yaml_node_pair_t* tp = v->data.mapping.pairs.start; tp < v->data.mapping.pairs.top; tp++) {
                yaml_node_t* tk = yaml_document_get_node(doc, tp->key);
                yaml_node_t* tv = yaml_document_get_node(doc, tp->value);
                char* tkey = dup_scalar(tk);
                if (tkey) {
                    if (strcmp(tkey, "wave") == 0) {
                        char* w = dup_scalar(tv);
                        if (w) {
                            tc.wave = mm_str_to_wave(w);
                            free(w);
                        }
                    } else if (strcmp(tkey, "gain") == 0) {
                        tc.gain = scalar_to_double(tv);
                    }
                    free(tkey);
                }
            }
            append_track_config(proj, tc);
        } else {
            free(tname);
        }
    }
}

// Parse a single MIDI assignment mapping
static void parse_assignment(yaml_document_t* doc, yaml_node_t* map_node, MM_File_Sequence_Track_Midi_Assignment* ass) {
    if (!map_node || map_node->type != YAML_MAPPING_NODE) return;
    for (yaml_node_pair_t* p = map_node->data.mapping.pairs.start; p < map_node->data.mapping.pairs.top; p++) {
        yaml_node_t* k = yaml_document_get_node(doc, p->key);
        yaml_node_t* v = yaml_document_get_node(doc, p->value);
        char* akey = dup_scalar(k);
        if (!akey) continue;
        if (strcmp(akey, "midi") == 0) {
            ass->midi_path = dup_scalar(v);
        } else if (strcmp(akey, "start") == 0) {
            ass->start = dup_scalar(v);
        } else if (strcmp(akey, "loop") == 0) {
            ass->loop = scalar_to_bool(v);
        } else if (strcmp(akey, "length") == 0) {
            ass->length = dup_scalar(v);
        }
        free(akey);
    }
    if (!ass->start) ass->start = strdup("0"); // spec default
}

// Parse tracks map inside a sequence
static void parse_sequence_tracks(yaml_document_t* doc, yaml_node_t* tracks_map, MM_File_Sequence* seq) {
    if (!tracks_map || tracks_map->type != YAML_MAPPING_NODE) return;
    for (yaml_node_pair_t* p = tracks_map->data.mapping.pairs.start; p < tracks_map->data.mapping.pairs.top; p++) {
        yaml_node_t* k = yaml_document_get_node(doc, p->key);
        yaml_node_t* v = yaml_document_get_node(doc, p->value);
        char* tname = dup_scalar(k);
        if (!tname) continue;
        if (v && v->type == YAML_SEQUENCE_NODE) {
            MM_File_Sequence_Track strack = {0};
            strack.track_name = tname;
            strack.assignments = NULL;
            strack.num_assignments = 0;
            for (yaml_node_item_t* item = v->data.sequence.items.start; item < v->data.sequence.items.top; item++) {
                yaml_node_t* amap = yaml_document_get_node(doc, *item);
                if (amap && amap->type == YAML_MAPPING_NODE) {
                    MM_File_Sequence_Track_Midi_Assignment ass = {0};
                    parse_assignment(doc, amap, &ass);
                    if (ass.midi_path) {
                        append_assignment(&strack, ass);
                    } else {
                        free(ass.midi_path);
                        free(ass.start);
                        free(ass.length);
                    }
                }
            }
            append_sequence_track(seq, strack);
        } else {
            free(tname);
        }
    }
}

// Parse one sequence item (mapping)
static void parse_sequence_item(yaml_document_t* doc, yaml_node_t* map_node, MM_File_Project* proj) {
    if (!map_node || map_node->type != YAML_MAPPING_NODE) return;
    MM_File_Sequence seq = {0};
    seq.loop_count = 1; // default per spec
    for (yaml_node_pair_t* p = map_node->data.mapping.pairs.start; p < map_node->data.mapping.pairs.top; p++) {
        yaml_node_t* k = yaml_document_get_node(doc, p->key);
        yaml_node_t* v = yaml_document_get_node(doc, p->value);
        char* key = dup_scalar(k);
        if (!key) continue;
        if (strcmp(key, "name") == 0) {
            seq.name = dup_scalar(v);
        } else if (strcmp(key, "length") == 0) {
            seq.length = dup_scalar(v);
        } else if (strcmp(key, "loop") == 0) {
            seq.loop_count = scalar_to_int(v);
        } else if (strcmp(key, "tracks") == 0) {
            parse_sequence_tracks(doc, v, &seq);
        }
        free(key);
    }
    if (seq.name && seq.length) {
        append_sequence(proj, seq);
    } else {
        // cleanup partial
        free(seq.name);
        free(seq.length);
        for (size_t j = 0; j < seq.num_tracks; j++) {
            free(seq.tracks[j].track_name);
            for (size_t k = 0; k < seq.tracks[j].num_assignments; k++) {
                free(seq.tracks[j].assignments[k].midi_path);
                free(seq.tracks[j].assignments[k].start);
                free(seq.tracks[j].assignments[k].length);
            }
            free(seq.tracks[j].assignments);
        }
        free(seq.tracks);
    }
}

// Parse sequence list
static void parse_sequences(yaml_document_t* doc, yaml_node_t* seq_node, MM_File_Project* proj) {
    if (!seq_node || seq_node->type != YAML_SEQUENCE_NODE) return;
    for (yaml_node_item_t* item = seq_node->data.sequence.items.start; item < seq_node->data.sequence.items.top; item++) {
        yaml_node_t* smap = yaml_document_get_node(doc, *item);
        parse_sequence_item(doc, smap, proj);
    }
}

int MM_Proj_File_read(const char* filepath, MM_File_Project* project) {
    if (!project) {
        log_error( "MM_Fle_read: NULL project pointer passed.");
        return -1;
    }

    memset(project, 0, sizeof(*project));
    FILE* fh = fopen(filepath, "rb");
    if (!fh) {
        fprintf(stderr, "Cannot open YAML file: %s\n", filepath);
        return -1;
    }
    yaml_parser_t parser;
    yaml_document_t document;
    if (!yaml_parser_initialize(&parser)) {
        fclose(fh);
        return -1;
    }
    yaml_parser_set_input_file(&parser, fh);
    if (!yaml_parser_load(&parser, &document)) {
        fprintf(stderr, "YAML parser error: %s\n", parser.problem);
        yaml_parser_delete(&parser);
        fclose(fh);
        return -1;
    }
    yaml_node_t* root = yaml_document_get_node(&document, 1); // root node ID is 1
    if (!root || root->type != YAML_MAPPING_NODE) {
        fprintf(stderr, "Root YAML node is not a mapping\n");
        yaml_document_delete(&document);
        yaml_parser_delete(&parser);
        fclose(fh);
        return -1;
    }
    // Traverse top-level mapping (order-independent)
    for (yaml_node_pair_t* p = root->data.mapping.pairs.start; p < root->data.mapping.pairs.top; p++) {
        yaml_node_t* k = yaml_document_get_node(&document, p->key);
        yaml_node_t* v = yaml_document_get_node(&document, p->value);
        char* key = dup_scalar(k);
        if (key) {
            if (strcmp(key, "name") == 0) {
                project->name = dup_scalar(v);
            } else if (strcmp(key, "tempo") == 0) {
                project->tempo = scalar_to_uint(v);
            } else if (strcmp(key, "sample_rate") == 0) {
                project->sample_rate = scalar_to_uint(v);
            } else if (strcmp(key, "ppqn") == 0) {
                project->ppqn = scalar_to_uint(v);
            } else if (strcmp(key, "beat_per_bar") == 0) {
                project->beat_per_bar = scalar_to_uint(v);
            } else if (strcmp(key, "track-config") == 0) {
                parse_track_configs(&document, v, project);
            } else if (strcmp(key, "sequence") == 0) {
                parse_sequences(&document, v, project);
            }
            free(key);
        }
    }
    link_sequence_tracks(project);
    // If any track link failed, we could return error here, but per spec we log and continue
    yaml_document_delete(&document);
    yaml_parser_delete(&parser);
    fclose(fh);
    return 0;
}

void MM_Proj_File_free(MM_File_Project* project) {
    if (!project) return;
    free(project->name);
    for (size_t i = 0; i < project->num_track_configs; i++) {
        free(project->track_configs[i].name);
    }
    free(project->track_configs);
    for (size_t i = 0; i < project->num_sequences; i++) {
        MM_File_Sequence* s = &project->sequences[i];
        free(s->name);
        free(s->length);
        for (size_t j = 0; j < s->num_tracks; j++) {
            MM_File_Sequence_Track* st = &s->tracks[j];
            free(st->track_name);
            for (size_t k = 0; k < st->num_assignments; k++) {
                MM_File_Sequence_Track_Midi_Assignment* a = &st->assignments[k];
                free(a->midi_path);
                free(a->start);
                free(a->length);
            }
            free(st->assignments);
        }
        free(s->tracks);
    }
    free(project->sequences);
    memset(project, 0, sizeof(*project));
}

// Emitter helpers (write)
static int emit_scalar(yaml_emitter_t* emitter, const char* value) {
    yaml_event_t event;
    if (!yaml_scalar_event_initialize(&event, NULL, NULL,
                                      (yaml_char_t*)value, strlen(value),
                                      1, 1, YAML_PLAIN_SCALAR_STYLE)) return 0;
    return yaml_emitter_emit(emitter, &event);
}

static int emit_key_value(yaml_emitter_t* emitter, const char* key, const char* value) {
    if (!emit_scalar(emitter, key)) return 0;
    if (!emit_scalar(emitter, value)) return 0;
    return 1;
}

static int emit_uint(yaml_emitter_t* emitter, const char* key, unsigned int val) {
    char buf[32];
    snprintf(buf, sizeof(buf), "%u", val);
    return emit_key_value(emitter, key, buf);
}

static int emit_double(yaml_emitter_t* emitter, const char* key, double val) {
    char buf[32];
    snprintf(buf, sizeof(buf), "%.2f", val);
    return emit_key_value(emitter, key, buf);
}

static int emit_bool(yaml_emitter_t* emitter, const char* key, bool val) {
    return emit_key_value(emitter, key, val ? "true" : "false");
}

int MM_Proj_File_write(const MM_File_Project* project, const char* filepath) {
    FILE* fh = fopen(filepath, "wb");
    if (!fh) {
        fprintf(stderr, "Cannot open output file: %s\n", filepath);
        return -1;
    }
    yaml_emitter_t emitter;
    if (!yaml_emitter_initialize(&emitter)) {
        fclose(fh);
        return -1;
    }
    yaml_emitter_set_output_file(&emitter, fh);
    yaml_event_t event;

    // Stream / document start
    yaml_stream_start_event_initialize(&event, YAML_UTF8_ENCODING);
    yaml_emitter_emit(&emitter, &event);
    yaml_document_start_event_initialize(&event, NULL, NULL, NULL, 0);
    yaml_emitter_emit(&emitter, &event);

    // Top-level mapping (block style)
    yaml_mapping_start_event_initialize(&event, NULL, (yaml_char_t*)YAML_MAP_TAG, 1, YAML_BLOCK_MAPPING_STYLE);
    yaml_emitter_emit(&emitter, &event);

    // Simple top-level scalars
    if (project->name) emit_key_value(&emitter, "name", project->name);
    emit_uint(&emitter, "tempo", project->tempo);
    emit_uint(&emitter, "sample_rate", project->sample_rate);
    emit_uint(&emitter, "ppqn", project->ppqn);
    emit_uint(&emitter, "beat_per_bar", project->beat_per_bar);

    // track-config map
    emit_scalar(&emitter, "track-config");
    yaml_mapping_start_event_initialize(&event, NULL, NULL, 1, YAML_BLOCK_MAPPING_STYLE);
    yaml_emitter_emit(&emitter, &event);
    for (size_t i = 0; i < project->num_track_configs; i++) {
        MM_File_TrackConfig* tc = &project->track_configs[i];
        emit_scalar(&emitter, tc->name);
        yaml_mapping_start_event_initialize(&event, NULL, NULL, 1, YAML_BLOCK_MAPPING_STYLE);
        yaml_emitter_emit(&emitter, &event);
        emit_key_value(&emitter, "wave", mm_wave_to_str(tc->wave));
        emit_double(&emitter, "gain", tc->gain);
        yaml_mapping_end_event_initialize(&event);
        yaml_emitter_emit(&emitter, &event);
    }
    yaml_mapping_end_event_initialize(&event);
    yaml_emitter_emit(&emitter, &event);

    // sequence list
    emit_scalar(&emitter, "sequence");
    yaml_sequence_start_event_initialize(&event, NULL, NULL, 1, YAML_BLOCK_SEQUENCE_STYLE);
    yaml_emitter_emit(&emitter, &event);
    for (size_t i = 0; i < project->num_sequences; i++) {
        MM_File_Sequence* seq = &project->sequences[i];
        yaml_mapping_start_event_initialize(&event, NULL, NULL, 1, YAML_BLOCK_MAPPING_STYLE);
        yaml_emitter_emit(&emitter, &event);

        emit_key_value(&emitter, "name", seq->name ? seq->name : "");
        emit_key_value(&emitter, "length", seq->length ? seq->length : "");
        if (seq->loop_count != 1) {
            char lbuf[16];
            snprintf(lbuf, sizeof(lbuf), "%d", seq->loop_count);
            emit_key_value(&emitter, "loop", lbuf);
        }

        emit_scalar(&emitter, "tracks");
        yaml_mapping_start_event_initialize(&event, NULL, NULL, 1, YAML_BLOCK_MAPPING_STYLE);
        yaml_emitter_emit(&emitter, &event);

        for (size_t j = 0; j < seq->num_tracks; j++) {
            MM_File_Sequence_Track* st = &seq->tracks[j];
            const char* tname = st->config ? st->config->name : "unknown";
            emit_scalar(&emitter, tname);
            yaml_sequence_start_event_initialize(&event, NULL, NULL, 1, YAML_BLOCK_SEQUENCE_STYLE);
            yaml_emitter_emit(&emitter, &event);
            for (size_t k = 0; k < st->num_assignments; k++) {
                MM_File_Sequence_Track_Midi_Assignment* ass = &st->assignments[k];
                yaml_mapping_start_event_initialize(&event, NULL, NULL, 1, YAML_BLOCK_MAPPING_STYLE);
                yaml_emitter_emit(&emitter, &event);
                emit_key_value(&emitter, "midi", ass->midi_path ? ass->midi_path : "");
                if (ass->start && strcmp(ass->start, "0") != 0) {
                    emit_key_value(&emitter, "start", ass->start);
                }
                emit_bool(&emitter, "loop", ass->loop);
                if (ass->length) emit_key_value(&emitter, "length", ass->length);
                yaml_mapping_end_event_initialize(&event);
                yaml_emitter_emit(&emitter, &event);
            }
            yaml_sequence_end_event_initialize(&event);
            yaml_emitter_emit(&emitter, &event);
        }
        yaml_mapping_end_event_initialize(&event);
        yaml_emitter_emit(&emitter, &event);

        yaml_mapping_end_event_initialize(&event);
        yaml_emitter_emit(&emitter, &event);
    }
    yaml_sequence_end_event_initialize(&event);
    yaml_emitter_emit(&emitter, &event);

    yaml_mapping_end_event_initialize(&event);
    yaml_emitter_emit(&emitter, &event);

    // Document / stream end
    yaml_document_end_event_initialize(&event, 1);
    yaml_emitter_emit(&emitter, &event);
    yaml_stream_end_event_initialize(&event);
    yaml_emitter_emit(&emitter, &event);

    yaml_emitter_delete(&emitter);
    fclose(fh);
    return 0;
}
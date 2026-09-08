#include "minimidi.h"

#include "minimidi-log.h"

#include <errno.h>
#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    MM_MidiEvent *items;
    size_t count;
    size_t capacity;
} EventVector;

static void midi_error(const char *path, const char *format, ...)
{
    char message[512];
    va_list arguments;
    va_start(arguments, format);
    vsnprintf(message, sizeof(message), format, arguments);
    va_end(arguments);
    fprintf(stderr, "MIDI '%s': %s\n", path, message);
    log_error("MIDI '%s': %s", path, message);
}

static uint16_t read_be16(const uint8_t *p)
{
    return (uint16_t)(((uint16_t)p[0] << 8) | p[1]);
}

static uint32_t read_be32(const uint8_t *p)
{
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16)
         | ((uint32_t)p[2] << 8) | p[3];
}

static int read_vlq(const uint8_t *data, size_t size, size_t *position,
                    uint32_t *value)
{
    uint32_t result = 0;
    unsigned int bytes = 0;

    do {
        uint8_t byte;
        if (*position >= size || bytes++ == 4) return -1;
        byte = data[(*position)++];
        if (result > (UINT32_MAX >> 7)) return -1;
        result = (result << 7) | (uint32_t)(byte & 0x7f);
        if ((byte & 0x80) == 0) {
            *value = result;
            return 0;
        }
    } while (true);
}

static int append_event(EventVector *events, const MM_MidiEvent *event)
{
    MM_MidiEvent *grown;
    size_t capacity;

    if (events->count == events->capacity) {
        capacity = events->capacity == 0 ? 64 : events->capacity * 2;
        if (capacity > SIZE_MAX / sizeof(*grown)) return -1;
        grown = realloc(events->items, capacity * sizeof(*grown));
        if (!grown) return -1;
        events->items = grown;
        events->capacity = capacity;
    }
    events->items[events->count++] = *event;
    return 0;
}

static MidiNote midi_note(uint8_t number)
{
    MidiNote result;
    result.note = (Note)(number % 12);
    result.octave = (int8_t)((int)number / 12 - 1);
    return result;
}

static int parse_track(const uint8_t *data, size_t size, EventVector *events,
                       uint64_t *track_end)
{
    size_t position = 0;
    uint64_t absolute_tick = 0;
    uint8_t running_status = 0;

    while (position < size) {
        uint32_t delta;
        uint8_t status;
        uint8_t data1;
        uint8_t data2 = 0;
        unsigned int data_count;
        MM_MidiEvent event;

        if (read_vlq(data, size, &position, &delta) != 0
            || UINT64_MAX - absolute_tick < delta) return -1;
        absolute_tick += delta;
        if (position >= size) return -1;

        status = data[position];
        if (status & 0x80) {
            position++;
            if (status < 0xf0) running_status = status;
        } else {
            if (running_status == 0) return -1;
            status = running_status;
        }

        if (status == 0xff) {
            uint32_t length;
            running_status = 0;
            if (position >= size) return -1;
            position++;
            if (read_vlq(data, size, &position, &length) != 0
                || length > size - position) return -1;
            position += length;
            continue;
        }
        if (status == 0xf0 || status == 0xf7) {
            uint32_t length;
            running_status = 0;
            if (read_vlq(data, size, &position, &length) != 0
                || length > size - position) return -1;
            position += length;
            continue;
        }
        if (status >= 0xf0) return -1;

        data_count = ((status & 0xf0) == 0xc0 || (status & 0xf0) == 0xd0) ? 1 : 2;
        if (data_count > size - position) return -1;
        data1 = data[position++];
        if (data_count == 2) data2 = data[position++];
        if ((data1 & 0x80) || (data2 & 0x80)) return -1;
        if ((status & 0xf0) != MIDI_NOTE_ON
            && (status & 0xf0) != MIDI_NOTE_OFF) continue;

        memset(&event, 0, sizeof(event));
        event.delta_ticks = delta;
        event.abs_ticks = absolute_tick;
        event.status_code = (MidiStatusCode)(status & 0xf0);
        if (event.status_code == MIDI_NOTE_ON && data2 == 0)
            event.status_code = MIDI_NOTE_OFF;
        event.channel = status & 0x0f;
        event.note_number = data1;
        event.evt_data[0] = data1;
        event.evt_data[1] = data2;
        event.note = midi_note(data1);
        if (append_event(events, &event) != 0) return -1;
    }

    *track_end = absolute_tick;
    return 0;
}

static int compare_events(const void *left, const void *right)
{
    const MM_MidiEvent *a = left;
    const MM_MidiEvent *b = right;
    if (a->abs_ticks < b->abs_ticks) return -1;
    if (a->abs_ticks > b->abs_ticks) return 1;
    if (a->status_code == MIDI_NOTE_OFF && b->status_code == MIDI_NOTE_ON) return -1;
    if (a->status_code == MIDI_NOTE_ON && b->status_code == MIDI_NOTE_OFF) return 1;
    return 0;
}

static void link_note_pairs(MM_MidiEvent *events, size_t count)
{
    MM_MidiEvent *active[16][128] = {{NULL}};
    size_t i;

    for (i = 0; i < count; i++) {
        MM_MidiEvent *event = &events[i];
        event->next = NULL;
        event->prev = NULL;
        if (event->status_code == MIDI_NOTE_ON) {
            active[event->channel][event->note_number] = event;
        } else if (event->status_code == MIDI_NOTE_OFF) {
            MM_MidiEvent *on = active[event->channel][event->note_number];
            if (on) {
                on->next = event;
                event->prev = on;
                active[event->channel][event->note_number] = NULL;
            }
        }
    }
}

static void list_clear(MM_MidiEvent_LList *list)
{
    MM_MidiEvent_LList_Node *node = list->first;
    while (node) {
        MM_MidiEvent_LList_Node *next = node->next;
        free(node);
        node = next;
    }
    list->first = NULL;
    list->last = NULL;
    list->length = 0;
}

static int list_append(MM_MidiEvent_LList *list, MM_MidiEvent *event)
{
    MM_MidiEvent_LList_Node *node = malloc(sizeof(*node));
    if (!node) return -1;
    node->value = event;
    node->next = NULL;
    if (list->last) list->last->next = node;
    else list->first = node;
    list->last = node;
    list->length++;
    return 0;
}

int MM_File_init(MM_Midi_File *file, const char *file_path)
{
    FILE *stream = NULL;
    uint8_t *contents = NULL;
    long file_length;
    size_t position;
    size_t track_index;
    EventVector events = {0};
    uint64_t total_ticks = 0;
    int result = -1;

    if (!file || !file_path) return -1;
    memset(file, 0, sizeof(*file));
    MM_MidiEvent_LList_init(&file->events);
    stream = fopen(file_path, "rb");
    if (!stream) {
        midi_error(file_path, "cannot open file: %s", strerror(errno));
        goto done;
    }
    if (fseek(stream, 0, SEEK_END) != 0 || (file_length = ftell(stream)) < 0
        || fseek(stream, 0, SEEK_SET) != 0) {
        midi_error(file_path, "cannot measure file");
        goto done;
    }
    if ((size_t)file_length < 14) {
        midi_error(file_path, "file is truncated");
        goto done;
    }
    contents = malloc((size_t)file_length);
    if (!contents || fread(contents, 1, (size_t)file_length, stream) != (size_t)file_length) {
        midi_error(file_path, "cannot read file");
        goto done;
    }
    if (memcmp(contents, "MThd", 4) != 0 || read_be32(contents + 4) < 6) {
        midi_error(file_path, "missing or invalid MThd header");
        goto done;
    }

    file->header.length = read_be32(contents + 4);
    file->header.format = read_be16(contents + 8);
    file->header.ntrks = read_be16(contents + 10);
    file->header.ppqn = read_be16(contents + 12);
    if (file->header.format > 1 || file->header.ntrks == 0
        || file->header.ppqn == 0 || (file->header.ppqn & 0x8000)) {
        midi_error(file_path, "unsupported format or SMPTE division");
        goto done;
    }
    position = 8 + file->header.length;
    if (position > (size_t)file_length) goto done;

    for (track_index = 0; track_index < file->header.ntrks; track_index++) {
        uint32_t chunk_length;
        uint64_t track_ticks = 0;
        if (position > (size_t)file_length || (size_t)file_length - position < 8
            || memcmp(contents + position, "MTrk", 4) != 0) {
            midi_error(file_path, "missing or truncated track %zu", track_index + 1);
            goto done;
        }
        chunk_length = read_be32(contents + position + 4);
        position += 8;
        if (chunk_length > (size_t)file_length - position
            || parse_track(contents + position, chunk_length, &events, &track_ticks) != 0) {
            midi_error(file_path, "malformed track %zu", track_index + 1);
            goto done;
        }
        if (track_ticks > total_ticks) total_ticks = track_ticks;
        position += chunk_length;
    }

    if (events.count > 1)
        qsort(events.items, events.count, sizeof(*events.items), compare_events);
    for (track_index = 0; track_index < events.count; track_index++) {
        events.items[track_index].delta_ticks = track_index == 0
            ? events.items[track_index].abs_ticks
            : events.items[track_index].abs_ticks - events.items[track_index - 1].abs_ticks;
    }
    link_note_pairs(events.items, events.count);
    file->filepath = strdup(file_path);
    if (!file->filepath) goto done;
    file->length = (size_t)file_length;
    file->track.length = events.count;
    file->track.n_events = events.count;
    file->track.event_arr = events.items;
    file->track.total_ticks = total_ticks;
    file->track.total_beats = (double)total_ticks / file->header.ppqn;
    events.items = NULL;
    if (MM_MidiEvent_LList_from_array(&file->events, file->track.event_arr,
                                      file->track.n_events) != 0) goto done;
    result = 0;

done:
    if (stream) fclose(stream);
    free(contents);
    free(events.items);
    if (result != 0) MM_File_free(file);
    return result;
}

void MM_File_free(MM_Midi_File *file)
{
    if (!file) return;
    list_clear(&file->events);
    free(file->track.event_arr);
    free(file->filepath);
    memset(file, 0, sizeof(*file));
}

int MM_MidiEvent_LList_init(MM_MidiEvent_LList *list)
{
    if (!list) return -1;
    memset(list, 0, sizeof(*list));
    return 0;
}

int MM_MidiEvent_LList_destroy(MM_MidiEvent_LList *list)
{
    if (!list) return -1;
    list_clear(list);
    return 0;
}

int MM_MidiEvent_LList_from_array(MM_MidiEvent_LList *list,
                                  MM_MidiEvent *array, size_t count)
{
    size_t i;
    if (!list || (!array && count != 0)) return -1;
    list_clear(list);
    for (i = 0; i < count; i++) {
        if (list_append(list, &array[i]) != 0) {
            list_clear(list);
            return -1;
        }
    }
    return 0;
}

int MM_File_get_events_in_range(MM_Midi_File *file,
                                MM_MidiEvent_LList *list,
                                uint64_t start_ticks,
                                uint64_t end_ticks,
                                int start_note,
                                int end_note)
{
    size_t i;
    if (!file || !list || start_ticks > end_ticks || start_note > end_note) return -1;
    list_clear(list);
    for (i = 0; i < file->track.n_events; i++) {
        MM_MidiEvent *event = &file->track.event_arr[i];
        if (event->abs_ticks >= start_ticks && event->abs_ticks <= end_ticks
            && event->note_number >= start_note && event->note_number <= end_note
            && list_append(list, event) != 0) {
            list_clear(list);
            return -1;
        }
    }
    return 0;
}

double MM_Util_tick_to_s(uint64_t ticks, unsigned int bpm, unsigned int ppqn)
{
    if (bpm == 0 || ppqn == 0) return 0.0;
    return 60.0 * (double)ticks / ((double)ppqn * bpm);
}

uint64_t MM_Util_s_to_tick(double seconds, unsigned int bpm, unsigned int ppqn)
{
    if (seconds <= 0.0 || bpm == 0 || ppqn == 0) return 0;
    return (uint64_t)floor(seconds * bpm * ppqn / 60.0);
}

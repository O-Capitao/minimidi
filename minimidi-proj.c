#include "minimidi-proj.h"

void midi_map_put( MM_Mid_Map_Entry *map, const char *key, MM_Midi_File value) {
    MM_Mid_Map_Entry *entry;

    HASH_FIND_STR( map, key, entry );

    if (entry == NULL) {
        entry = malloc(sizeof *entry);
        entry->key = strdup(key);  // copy the key!
        HASH_ADD_KEYPTR(hh, map, entry->key, strlen(entry->key), entry);
    }

    entry->value = value;
}

MM_Midi_File* midi_map_get( MM_Mid_Map_Entry *map, const char *key) {
    MM_Mid_Map_Entry *entry;

    HASH_FIND_STR( map, key, entry);

    if (entry == NULL) {
        return NULL;
    }

    return &entry->value;
}


void midi_map_free_all( MM_Mid_Map_Entry *map ) {
    MM_Mid_Map_Entry *current, *tmp;

    HASH_ITER(hh, map, current, tmp)
    {
        HASH_DEL(map, current);
        free(current->key);
        free(current);
    }
}

unsigned int parse_bars_and_beats(const char *s) {
    unsigned int bars = 0, beats = 0;

    char *end;
    unsigned long first = strtoul(s, &end, 10);

    if (end != s) {
        if (*end == 'B') {
            bars = (unsigned int)first;
            end++;
            /* optional beat component */
            char *end2;
            unsigned long second = strtoul(end, &end2, 10);
            if (end2 != end && *end2 == 'b')
                beats = (unsigned int)second;
        } else if (*end == 'b') {
            beats = (unsigned int)first;
        }
    }

    return bars * 4 + beats;
}

int MM_Project_init( MM_Project *p, const char* filepath )
{
    log_debug( BLUE "MM_Project_init()" RESET "entering.");

    // init all to zeros
    memset(p, 0, sizeof(*p));

    p->sequence_arr = malloc(p->n_sequences * sizeof(MM_Sequence));
    p->file = calloc(1, sizeof(MM_File_Project));

    if (p->file == NULL)
    {
        log_fatal("Out of memory allocating MM_File_Project");
        return -1;
    }

    if (MM_Proj_File_read(filepath, p->file) > 0)
    {
        log_fatal("Error reading proj file");
        return -1;
    }

    MM_File_Sequence_Track *_fst;
    MM_File_Sequence_Track_Midi_Assignment *_sqma;

    p->midi_map = NULL;
    // init and load MIDI files
    for (int i = 0; i < p->file->num_sequences; i++)
    {
        log_debug( BLUE "MM_Project_init():" RESET "Entering sequence %i - %s.", i, p->file->sequences[i].name );

        p->sequence_arr[i].length_beats = parse_bars_and_beats(p->file->sequences[i].length);
        p->sequence_arr[i].loop_count = p->file->sequences[i].loop_count;

        log_debug( GREEN "Sequence length: " RESET "%s = %i beats",p->file->sequences[i].length, p->sequence_arr[i].length_beats );
        
        for (int j = 0; j < p->file->sequences[i].num_tracks; j++)
        {
            log_debug("     Looking at Midi files for track %i - %s", j, p->file->sequences[i].tracks[j].track_name );
            _fst = &(p->file->sequences[i].tracks[j]);

            for (int k = 0; k < _fst->num_assignments; k++ )
            {
                _sqma = &(_fst->assignments[k]);
                log_debug("         Looking at %s", _sqma->midi_path );

                if (midi_map_get( p->midi_map, _sqma->midi_path ) != NULL )
                {
                    log_debug("             Already in. Skipping...." );
                } else {
                    MM_Midi_File _midi;
                    if (MM_File_init(&_midi, _sqma->midi_path) != 0)
                    {
                        log_error(RED "MM_Project_init():" RESET " could not init %s", _sqma->midi_path );
                    } else
                    {
                        midi_map_put( p->midi_map, _sqma->midi_path, _midi);
                        log_debug("             inserted %s", _sqma->midi_path );
                    }
                }
            }
        }
    }

    return 0;
}

void MM_Project_free(MM_Project *p)
{
    if (!p) return;
    if (p->file) {
        MM_Proj_File_free(p->file);
        free(p->file);
        p->file = NULL;

        midi_map_free_all(p->midi_map);
    }
    
}
#include <string.h>
#include <assert.h>
#include <stdio.h>
#include <unistd.h>
#include <math.h>

#include "minimidi.h"




/****************************************************************************************
*
*
*   -> Utility  Functions
****************************************************************************************/
void print_byte_as_binary(_Byte *byte, int little_endian)
{
    if (byte == NULL) {
        printf("Null pointer received.\n");
        return;
    }

    if (little_endian) {
        // Print from LSB to MSB (Little Endian Representation)
        for (int i = 0; i < 8; i++) {
            printf("%c", (*byte & (1 << i)) ? '1' : '0');
        }
    } else {
        // Print from MSB to LSB (Big Endian Representation)
        for (int i = 7; i >= 0; i--) {
            printf("%c", (*byte & (1 << i)) ? '1' : '0');
        }
    }
}





// Function to get MIDI Status Code from a byte
MidiStatusCode _get_midi_status_code( _Byte *byte)
{
    // Extract the status nibble (upper 4 bits)
    _Byte status = *byte & 0xF0;
    
    // Check if it's a valid MIDI status byte (first bit must be 1)
    if (!(*byte & 0x80)) {
        return MIDI_INVALID;
    }

    switch (status) {
        case 0x80: // 0b10000000
            return MIDI_NOTE_OFF;
        case 0x90: // 0b10010000
            return MIDI_NOTE_ON;
        case 0xA0:
            return MIDI_POLY_AFTERTOUCH;
        case 0xB0:
            return MIDI_CONTROL_CHANGE;
        case 0xC0:
            return MIDI_PROGRAM_CHANGE;
        case 0xD0:
            return MIDI_CHAN_AFTERTOUCH;
        case 0xE0:
            return MIDI_PITCH_BEND;
        case 0xF0:
            return MIDI_SYSTEM;
        default:
            return MIDI_INVALID;
    }
}


void _print_midi_status_code(MidiStatusCode status)
{
    printf(CYAN "MIDI Status: " RESET);
    
    switch (status) {
        case MIDI_NOTE_OFF:
            printf("Note Off (0x80)\n");
            break;
        case MIDI_NOTE_ON:
            printf("Note On (0x90)\n");
            break;
        case MIDI_POLY_AFTERTOUCH:
            printf("Polyphonic Aftertouch (0xA0)\n");
            break;
        case MIDI_CONTROL_CHANGE:
            printf("Control Change (0xB0)\n");
            break;
        case MIDI_PROGRAM_CHANGE:
            printf("Program Change (0xC0)\n");
            break;
        case MIDI_CHAN_AFTERTOUCH:
            printf("Channel Aftertouch (0xD0)\n");
            break;
        case MIDI_PITCH_BEND:
            printf("Pitch Bend (0xE0)\n");
            break;
        case MIDI_SYSTEM:
            printf("System Message (0xF0)\n");
            break;
        case MIDI_INVALID:
            printf("Invalid Status (0x00)\n");
            break;
        default:
            printf("Unknown Status Code\n");
            break;
    }
}




uint8_t _get_midi_data_byte_count(MidiStatusCode status)
{

    switch (status) {
        case MIDI_NOTE_OFF:        // 0x80
        case MIDI_NOTE_ON:         // 0x90
        case MIDI_POLY_AFTERTOUCH: // 0xA0
        case MIDI_CONTROL_CHANGE:  // 0xB0
        case MIDI_PITCH_BEND:      // 0xE0
            return 2;              // These events all take 2 data bytes

        case MIDI_PROGRAM_CHANGE:  // 0xC0
        case MIDI_CHAN_AFTERTOUCH: // 0xD0
            return 1;              // These take 1 data byte

        case MIDI_SYSTEM:          // 0xF0
            // System messages like SysEx have variable length,
            // but for basic handling, we’ll assume it’s incomplete here
            return 0;              // Could expand this for SysEx later

        case MIDI_INVALID:         // 0x00
        default:
            return 0;              // Invalid or unrecognized status
    }
}




MidiNote _event_data_bytes_to_note( _Byte event_data_byte )
{

    MidiNote result;
    
    // Ensure note_number is in valid MIDI range (0-127)
    if (event_data_byte > 127) {
        event_data_byte = 127;  // Clamp to max MIDI value
    }
    
    // Extract chromatic note (0-11) using modulo
    result.note = (Note)(event_data_byte % 12);
    
    // Calculate octave
    // MIDI note 0 = C-1, 12 = C0, 24 = C1, ..., 60 = C4, etc.
    // So we divide by 12 and subtract 1 to get standard octave numbers
    result.octave = (event_data_byte / 12) - 1;
    
    return result;
}




void _print_midi_note(MidiNote note)
{

    switch (note.note) {
        case C:
            printf("C");
            break;
        case Cs:
            printf("C#");
            break;
        case D:
            printf("D");
            break;
        case Ds:
            printf("D#");
            break;
        case E:
            printf("E");
            break;
        case F:
            printf("F");
            break;
        case Fs:
            printf("F#");
            break;
        case G:
            printf("G");
            break;
        case Gs:
            printf("G#");
            break;
        case A:
            printf("A");
            break;
        case As:
            printf("A#");
            break;
        case B:
            printf("B");
            break;
        default:
            printf("Invalid note\n");
            break;
    }

    printf(" Oct: %hu ", note.octave );
}


bool _compare_MidiNote(MidiNote *a, MidiNote *b)
{
    return (a->note == b->note) && (a->octave == b->octave);
}

int _midi_note_to_int( MidiNote *n )
{
    return (int)(n->note) + (n->octave) * 12;
}

void _reverse_byte_array(_Byte* arr, size_t len)
{
    _Byte aux;
    for (size_t i = 0; i < len / 2; i++)
    {
        aux = arr[i];
        arr[i] = arr[len-1-i];
        arr[len-1-i] = aux;
    }
}




void _extract_number_from_byte_array( void* tgt, _Byte* src, size_t start_ind, size_t len )
{
    _Byte extracted_bytes[len];
    memcpy( extracted_bytes, &src[start_ind], len );

    _reverse_byte_array( extracted_bytes, len );
    memcpy(tgt, extracted_bytes, len);
}




void _get_substring(_Byte *src_str, _Byte* tgt_str, size_t start_index, size_t n_elements_to_copy, bool add_null_termination )
{
    memcpy( tgt_str, &src_str[start_index], n_elements_to_copy );

    if (add_null_termination)
    {
        tgt_str[ n_elements_to_copy ] = '\0';
    }
}




size_t _read_VLQ_delta_t( _Byte *bytes, size_t len, uint64_t *val_ptr)
{
    size_t _index = 0;
    _Byte _curr_byte;



    const _Byte _sign_bit_mask =    0x80; // 0b10000000
    const _Byte _7_last_bits_mask = 0x7F; // 0b01111111

    unsigned long retval = 0; // 4 bytes maximum...

    while ( _index < len )
    {
        // grab a byte
        _curr_byte = bytes[_index++];



        retval<<=7;
        retval += ( _curr_byte & _7_last_bits_mask );

        if ( ( _curr_byte & _sign_bit_mask ) == 0 )
        {
            break;
        }
    }

    *val_ptr = retval;



    return _index;
}




/****************************************************************************************
*
*
*   -> Main Struct Methods
****************************************************************************************/
void _parse_track_events( MM_Track *track, _Byte *evts_chunk )
{
    track->total_ticks = 0;
    
    size_t _byte_counter = 0;
    size_t _event_counter = 0;


    // Last status byte for running status handling.
    _Byte *_last_status_byte = NULL;
    // _Byte *_next_byte = NULL;
    uint8_t _data_bytes_count = 0;

    while ( _byte_counter < track->length )
    {

        struct MM_Event evt;

        evt.next = NULL;
        evt.prev = NULL;

        _byte_counter += _read_VLQ_delta_t( evts_chunk + _byte_counter, track->length - _byte_counter, &(evt.delta_ticks));
        track->total_ticks += evt.delta_ticks;
        evt.abs_ticks = track->total_ticks;


        if ( *(evts_chunk + _byte_counter) >= 0x80 )
        {
            // This is a new status byte (has the high bit set)
            evt.status_code = _get_midi_status_code((evts_chunk + _byte_counter));
            _last_status_byte = (evts_chunk + _byte_counter);  // Remember for running status
            _byte_counter++;

        } else {
            // No new status byte — use running status


            evt.status_code = _get_midi_status_code(_last_status_byte);
            // Note: byte_counter not incremented here, since there's no status byte.
        }

        if (evt.status_code == MIDI_INVALID) {
            printf(RED "Invalid status byte detected — aborting!\n" RESET);
            return;
        }

         _data_bytes_count = _get_midi_data_byte_count( evt.status_code );

         // extract databytes
        // when do we care?
        //  when evt is a Note On, and never elses
        if (evt.status_code == MIDI_NOTE_ON || evt.status_code == MIDI_NOTE_OFF)
        {
            evt.note = _event_data_bytes_to_note(*(evts_chunk + _byte_counter));

        }
        _byte_counter += _data_bytes_count;

        track->event_arr[_event_counter ++] = evt;

    }
    track->n_events = _event_counter;
    // track->total_beats = track->total_ticks / ppqn;
}




MM_File* create_mini_midi_file(const char *filepath) {
    MM_File *midi_file = malloc(sizeof(MM_File));
    if (!midi_file) return NULL;

    midi_file->filepath = strdup(filepath);
    if (!midi_file->filepath) {
        free(midi_file);
        return NULL;
    }

    // Allocate and zero-initialize the header
    midi_file->header = calloc(1, sizeof(MM_Header));
    if (!midi_file->header) {
        free(midi_file->filepath);
        free(midi_file);
        return NULL;
    }

    // Set format = 0 and ntrks = 1 by convention for single-track
    midi_file->header->format = 0;
    midi_file->header->ntrks = 1;

    // Allocate one track
    midi_file->track = calloc(1, sizeof(MM_Track));
    if (!midi_file->track) {
        free(midi_file->header);
        free(midi_file->filepath);
        free(midi_file);
        return NULL;
    }

    midi_file->length = 0; // can set this when writing or parsing

    return midi_file;
}

int hook_up_events( MM_Event *arr, size_t n )
{
    log_debug("minimidi.c > hook_up_events() : Entering");



    int hook_counter = 0;
    MM_Event *cursor, *cursor2;

    for (int i = 0; i < n; i++)
    {

        cursor = &(arr[i]);
        
        // snprintf(log_line, 100, "hook_up_events:: connecting %li", i);

        if (cursor->status_code == MIDI_NOTE_ON)
        {            
            // odds are that a NOTE OFF exists for this note
            for (int j = i; j < n; j++)
            {
                cursor2 = &(arr[j]);
                if ( cursor2->status_code == MIDI_NOTE_OFF && _compare_MidiNote( &(cursor->note), &(cursor2->note) ))
                {
                    log_debug("minimidi.c > hook_up_events() > hooking up %i to %i ", i, j);
                    hook_counter++;
                    cursor->next = cursor2;
                    cursor2->prev = cursor;

                    break;
                }
            }
        }
    }

    return hook_counter;
}

// "Class" Methods
MM_Header *_midi_header_read( _Byte *file_contents )
{

    MM_Header *retval = (MM_Header*)malloc( sizeof( struct MM_Header ) );
    if (!retval) return NULL; 
    
    uint32_t aux_for_chunk_size;
    _extract_number_from_byte_array( &aux_for_chunk_size, file_contents, 4, 4 );
    retval->length = (size_t)aux_for_chunk_size;

    _extract_number_from_byte_array( &(retval->format), file_contents, 8, 2 );
    _extract_number_from_byte_array( &(retval->ntrks), file_contents, 10, 2 );
    _extract_number_from_byte_array( &(retval->ppqn), file_contents, 12, 2 );

    return retval;
}




MM_Track *MM_Track_read( _Byte *file_content, size_t start_index, size_t total_chunk_len )
{
    MM_Track *track = (MM_Track*)malloc( sizeof( struct MM_Track ) );
    _Byte _track_bin_data[ total_chunk_len ];

    if (!track) return NULL; 

#if DEBUG
    printf(GREEN "Reading Track Chunk" RESET ": Starting at %lu / %lu Bytes.\n", start_index, total_chunk_len);
#endif

    track->length = total_chunk_len;

    // ESTIMATE: each event is minimum 3 bytes.
    size_t max_events = track->length / 3;
    track->event_arr = (MM_Event*)malloc( max_events * sizeof( MM_Event ) );

    _extract_number_from_byte_array( &(track->length), file_content, start_index + 4, 4 );
    _get_substring(file_content, _track_bin_data, start_index + 8, track->length, false );

    //      total                                    + Chunk Id + Chunk len
    assert( total_chunk_len == ( start_index + track->length + 4        + 4 ));
    _parse_track_events( track, _track_bin_data );
    hook_up_events( track->event_arr, track->n_events );

    return track;
}




void MM_Header_print( MM_Header *mh )
{
    printf(BOLDWHITE "HEADER:\n---------------------------\n" RESET);
    printf(TAB WHITE "Chunk Size:" RESET BOLDWHITE " %zu" RESET " bytes\n", mh->length );
    printf(TAB WHITE "Format Code:" RESET " %i.\n", mh->format);
    printf(TAB WHITE "Number Of Tracks:" RESET " %i.\n", mh->ntrks);
    printf(TAB WHITE "Division:" RESET " %i PPQN.\n", mh->ppqn);
    printf("---------------------------\n");
}

void MM_Event_print( MM_Event *me )
{
    printf(TAB TAB "Event:\n");
    printf(TAB TAB "Delta: %lu ticks\n", me->delta_ticks );
    printf(TAB TAB "Status Code: ");
    _print_midi_status_code(me->status_code);
    printf("\n");
    printf(TAB TAB "Note: ");
    _print_midi_note(me->note);
    printf("\n");
    printf(TAB TAB "----\n");
}

// FOR LOGGINING
void _midi_note_to_str(MidiNote note, char *str)
{

    switch (note.note) {
        case C:
            sprintf(str, "C%d", note.octave );
            break;
        case Cs:
            sprintf(str, "C#%d", note.octave );
            break;
        case D:
            sprintf(str, "D%d", note.octave );
            break;
        case Ds:
            sprintf(str, "D#%d", note.octave );
            break;
        case E:
            sprintf(str, "E%d", note.octave );
            break;
        case F:
            sprintf(str, "F%d", note.octave );
            break;
        case Fs:
            sprintf(str, "F#%d", note.octave );
            break;
        case G:
            sprintf(str, "G%d", note.octave );
            break;
        case Gs:
            sprintf(str, "G#%d", note.octave );
            break;
        case A:
            sprintf(str, "A%d", note.octave );
            break;
        case As:
            sprintf(str, "A#%d", note.octave );
            break;
        case B:
            sprintf(str, "B%d", note.octave );
            break;
        default:
            sprintf(str, "XX");
            break;
    }
}

void _midi_status_code_to_str( MidiStatusCode status, char* str )
{
    switch (status)
    {
        case MIDI_NOTE_OFF:
            sprintf( str, "NOTE_OFF" );
            break;
        case MIDI_NOTE_ON:
            sprintf( str, "NOTE_ON" );
            break;
        default:
            sprintf( str, "OTHER");
    }
}






void MM_Track_print( MM_Track *mt )
{
    printf( BOLDWHITE "TRACK:" RESET "\n---------------------------\n");
    printf( TAB WHITE "Chunk Size:" RESET BOLDWHITE" %li" RESET " bytes\n", mt->length );
    printf( TAB WHITE "Number of Events: %li \n" RESET, mt->n_events );

    for (int i = 0; i < mt->n_events; i++ ){
        MM_Event_print( &(mt->event_arr[i]) );
    }

    printf("---------------------------\n");
    
}

void MM_File_free( MM_File *self )
{
    if (!self) return;

    if (self->filepath) free(self->filepath);
    if (self->header) free(self->header);

    if (self->track) {
        // Free event array if it exists
        if (self->track->event_arr) {
            free(self->track->event_arr);
        }
        free(self->track);
    }

    free(self);
}

void MM_File_print( MM_File *file )
{
    MM_Header_print( file->header );
    MM_Track_print( file->track );
}

MM_File * MM_File_init( char *file_path )
{
    MM_File *retval = create_mini_midi_file( file_path );
    
    if (!retval) return NULL; 

    FILE *fileptr;
    fileptr = fopen( file_path, "rb" );
    _Byte * buffer = 0;
    size_t length;

    int freadres = 0;
    
    if (fileptr)
    {
        fseek (fileptr, 0, SEEK_END);
        length = ftell (fileptr);

        fseek (fileptr, 0, SEEK_SET);
        buffer = malloc (length);

        if (buffer)
        {
            // fread returns read bytes.
            freadres = fread (buffer, 1, length, fileptr);
            printf(GREEN "Success" RESET " Read %i bytes.\n", freadres);
        }

        fclose (fileptr);
    }

    retval->length = length;
    retval->header = _midi_header_read( buffer );
    retval->track = MM_Track_read( buffer, 14, length );
    retval->track->total_beats = (retval->track->total_ticks / retval->header->ppqn) + 1;
    
    free( buffer );

    log_info(
        "MM_File : parsed %s : %ld bytes, got %ld events.",
        file_path,
        retval->track->length,
        retval->track->n_events );

    // log header info
    log_info(
        "MM_Header: Chunk Size: %zu, PPQN: %i.",
        retval->header->length,
        retval->header->ppqn );
    
    retval->bpm = 120;

    retval->events = MM_Event_LList_init();
    MM_Event_LList_from_array( retval->events, retval->track->event_arr, retval->track->n_events );

    return retval;
}

unsigned short MM_File_get_bpm( MM_File *f ){
    return f->bpm;
}

MM_Event_LList *MM_Event_LList_init()
{
    MM_Event_LList *self = (MM_Event_LList *)malloc(sizeof( MM_Event_LList ));
    self->length = 0;
    self->first = NULL;
    self->last = NULL;

    return self;
}


// Kenny Loggings
void __dump_list_to_log(MM_File *f, MM_Event_LList *l) {
    char note_str[8];
    char status_str[16];
    int e_counter = 0;
    MM_Event_LList_Node *cursor = l->first;

    while (cursor) {
        _midi_note_to_str(cursor->value->note, note_str);
        _midi_status_code_to_str(cursor->value->status_code, status_str);
        log_trace("EVT [%i]: ticks=%ld, note=%s, status=%s", e_counter, cursor->value->abs_ticks, note_str, status_str);

        cursor = cursor->next;
        e_counter++;
    }

    log_debug("minimidi.c > MM_Event_LList > init : Done initing with %i events.", e_counter);
}


int MM_Event_LList_append(MM_Event_LList*self, MM_Event *v)
{
    MM_Event_LList_Node *node = (MM_Event_LList_Node *)malloc(sizeof( MM_Event_LList_Node ));
    MM_Event_LList_Node *aux = 0;

    node->next = NULL;
    node->value = v;

    // list is empty
    if (!self->first)
    {
        self->last = node;
        self->first = node;
        node->next = NULL;

    } else
    {
        aux = self->last;
        self->last = node;
        aux->next = node;
    }

    self->length++;
    return 0;
}

void _recurse_and_destroy( MM_Event_LList_Node *node)
{
    if (node->next)
    {
        _recurse_and_destroy(node->next);
    }
    free(node);
}

int _emptyList( MM_Event_LList* self )
{
    if (self->first)
        _recurse_and_destroy(self->first);
    
    self->first = NULL;
    self->last = NULL;
    self->length = 0;
    return 0;
}

/**
 * Public again
 */
int MM_Event_LList_destroy(MM_Event_LList*self)
{
    _emptyList( self );
    free(self);
    return 0;
}

int MM_File_get_events_in_range( MM_File *self,  MM_Event_LList *list, int start_ticks, int end_ticks, int start_note, int end_note )
{
    _emptyList(list);
    MM_Event *evt;

    for (int i = 0; i < self->track->n_events; i++ )
    {
        evt = &(self->track->event_arr[i]);

        if ( evt->abs_ticks >= start_ticks && evt->abs_ticks <= end_ticks 
            && _midi_note_to_int( &(evt->note) ) >= start_note && _midi_note_to_int( &(evt->note) ) <= end_note )
        {
            MM_Event_LList_append(list, evt);
        }
    }
    
    return 0;
}

int MM_Event_LList_from_array( MM_Event_LList *list, MM_Event *array, size_t n_events ){
    int err = 0;
    for (int i = 0; i < n_events; i++){
        err = MM_Event_LList_append(list, array + i);
        if (err){
            log_error("MM_Event_LList_from_array. err@ %i", i);
            return err;
        }
    }
    return 0;
}

MM_Event_LList_Node *MM_Event_LList_find_next_node_at_ticks(MM_Event_LList *list, unsigned int ticks) {

    MM_Event_LList_Node *retval = list->first;

    while (retval->next && retval->next->value->abs_ticks < ticks ){
        retval = retval->next;
    }
    log_debug("MM_Event_LList_find_next_node_at_ticks( %i ) -> EVT", ticks);
    return retval;
}


double MM_Util_tick_to_s(unsigned int ticks, unsigned short bpm, unsigned int ppqn) {
    return ( 60 * (double)ticks ) / ((double)ppqn * (double)bpm );
}

unsigned int MM_Util_s_to_tick(double t_s, unsigned short bpm, unsigned int ppqn) {
    return (unsigned int)floor(t_s * (double)bpm * (double)ppqn / 60.0);
}

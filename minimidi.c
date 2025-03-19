#include "stdbool.h"
#include "minimidi.h"
#include "string.h"
#include "assert.h"
#include "stdio.h"

#define DEBUG 0

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
MidiStatusCode get_midi_status_code( _Byte *byte)
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

void print_midi_status_code(MidiStatusCode status)
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

uint8_t get_midi_data_byte_count(MidiStatusCode status)
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

MidiNote event_data_bytes_to_note( _Byte event_data_byte )
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

void print_midi_note(MidiNote note)
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

void reverse_byte_array(_Byte* arr, size_t len)
{
    _Byte aux;
    for (size_t i = 0; i < len / 2; i++)
    {
        aux = arr[i];
        arr[i] = arr[len-1-i];
        arr[len-1-i] = aux;
    }
}

void extract_number_from_byte_array( void* tgt, _Byte* src, size_t start_ind, size_t len )
{
    _Byte extracted_bytes[len];
    memcpy( extracted_bytes, &src[start_ind], len );

    reverse_byte_array( extracted_bytes, len );
    memcpy(tgt, extracted_bytes, len);
}

void get_substring(_Byte *src_str, _Byte* tgt_str, size_t start_index, size_t n_elements_to_copy, bool add_null_termination )
{
    memcpy( tgt_str, &src_str[start_index], n_elements_to_copy );

    if (add_null_termination)
    {
        tgt_str[ n_elements_to_copy ] = '\0';
    }
}

size_t read_VLQ_delta_t( _Byte *bytes, size_t len, uint64_t *val_ptr)
{
    size_t _index = 0;
    _Byte _curr_byte;


#if DEBUG
/* DEBUG IT */ printf(TAB TAB "processing VLQ :: ");
#endif

    const _Byte _sign_bit_mask =    0x80; // 0b10000000
    const _Byte _7_last_bits_mask = 0x7F; // 0b01111111

    unsigned long retval = 0; // 4 bytes maximum...

    while ( _index < len )
    {
        // grab a byte
        _curr_byte = bytes[_index++];

#if DEBUG
/* DEBUG IT */ print_byte_as_binary(&_curr_byte, 0 );
/* DEBUG IT */ printf(" ");
#endif

        retval<<=7;
        retval += ( _curr_byte & _7_last_bits_mask );

        if ( ( _curr_byte & _sign_bit_mask ) == 0 )
        {
            break;
        }
    }

    *val_ptr = retval;

#if DEBUG
    printf("processed %li bytes for VLQ.\n", _index);
#endif

    return _index;
}

/****************************************************************************************
*
*
*   -> Main Struct Methods
****************************************************************************************/
void parse_track_events( MiniMidi_Track *track, _Byte *evts_chunk )
{
    size_t _byte_counter = 0;
    size_t _event_counter = 0;

#if DEBUG
    size_t _debug_event_byte_count = 0;
#endif

    // Last status byte for running status handling.
    _Byte *_last_status_byte = NULL;
    // _Byte *_next_byte = NULL;
    uint8_t _data_bytes_count = 0;

    while ( _byte_counter < track->length )
    {

#if DEBUG
        /* DEBUG IT */_debug_event_byte_count = _byte_counter;
        /* DEBUG IT */ printf(BOLDWHITE "Starting grab %lith byte. byte_counter=%li\n" RESET, (_byte_counter + 1), _byte_counter);
#endif


        struct MiniMidi_Event evt;
        _byte_counter += read_VLQ_delta_t( evts_chunk + _byte_counter, track->length - _byte_counter, &(evt.delta_ticks));


#if DEBUG
        /* DEBUG IT */ printf(BOLDWHITE "Parsed Event number %lu:\n  " RESET CYAN "ticks=%lu\n" RESET, _event_counter, evt.delta_ticks );
        /* DEBUG IT */ printf(TAB "After moving, counter is at %lu.\n", _byte_counter);      
        /* DEBUG IT */ printf(TAB TAB "parseEvents::Next Bite is ");
        /* DEBUG IT */ print_byte_as_binary(evts_chunk + _byte_counter, 0);
        /* DEBUG IT */ printf("\n");
#endif

        if ( *(evts_chunk + _byte_counter) >= 0x80 )
        {
            // This is a new status byte (has the high bit set)
            evt.status_code = get_midi_status_code((evts_chunk + _byte_counter));
            _last_status_byte = (evts_chunk + _byte_counter);  // Remember for running status
            _byte_counter++;

        } else {
            // No new status byte — use running status
#if DEBUG
            /* DEBUG IT */ printf( TAB TAB RED "Status is Running!\n" RESET );
#endif

            evt.status_code = get_midi_status_code(_last_status_byte);
            // Note: byte_counter not incremented here, since there's no status byte.
        }

        if (evt.status_code == MIDI_INVALID) {
            printf(RED "Invalid status byte detected — aborting!\n" RESET);
            return;
        }
#if DEBUG
        /* DEBUG IT */ printf( TAB TAB GREEN "Status Code: " RESET );
         /* DEBUG IT */ print_midi_status_code( evt.status_code );
#endif
         _data_bytes_count = get_midi_data_byte_count( evt.status_code );
#if DEBUG
        /* DEBUG IT */ printf( TAB TAB "Event has %li data bytes: " RESET, _data_bytes_count);
#endif
        
 
        // extract databytes
        // when do we care?
        //  when evt is a Note On, and never elses
        if (evt.status_code == MIDI_NOTE_ON || evt.status_code == MIDI_NOTE_OFF)
        {
            evt.note = event_data_bytes_to_note(*(evts_chunk + _byte_counter));
#if DEBUG
            print_midi_note(evt.note);
            printf("\n");
#endif
        }
        _byte_counter += _data_bytes_count;
#if DEBUG
        /* DEBUG IT */ printf(TAB TAB "Event Size in Bytes: %li\n", (_byte_counter - _debug_event_byte_count));
        /* DEBUG IT */ printf(TAB TAB "Finishing %linth loop, byte_counter=%li.\n\n\n", _event_counter, _byte_counter);
#endif
        track->event_arr[_event_counter ++] = evt;

    }
    track->n_events = _event_counter;
}

// "Class" Methods
MiniMidi_Header *MiniMidi_Header_read( _Byte *file_contents )
{

    MiniMidi_Header *retval = (MiniMidi_Header*)malloc( sizeof( struct MiniMidi_Header ) );
    if (!retval) return NULL; 
    
    extract_number_from_byte_array( &(retval->length), file_contents, 4, 4 );
    extract_number_from_byte_array( &(retval->format), file_contents, 8, 2 );
    extract_number_from_byte_array( &(retval->ntrks), file_contents, 10, 2 );
    extract_number_from_byte_array( &(retval->division), file_contents, 12, 2 );

    return retval;
}

MiniMidi_Track *MiniMidi_Track_read( _Byte *file_content, size_t start_index, size_t total_chunk_len )
{
    MiniMidi_Track *track = (MiniMidi_Track*)malloc( sizeof( struct MiniMidi_Track ) );
    if (!track) return NULL; 

#if DEBUG
    printf(GREEN "Reading Track Chunk" RESET ": Starting at %lu / %lu Bytes.\n", start_index, total_chunk_len);
#endif
    track->length = total_chunk_len;

    // ESTIMATE: each event is minimum 3 bytes.
    size_t max_events = track->length / 3;
    track->event_arr = (MiniMidi_Event*)malloc( max_events * sizeof( MiniMidi_Event ) );

    extract_number_from_byte_array( &(track->length), file_content, start_index + 4, 4 );

    _Byte _track_bin_data[ track->length ];

    get_substring(file_content, _track_bin_data, start_index + 8, track->length, false );

    //      total                                    + Chunk Id + Chunk len
    assert( total_chunk_len == ( start_index + track->length + 4        + 4 ));
    parse_track_events( track, _track_bin_data );
    
    return track;
}

void MiniMidi_Header_print( MiniMidi_Header *mh )
{
    printf(BOLDWHITE "HEADER:\n---------------------------\n" RESET);
    printf(TAB WHITE "Chunk Size:" RESET BOLDWHITE " %lu" RESET " bytes\n", mh->length );
    printf(TAB WHITE "Format Code:" RESET " %i.\n", mh->format);
    printf(TAB WHITE "Number Of Tracks:" RESET " %i.\n", mh->ntrks);
    printf(TAB WHITE "Division:" RESET " %i.\n", mh->division);
    printf("---------------------------\n");
}

void MiniMidi_Event_print( MiniMidi_Event *me )
{
    printf(TAB TAB "Event:\n");
    printf(TAB TAB "Delta: %lu ticks\n", me->delta_ticks );
    printf(TAB TAB "Status Code: ");
    print_midi_status_code(me->status_code);
    printf("\n");
    printf(TAB TAB "Note: ");
    print_midi_note(me->note);
    printf("\n");
    printf(TAB TAB "----\n");
}

void MiniMidi_Track_print( MiniMidi_Track *mt )
{
    printf(BOLDWHITE "TRACK:" RESET "\n---------------------------\n");
    printf(TAB WHITE "Chunk Size:" RESET BOLDWHITE" %li" RESET " bytes\n", mt->length );
    printf(TAB WHITE "Number of Events: %li \n" RESET, mt->n_events );

    for (int i = 0; i < mt->n_events; i++ ){
        MiniMidi_Event_print( &(mt->event_arr[i]) );
    }

    printf("---------------------------\n");
}

void MiniMidi_Header_free( MiniMidi_Header *header )
{
    free( header );
}

void MiniMidi_Track_free( MiniMidi_Track *track )
{
    free( track->event_arr);
    free( track );
}

void MiniMidi_File_free( MiniMidi_File *file )
{
    MiniMidi_Header_free( file->header );
    MiniMidi_Track_free( file->track );
    free( file );
}

void MiniMidi_File_print( MiniMidi_File *file )
{
    MiniMidi_Header_print( file->header );
    MiniMidi_Track_print( file->track );
}

MiniMidi_File *MiniMidi_File_read( _Byte *file_contents, size_t file_len )
{
    MiniMidi_File *retval = (MiniMidi_File*)malloc( sizeof( struct MiniMidi_File ) );
    if (!retval) return NULL; 
    retval->length = file_len;
    retval->header = MiniMidi_Header_read( file_contents );

    retval->track = MiniMidi_Track_read( file_contents, 14, file_len );

    return retval;
}

MiniMidi_File * MiniMidi_File_read_from_file( char *file_path, int path_len )
{
    FILE *fileptr;
    fileptr = fopen( file_path, "rb" );
    _Byte * buffer = 0;
    size_t length;

    if (fileptr)
    {
        fseek (fileptr, 0, SEEK_END);
        length = ftell (fileptr);

        fseek (fileptr, 0, SEEK_SET);
        buffer = malloc (length);

        if (buffer)
        {
            // fread returns read bytes.
            int freadres = fread (buffer, 1, length, fileptr);
            printf(GREEN "Success" RESET " Read %i bytes.\n", freadres);
        }

        fclose (fileptr);
    }

    // MiniMidi_File *retval = MiniMidi_File_read( buffer, length );
    MiniMidi_File *retval = MiniMidi_File_read( buffer, length );
    free( buffer );

    return retval;
}
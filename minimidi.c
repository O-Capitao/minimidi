#include "stdbool.h"
#include "minimidi.h"
#include "string.h"
#include "assert.h"
#include "stdio.h"



void print_byte_as_binary(_Byte *byte, int little_endian) {
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
    
    // printf("\n");
}


// Function to get MIDI Status Code from a byte
MidiStatusCode getMidiStatusCode( _Byte *byte) {
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

void printMidiStatusCode(MidiStatusCode status) {
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

uint8_t getMidiDataByteCount(MidiStatusCode status) {
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

void printNote(MidiNote note) {
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



MidiNote eventDataBytesToNote( _Byte eventDataByte ){

    // printf(TAB TAB "Note N:%i\n" RESET, eventDataByte );
    MidiNote result;
    
    // Ensure note_number is in valid MIDI range (0-127)
    if (eventDataByte > 127) {
        eventDataByte = 127;  // Clamp to max MIDI value
    }
    
    // Extract chromatic note (0-11) using modulo
    result.note = (Note)(eventDataByte % 12);
    
    // Calculate octave
    // MIDI note 0 = C-1, 12 = C0, 24 = C1, ..., 60 = C4, etc.
    // So we divide by 12 and subtract 1 to get standard octave numbers
    result.octave = (eventDataByte / 12) - 1;
    
    return result;
}

void reverseByteArray(_Byte* arr, int len)
{
    _Byte aux;
    for (int i = 0; i < len / 2; i++)
    {
        aux = arr[i];
        arr[i] = arr[len-1-i];
        arr[len-1-i] = aux;
    }
}

void extractNumberFromByteArray( void* tgt, _Byte* src, int start_ind, int len )
{
    _Byte extracted_bytes[len];
    memcpy( extracted_bytes, &src[start_ind], len );
    reverseByteArray( extracted_bytes, len );
    memcpy(tgt, extracted_bytes, len);
}

void getSubstring(_Byte *src_str, unsigned char* tgt_str, int start_index, int n_elements_to_copy, bool add_null_termination )
{
    memcpy( tgt_str, &src_str[start_index], n_elements_to_copy );

    if (add_null_termination)
    {
        tgt_str[ n_elements_to_copy ] = '\0';
    }
}

size_t readVLQ_DeltaT( _Byte *bytes, size_t len, unsigned long *val_ptr)
{
    size_t _index = 0;
    _Byte _curr_byte;

    /* DEBUG SHIT */ printf(TAB TAB "processing VLQ :: ");


    const _Byte _sign_bit_mask =    0x80; // 0b10000000
    const _Byte _7_last_bits_mask = 0x7F; // 0b01111111

    unsigned long retval = 0; // 4 bytes maximum...

    while ( _index < len )
    {
        // grab a byte
        _curr_byte = bytes[_index++];
        
        /* DEBUG SHIT */ print_byte_as_binary(&_curr_byte, 0 );
        /* DEBUG SHIT */ printf(" ");
        
        retval<<=7;
        retval += ( _curr_byte & _7_last_bits_mask );

        if ( ( _curr_byte & _sign_bit_mask ) == 0 )
        {
            break;
        }
    }

    *val_ptr = retval;

    printf("processed %li bytes for VLQ.\n", _index);
    return _index;
}

void parseEvents( /* struct MidiFileTrackEvent *evts,*/ _Byte *evts_chunk, size_t chunk_len  ){
    size_t _byte_counter = 0;
    size_t _event_counter = 0;
    size_t _debug_event_byte_count = 0;

    // Last status byte for running status handling.
    _Byte *_last_status_byte = NULL;
    // _Byte *_next_byte = NULL;
    uint8_t _data_bytes_count = 0;

    while ( _byte_counter < chunk_len )
    {
        
        /* DEBUG SHIT */_debug_event_byte_count = _byte_counter;
        /* DEBUG SHIT */ printf(BOLDWHITE "Starting grab %lith byte. byte_counter=%li\n" RESET, (_byte_counter + 1), _byte_counter);



        struct MidiFileTrackEvent evt;
        _byte_counter += readVLQ_DeltaT( evts_chunk + _byte_counter, chunk_len - _byte_counter, &(evt.delta_ticks));



        /* DEBUG SHIT */ printf(BOLDWHITE "Parsed Event number %lu:\n  " RESET CYAN "ticks=%lu\n" RESET, _event_counter, evt.delta_ticks );
        /* DEBUG SHIT */ printf(TAB "After moving, counter is at %lu.\n", _byte_counter);      
        /* DEBUG SHIT */ printf(TAB TAB "parseEvents::Next Bite is ");
        /* DEBUG SHIT */ print_byte_as_binary(evts_chunk + _byte_counter, 0);
        /* DEBUG SHIT */ printf("\n");


        // if (nextByte >= 0x80) { // 0b10000000k
        if ( *(evts_chunk + _byte_counter) >= 0x80 )
        {
            // This is a new status byte (has the high bit set)
            evt.status_code = getMidiStatusCode((evts_chunk + _byte_counter));
            _last_status_byte = (evts_chunk + _byte_counter);  // Remember for running status
            _byte_counter++;

        } else {
            // No new status byte — use running status
            /* DEBUG SHIT */ printf( TAB TAB RED "Status is Running!\n" RESET );
            evt.status_code = getMidiStatusCode(_last_status_byte);
            // Note: byte_counter not incremented here, since there's no status byte.
        }
        



        if (evt.status_code == MIDI_INVALID) {
            printf(RED "Invalid status byte detected — aborting!\n" RESET);
            return;
        }

         /* DEBUG SHIT */ printf( TAB TAB GREEN "Status Code: " RESET );
         /* DEBUG SHIT */ printMidiStatusCode( evt.status_code );

         _data_bytes_count = getMidiDataByteCount( evt.status_code );
        /* DEBUG SHIT */ printf( TAB TAB "Event has %i data bytes: " RESET, _data_bytes_count);

        
 
        // extract databytes
        // when do we care?
        //  when evt is a Note On, and never elses
        if (evt.status_code == MIDI_NOTE_ON || evt.status_code == MIDI_NOTE_OFF)
        {
            evt.note = eventDataBytesToNote(*(evts_chunk + _byte_counter));
            printNote(evt.note);
            printf("\n");
        }
        _byte_counter += _data_bytes_count;

        /* DEBUG SHIT */ printf(TAB TAB "Event Size in Bytes: %li\n", (_byte_counter - _debug_event_byte_count));
        /* DEBUG SHIT */ printf(TAB TAB "Finishing %linth loop, byte_counter=%li.\n\n\n", _event_counter, _byte_counter);

        _event_counter ++;

    }
}

// "Class" Methods
struct MidiFileHeaderChunk readHeaderChunk( _Byte *fileContents )
{

    struct MidiFileHeaderChunk retval;
    getSubstring( fileContents, retval.chunkType, 0, 4, true );

    
    extractNumberFromByteArray( &(retval.length), fileContents, 4, 4 );
    extractNumberFromByteArray( &(retval.format), fileContents, 8, 2 );
    extractNumberFromByteArray( &(retval.ntrks), fileContents, 10, 2 );
    extractNumberFromByteArray( &(retval.ntrks), fileContents, 10, 2 );
    extractNumberFromByteArray( &(retval.division), fileContents, 12, 2 );

    // printf(TAB BOLDGREEN "- HEADER CHUNK:" RESET "\n");

    printf(TAB TAB WHITE "Chunk Type:" RESET " %s \n", retval.chunkType);
    printf(TAB TAB WHITE "Chunk Length:" RESET " %i\n", retval.length );
    printf(TAB TAB WHITE "Format Code:" RESET " %i.\n", retval.format);
    printf(TAB TAB WHITE "Number Of Tracks:" RESET " %i.\n", retval.ntrks);
    printf(TAB TAB WHITE "Division:" RESET " %i.\n", retval.division);

    return retval;
}

struct MidiFileTrackChunk readTrackChunk( _Byte *fileContent, size_t startIndex, size_t totalLen )
{
    printf(GREEN "Reading Track Chunk" RESET ": Starting at %lu / %lu Bytes.\n", startIndex, totalLen);
    struct MidiFileTrackChunk retval;

    getSubstring( fileContent, retval.chunkType, startIndex, 4, true ); 
    extractNumberFromByteArray( &(retval.length), fileContent, startIndex + 4, 4 );
    getSubstring(fileContent, retval.trackBinData, startIndex + 8, retval.length, false );

    //      total                                    + Chunk Id + Chunk len
    assert( totalLen == ( startIndex + retval.length + 4        + 4 ));
    printf("\n" GREEN "Track Chunk Len: " RESET "%i bytes.\n", retval.length);

    parseEvents( retval.trackBinData, retval.length );
    


    return retval;
}

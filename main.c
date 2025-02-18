#include "stdio.h"
#include "string.h"
#include "stdlib.h"
#include "stdbool.h"

#include "mylog.h"

#define ARG_MAX_LEN 100

typedef unsigned char _Byte;



MyLogger *logger_ptr = NULL;

struct MidiFileHeaderChunk {
    unsigned char chunkType[5];
    __uint32_t length;
    __uint16_t format;
    __uint16_t ntrks;
    __uint16_t division;
};

struct MidiFileTrackEvent {
    unsigned long delta_ticks;
    _Byte evt_code[2];
    _Byte evt_data[2]; // not always 2, but its 1 or 2, moving on....
};

struct MidiFileTrackChunk {
    unsigned char chunkType[5];
    __uint32_t length;
    _Byte* trackBinData;
    size_t n_events;
    struct MidiFileTrackEvent *event_arr;
};

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
    const _Byte _sign_bit_mask = 0b1000000;
    const _Byte _7_last_bits_mask = 0b0111111;
    unsigned long retval = 0; // 4 bytes maximum....

    // for (int i = 0; i < len;  i++ ){
    //     printf("%b\n", bytes[i]);
    // }

    while ( _index < len )
    {
        // grab a byte
        _curr_byte = bytes[_index++];
        printf("    grabbed byte: %b - %x \n", _curr_byte, _curr_byte );

        // accumulate accumulator with least significant 7 bits of curr byte (base 128)        
        // shift accumulated value 7 bits to the left
        printf("    retval is %b before ops.\n", retval );
        retval<<=7;
        printf("    shifted 7 to the left: retval = %b .\n", retval );
        // sum with 7 last bits of curr byte
        retval += (_curr_byte & _7_last_bits_mask);
        printf("    sum with curr byte: retval = %b .\n\n", retval );
        // if signal bit == 0, this is the last byte in the VLQ
        if ( (_curr_byte & _sign_bit_mask) == 0)
        {
            break;
        }
    }

    *val_ptr = retval;
    printf("    VLQ is %lu bytes long.\n", _index );
    return _index; 
}

void parseEvents( /* struct MidiFileTrackEvent *evts,*/ _Byte *evts_chunk, size_t chunk_len  ){
    size_t byte_counter = 0;
    size_t event_counter = 0;
    // size_t var_len_quant_delta_t = 0;
    // struct MidiFileTrackEvent evts[100];
    
    while ( byte_counter < 1 ) //chunk_len )
    {
        struct MidiFileTrackEvent evt;
        byte_counter += readVLQ_DeltaT( evts_chunk, chunk_len, &(evt.delta_ticks) );

        printf("Parsed Event number %lu:\n  " CYAN "ticks=%lu\n" RESET, event_counter, evt.delta_ticks );

        event_counter ++;
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

    printf(TAB BOLDGREEN "- HEADER CHUNK:" RESET "\n");

    printf(TAB TAB WHITE "Chunk Type:" RESET " %s \n", retval.chunkType);
    printf(TAB TAB WHITE "Chunk Length:" RESET " %i\n", retval.length );
    printf(TAB TAB WHITE "Format Code:" RESET " %i.\n", retval.format);
    printf(TAB TAB WHITE "Number Of Tracks:" RESET " %i.\n", retval.ntrks);
    printf(TAB TAB WHITE "Division:" RESET " %i.\n", retval.division);

    return retval;
}

struct MidiFileTrackChunk readTrackChunk( _Byte *fileContent, size_t startIndex, size_t totalLen )
{
    struct MidiFileTrackChunk retval;

    getSubstring( fileContent, retval.chunkType, startIndex, 4, true );
    extractNumberFromByteArray( &(retval.length), fileContent, startIndex + 4, 4 );
    getSubstring(fileContent, retval.trackBinData, startIndex + 8, retval.length, false );

    printf(TAB BOLDGREEN "- TRACK CHUNK:" RESET "\n");
    printf(TAB TAB WHITE "Chunk Type:" RESET " %s\n", retval.chunkType );
    printf(TAB TAB WHITE "Chunk Length:" RESET " %i\n", retval.length );

    parseEvents( retval.trackBinData, retval.length );
    
    return retval;
}



int main( int argc, char *argv[] )
{

    logger_ptr = MyLogger__create("./myfile.log");

    // safety for
    if (argc < 2){
        MyLogger_error( logger_ptr, "Please supply an argument." );
        MyLogger_destroy(logger_ptr);
        return 1;
    }

    size_t sizeofarg = strlen(argv[1]);
    if (sizeofarg > ARG_MAX_LEN){
        MyLogger_error( logger_ptr, "Input arg is too long." );
        MyLogger_destroy(logger_ptr);
        return 1;
    }

    FILE *fileptr;

    MyLogger_success( logger_ptr, "Parsed Inputs." );
    MyLogger_logStr(logger_ptr, "STARTING parse of ", argv[1]);
    
    fileptr = fopen( argv[1], "rb" );
    _Byte * buffer = 0;
    size_t length;

    MyLogger_flush(logger_ptr);

    if (fileptr)
    {
        fseek (fileptr, 0, SEEK_END);
        length = ftell (fileptr);

        // printf("File is %li bytes long.\n", length);
        MyLogger_logInt(logger_ptr, "File Len: ", length);

        fseek (fileptr, 0, SEEK_SET);
        buffer = malloc (length);

        if (buffer)
        {
            // fread returns read bytes.
            int freadres = fread (buffer, 1, length, fileptr);
            // printf("fread returns %i\n", freadres);
            MyLogger_logInt(logger_ptr, "Read Bytes: ", freadres);
        }

        fclose (fileptr);
    }

    // Parse The Midi File :)
    struct MidiFileHeaderChunk header = readHeaderChunk(buffer);
    // headeer chunk is ! ALWAYS ! 14 bytes
    struct MidiFileTrackChunk track = readTrackChunk(buffer, 14, length );


    free( buffer );
    MyLogger_destroy(logger_ptr);
    return 0;
}
#include "stdio.h"
#include "string.h"
#include "stdlib.h"
#include "stdbool.h"

#define ARG_MAX_LEN 100

struct MidiFileHeader {
    char chunkType[5];
    __uint32_t length;
    __uint16_t format;
    __uint16_t ntrks;
    __uint16_t division;
};


void reverseCharArray(char *src, char *tgt, int len)
{
    for (int i = 0; i < len; i++){
        tgt[len-i-1] = src[i];
    }
}


void getSubstring(char *src_str, char* tgt_str, int start_index, int n_elements_to_copy, bool add_null_termination )
{
    memcpy( tgt_str, &src_str[start_index], n_elements_to_copy );

    if (add_null_termination)
    {
        tgt_str[ n_elements_to_copy ] = '\0';
    }
}

struct MidiFileHeader readHeaderChunk(char *fileContents){

    struct MidiFileHeader retval;
    getSubstring( fileContents, retval.chunkType, 0, 4, true );

    char lenghtBytes[4];
    char reversedLengthBytes[4];

    // ! NOTE:
    // Midi num values -> always big endian 
    getSubstring( fileContents, lenghtBytes, 4, 4, false );
    reverseCharArray( lenghtBytes, reversedLengthBytes, 4 );
    memcpy( &(retval.length), reversedLengthBytes, 4 );

    // neext 2 bytes are for format
    char formatBytes[2], 
         reversedFormatBytes[2];

    getSubstring( fileContents, formatBytes, 8, 2, false );
    reverseCharArray( formatBytes, reversedFormatBytes, 2 );
    memcpy( &(retval.format), reversedFormatBytes, 2 );


    // neext 2 bytes are for number of tracks
    char ntrkBytes[2], 
         reversedNtrkBytes[2];

    getSubstring( fileContents, ntrkBytes, 10, 2, false );
    reverseCharArray( ntrkBytes, reversedNtrkBytes, 2 );
    memcpy( &(retval.ntrks), reversedNtrkBytes, 2 );

    // last 2 bytes are for divisin
    char divBytes[2], 
         reversedDivBytes[2];

    getSubstring( fileContents, divBytes, 12, 2, false );
    reverseCharArray( divBytes, reversedDivBytes, 2 );
    memcpy( &(retval.division), reversedDivBytes, 2 );

    return retval;
}


int main(int argc, char *argv[]){

    // safety for
    if (argc < 2){
        printf( "\033[1;31mERROR!\033[0m : Please supply an argument.\n");
        return 1;
    }

    size_t sizeofarg = strlen(argv[1]);
    if (sizeofarg > ARG_MAX_LEN){
        printf( "\033[1;31mERROR!\033[0m : Input arg is too long!\n");
    }

    FILE *fileptr;

    printf("parsing %s", argv[1]);
    
    fileptr = fopen( argv[1], "rb" );
    char * buffer = 0;
    long length;


    if (fileptr)
    {
        fseek (fileptr, 0, SEEK_END);
        length = ftell (fileptr);

        printf("File is %li bytes long.\n", length);

        fseek (fileptr, 0, SEEK_SET);
        buffer = malloc (length);

        if (buffer)
        {
            // fread returns read bytes.
            int freadres = fread (buffer, 1, length, fileptr);
            printf("fread returns %i\n", freadres);
        }

        fclose (fileptr);
    }

    printf("buffer is %li bytes long.\n", strlen(buffer));

    // Parse The Midi File :)
    struct MidiFileHeader header = readHeaderChunk(buffer);
    printf("HEY! Got substring: %s \n\n", header.chunkType);
    printf("Hey! Also here's the length of the thing: %i\n", header.length );
    printf("Hey! Also here's the format code: %i.\n", header.format);
    printf("Hey! Also here's the number of tracks: %i.\n", header.ntrks);
    printf("Hey! Also here's the division: %i.\n", header.division);

    return 0;
}
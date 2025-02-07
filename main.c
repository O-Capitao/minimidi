#include "stdio.h"
#include "string.h"
#include "stdlib.h"
#include "stdbool.h"

#define ARG_MAX_LEN 100


// terminal stuff
#define RESET   "\033[0m"
#define BLACK   "\033[30m"      /* Black */
#define RED     "\033[31m"      /* Red */
#define GREEN   "\033[32m"      /* Green */
#define YELLOW  "\033[33m"      /* Yellow */
#define BLUE    "\033[34m"      /* Blue */
#define MAGENTA "\033[35m"      /* Magenta */
#define CYAN    "\033[36m"      /* Cyan */
#define WHITE   "\033[37m"      /* White */
#define BOLDBLACK   "\033[1m\033[30m"      /* Bold Black */
#define BOLDRED     "\033[1m\033[31m"      /* Bold Red */
#define BOLDGREEN   "\033[1m\033[32m"      /* Bold Green */
#define BOLDYELLOW  "\033[1m\033[33m"      /* Bold Yellow */
#define BOLDBLUE    "\033[1m\033[34m"      /* Bold Blue */
#define BOLDMAGENTA "\033[1m\033[35m"      /* Bold Magenta */
#define BOLDCYAN    "\033[1m\033[36m"      /* Bold Cyan */
#define BOLDWHITE   "\033[1m\033[37m"      /* Bold White */

#define TAB "   "

struct MidiFileHeaderChunk {
    char chunkType[5];
    __uint32_t length;
    __uint16_t format;
    __uint16_t ntrks;
    __uint16_t division;
};

struct MidiFileTrackEvent {

};

struct MidiFileTrackChunk {
    char chunkType[5];
    __uint32_t length;
    char* trackChunkData;
};

void reverseCharArray(char* arr, int len)
{
    char aux;
    for (int i = 0; i < len / 2; i++)
    {
        aux = arr[i];
        arr[i] = arr[len-1-i];
        arr[len-1-i] = aux;
    }
}

void extractNumberFromByteArray( void* tgt, char* src, int start_ind, int len )
{
    char extracted_bytes[len];
    memcpy( extracted_bytes, &src[start_ind], len );
    reverseCharArray( extracted_bytes, len );
    memcpy(tgt, extracted_bytes, len);
}

void getSubstring(char *src_str, char* tgt_str, int start_index, int n_elements_to_copy, bool add_null_termination )
{
    memcpy( tgt_str, &src_str[start_index], n_elements_to_copy );

    if (add_null_termination)
    {
        tgt_str[ n_elements_to_copy ] = '\0';
    }
}

// "Class" Methods
struct MidiFileHeaderChunk readHeaderChunk(char *fileContents){

    struct MidiFileHeaderChunk retval;
    getSubstring( fileContents, retval.chunkType, 0, 4, true );

    
    extractNumberFromByteArray( &(retval.length), fileContents, 4, 4 );
    extractNumberFromByteArray( &(retval.format), fileContents, 8, 2 );
    extractNumberFromByteArray( &(retval.ntrks), fileContents, 10, 2 );
    extractNumberFromByteArray( &(retval.ntrks), fileContents, 10, 2 );
    extractNumberFromByteArray( &(retval.division), fileContents, 12, 2 );

    printf(TAB GREEN "Header Parsed" RESET "\n");

    printf(TAB TAB WHITE "Chunk Type:" RESET " %s \n", retval.chunkType);
    printf(TAB TAB WHITE "Length of Header:" RESET " %i\n", retval.length );
    printf(TAB TAB WHITE "Format Code:" RESET " %i.\n", retval.format);
    printf(TAB TAB WHITE "Number Of Tracks:" RESET " %i.\n", retval.ntrks);
    printf(TAB TAB WHITE "Division:" RESET " %i.\n", retval.division);

    return retval;
}



//------------------------------------------------------------------------------------------
int main(int argc, char *argv[]){

    // safety for
    if (argc < 2){
        printf( RED "ERROR: " RESET "Please supply an argument.\n");
        return 1;
    }

    size_t sizeofarg = strlen(argv[1]);
    if (sizeofarg > ARG_MAX_LEN){
        printf( RED "ERROR: " RESET "Input arg is too long!\n");
        return 1;
    }

    FILE *fileptr;

    printf(WHITE "STARTING parse of %s\n\n" RESET, argv[1]);
    
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

    // Parse The Midi File :)
    struct MidiFileHeaderChunk header = readHeaderChunk(buffer);

    free( buffer );

    return 0;
}
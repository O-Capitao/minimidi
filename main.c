#include "stdio.h"
#include "string.h"
#include "stdlib.h"

#define ARG_MAX_LEN 100

struct MidiFileHeader {
    char* chunkType;
    __uint32_t length;
    __uint16_t format;
    __uint16_t ntrks;
    __uint16_t division;
};



struct MidiFileHeader readHeaderChunk(char *fileContents){
    return 0;
}


int main(int argc, char *argv[]){

    // safety for args
    if (argc < 2){
        printf( "\033[1;31mERROR!\033[0m : Please supply an argument.\n");
        return 1;
    }


    size_t sizeofarg = strlen(argv[1]);
    if (sizeofarg > ARG_MAX_LEN){
        printf( "\033[1;31mERROR!\033[0m : Input arg is too long!\n");
    }

    printf("len is %li.\n", sizeofarg );

    FILE *fileptr;
    char filepath[ strlen(argv[1]) ];

    


    strcpy(filepath, argv[1]);
    // char* filepath = "midi_test_case.mid";
    
    printf("parsing %s", filepath);
    fileptr = fopen( filepath, "rb" );
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
    // printf("%s\n", buff

    for (int i = 0; i < length; i++ ){
        printf("at %i: 0x%x or %c\n", i, buffer[i], buffer[i]);
    }

    return 0;
}
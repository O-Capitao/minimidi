#include "stdio.h"
#include "string.h"
#include "stdlib.h"
#include "stdbool.h"
#include "minimidi.h"
#include "globals.h"




#define ARG_MAX_LEN 100

int main( int argc, char *argv[] )
{

    // safety for
    if (argc < 2){
        printf(RED "ERROR" RESET " please supply args.\n");
        return 1;
    }

    size_t sizeofarg = strlen(argv[1]);
    if (sizeofarg > ARG_MAX_LEN){
        printf(RED "ERROR" RESET " Too many args.\n");
        return 1;
    }

    FILE *fileptr;

    
    fileptr = fopen( argv[1], "rb" );
    _Byte * buffer = 0;
    size_t length;


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
            printf(GREEN "Success" RESET " Read %i bytes.\n", freadres);
        }

        fclose (fileptr);
    }

    // Parse The Midi File :)
    MiniMidi_Header *header = MiniMidi_Header_read(buffer);
    MiniMidi_Header_print(header);


    MiniMidi_Track *track = MiniMidi_Track_read(buffer, 14, length );
    MiniMidi_Track_print(track);

    MiniMidi_Track_free( track );


    return 0;
}
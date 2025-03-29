#include "stdio.h"
#include "string.h"
#include "stdlib.h"
#include "stdbool.h"
#include "minimidi.h"
#include "minimidi-ui.h"
#include "globals.h"

// TUI
#include <ncurses.h>
#include <unistd.h>
#include <time.h>

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
    printf("Reading %s.\n", argv[1]);
    MiniMidiUi *ui = MiniMidiUi_init();
    MiniMidiUi_load_file( ui, argv[1] );
    MiniMidiUi_run(ui);
    MiniMidiUi_kill(ui);

    return 0;
}
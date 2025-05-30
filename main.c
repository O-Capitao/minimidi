#include <stdio.h>
#include <string.h>

#include "globals.h"
#include "minimidi.h"
#include "minimidi-tui.h"
#include "minimidi-log.h"

#define ARG_MAX_LEN 100

void quit( MiniMidi_TUI *ui, MiniMidi_File *f, int is_error )
{   
    MiniMidi_TUI_destroy(ui);
    MiniMidi_File_free( f );
    if (is_error)
    {
        printf(RED "ERROR" RESET "Houston we have a problem...");
    }
}

/***
 *  MAIN!
 */
int main( int argc, char *argv[] )
{
    // Catch Args
    if (argc < 2){
        printf(RED "ERROR" RESET " please supply args.\n");
        return 1;
    }

    size_t sizeofarg = strlen(argv[1]);
    if (sizeofarg > ARG_MAX_LEN){
        
        printf(RED "ERROR" RESET " Too many args.\n");
        return 1;
    }
    
    // Check tmux
    char *tmux = getenv("TMUX");

    if (tmux)
    {
        printf("Running inside tmux. Launching a new tmux session...\n");

        // Define the session name and command to run
        const char *session_name = "new_session";
        const char *cmd = "tmux new-session -d -s new_session './check_tmux'";

        // Execute the command
        int result = system(cmd);

        if (result == 0) {
            printf("New tmux session '%s' started successfully.\n", session_name);
        } else {
            printf("Failed to start new tmux session.\n");
        }
    }

    // init logger
    MiniMidi_Log *logger = MiniMidi_Log_init();

    // Read the file passed in by arg
    MiniMidi_File *midi_file = MiniMidi_File_init( argv[1] , logger );
    if (midi_file == NULL) {
        printf(RED "ERROR" RESET " Failed to read MIDI file: %s\n", argv[1]);
        return 1;
    }

    MiniMidi_TUI *ui = (MiniMidi_TUI*)malloc( sizeof( MiniMidi_TUI ) );
    MiniMidi_TUI_init(ui, midi_file, logger);

    int ERRSTATUS = 0;

    while (ui->is_running)
    {
        MiniMidi_TUI_render( ui );
        ERRSTATUS = MiniMidi_TUI_update( ui );
    }

    quit(ui, midi_file, ERRSTATUS ? true: false);
    MiniMidi_Log_free( logger );

    return 0;
}

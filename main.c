#include <stdio.h>
#include <string.h>


#include "globals.h"
#include "minimidi.h"
#include "minimidi-tui.h"
// #include "minimidi-audio.h"
#include "minimidi-log.h"
// #include "minimidi-transport.h"
// #include "minimidi-file.h"
#include "minimidi-proj.h"

#define ARG_MAX_LEN 100

void quit( MM_TUI *ui, MM_Midi_File *f, int is_error )
{   
    MM_TUI_destroy(ui);
    MM_File_free( f );
    if (is_error)
    {
        log_error("An error occurred, quitting.");
        printf(RED "ERROR" RESET " Houston we have a problem...\n");
    }
}

/***
 *  MAIN!
 */
int main( int argc, char *argv[] )
{
    // Catch Args
    if (argc < 2){
        log_fatal("Please supply args.");
        printf(RED "ERROR" RESET " please supply args.\n");
        return 1;
    }

    size_t sizeofarg = strlen(argv[1]);
    if (sizeofarg > ARG_MAX_LEN){
        log_fatal("Too many args.");
        printf(RED "ERROR" RESET " Too many args.\n");
        return 1;
    }
    
    // Check tmux
    char *tmux = getenv("TMUX");
    
    // init logger
    if (log_init("minimidi.log") != 0) {
        // If logger fails, we can't log the error, so print to stderr and exit.
        fprintf(stderr, "Failed to initialize logger. Exiting.\n");
        return 1;
    }
    log_info("main: initting.");

    // check if we're running in tmux
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

    // Read the project file
    MM_Project project;
    MM_Project_init( &project, argv[1] );

    MM_Ring_Buffer *cmd_queue = MM_Ring_Buffer__init(128, sizeof(MM_AudioCommand));

    MM_AudioEngine eng;
    MM_AudioEngine_init(&eng, &project, cmd_queue );

    // // MM_TUI *ui = (MM_TUI*)malloc( sizeof( MM_TUI ) );
    // // MM_TUI_init(ui, &midi_file, cmd_queue, &eng, bpm );

    // // int ERRSTATUS = 0;



    // /**
    //  * MAIN LOOP
    //  */
    // while (ui->is_running && ERRSTATUS == 0)
    // {
    //     ERRSTATUS = MM_TUI_step( ui );

    //     if (ERRSTATUS){
    //         printf("Like whatever");
    //     }
    // }

    // quit(ui, &midi_file, ERRSTATUS ? true: false);

    MM_Project_free( &project );
    log_deinit();

    return 0;
}

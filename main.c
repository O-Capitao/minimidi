#include "stdio.h"
#include "string.h"
#include "stdlib.h"
#include "stdbool.h"
#include "minimidi.h"
#include "globals.h"

// TUI
#include <ncurses.h>
#include <unistd.h>
#include <time.h>

#define ARG_MAX_LEN 100

// Util private stuff here at top
static const char *ALL_NOTES[] = {"A", "Bb", "B", "C", "Db", "D", "Eb", "E", "F", "Gb", "G", "Ab" };
static const int OCT_RANGE = 8;
static const int MAX_NOTE_VAL = OCT_RANGE * 12;

static const int LINES_PER_NOTE = 1;
static const int COLS_PER_BEAT = 1;

// size things
static const int VE_LEFT_LABELS_WIDTH = 5;
static const int VE_BOTT_LABELS_HEIGHT = 3;

static const int TOP_BAR_HEIGHT = 1;
static const int TOP_RIGHT_WIDTH = 10;
static const int BOTT_BAR_HEIGHT = 1;

// static int key;
int handle_keys( int *dummyval, bool *_is_running )
{
    int key = getch();

    if (key == ERR)
    {
        return 0; // No key pressed
    }

    switch (key)
    {
        case KEY_UP:
            // Handle up arrow key
            (*dummyval)++;
            break;
        case KEY_DOWN:
            (*dummyval)--;
            // Handle down arrow key
            break;
        case KEY_LEFT:
            // Handle left arrow key
            break;
        case KEY_RIGHT:
            // Handle right arrow key
            break;
        case 'q':
            *_is_running = false;
            break;

        default:
            break;
    }
    return 0;
}


int render_info( MiniMidi_File *file, int *outer_size )
{
    if (mvprintw(0, 0, "file: %s . size: %li bytes", file->filepath, file->length) > 0)
    {
        return 1;
    }

    if (mvprintw(0, outer_size[0] - TOP_RIGHT_WIDTH, "-CLEAN-"))
    {
        return 1;
    }

    return 0;
}

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

    // Read the file passed in by arg
    MiniMidi_File *midi_file = MiniMidi_File_read_from_file( argv[1] );

    // MiniMidi_File_print( midi_file );

    if (midi_file == NULL) {
        printf(RED "ERROR" RESET " Failed to read MIDI file: %s\n", argv[1]);
        return 1;
    }

    // Start UI
	initscr();			        /* Start curses mode 		*/
	raw();				        /* Line buffering disabled	*/
	keypad(stdscr, TRUE);		/* We get F1, F2 etc..		*/
	noecho();			        /* Don't echo() while we do getch */
    curs_set(0);                /* Hide Cursor*/

    int outer_size[2];
    int dummy_position[2];

    getmaxyx(stdscr, outer_size[1], outer_size[0]);
    dummy_position[0] = outer_size[0] / 2;
    dummy_position[1] = outer_size[1] / 2;


    WINDOW *midieditor_derwin = derwin( stdscr, 
        outer_size[1] - TOP_BAR_HEIGHT - BOTT_BAR_HEIGHT,
        outer_size[0],
        TOP_BAR_HEIGHT,
        0
    );
    
    bool _running = true;
    int dummy_val = 0;



    while (_running){

        clear();
        render_info( midi_file, outer_size );
        // mvprintw(0, outer_size[0] - TOP_RIGHT_WIDTH, "TOP RGHT" );

        box( midieditor_derwin, '|', '=' );
        mvwprintw( midieditor_derwin, dummy_position[1], dummy_position[0], "->%i", dummy_val);

        wrefresh( stdscr );
        wrefresh( midieditor_derwin );

        handle_keys( &dummy_val, &_running );
    }

    delwin( midieditor_derwin );
    endwin();
    
    MiniMidi_File_free( midi_file );
    return 0;
}
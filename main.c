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

static const int LINES_PER_NOTE = 2;
static const int COLS_PER_BEAT = 1;

// size things
static const int VE_LEFT_LABELS_WIDTH = 5;
static const int VE_BOTT_LABELS_HEIGHT = 3;

static const int TOP_BAR_HEIGHT = 1;
static const int TOP_RIGHT_WIDTH = 10;
static const int BOTT_BAR_HEIGHT = 1;

typedef struct UiState
{
    // ZOOM
    int bars_in_screen;
    int notes_in_screen;
    
    int logical_start[2]; // cols, lines
    int key_press_displacement[2]; // how much do we move each arrow key press?
    int grid_size[2];

    bool is_dirty;
    bool is_running;
} UiState;

void UiState_init( UiState *self )
{
    self->is_dirty = false;
    self->is_running = true;
    //
    self->bars_in_screen = 4;
    self->notes_in_screen = 12;
    //
    self->logical_start[0] = 0;
    self->logical_start[1] = 0;

    self->key_press_displacement[0] = 1;
    self->key_press_displacement[1] = 1;

    self->grid_size[0] = 0;
    self->grid_size[1] = 0;

}

void make_line( char *line_str,  int size )
{
    for (int i = 0; i < size -1 ; i++)
    {
        line_str[i] = '_';
    }

    line_str[size - 1] = '\0';
}

void UiState_update_sizes( UiState *self, int sz_cols, int sz_lines )
{
    // how many notes can we see on the screen?
    self->grid_size[0] = sz_cols - VE_LEFT_LABELS_WIDTH - 2;
    self->grid_size[1] = sz_lines - VE_BOTT_LABELS_HEIGHT - 2;

    self->notes_in_screen = sz_lines / LINES_PER_NOTE;
    self->bars_in_screen = sz_cols / ( 4 * COLS_PER_BEAT);

    printf("s");
}

int render_grid( WINDOW *tgt, UiState *state)
{
    int line_index;
    int line_size = state->grid_size[0] - VE_LEFT_LABELS_WIDTH;
    char line[ line_size ];
    make_line(line, line_size );

    for (int i_note = state->logical_start[1]; i_note < state->logical_start[1] + state->notes_in_screen; i_note ++ )
    {
        line_index = state->grid_size[1] - VE_BOTT_LABELS_HEIGHT - ( i_note - state->logical_start[1] ) * LINES_PER_NOTE;

        if (mvwprintw( tgt, line_index, VE_LEFT_LABELS_WIDTH, "%s", line ))
        {
            return 1;
        }

        line_index--;
        
        // make columns        
        for (int j_beat = state->logical_start[0]; j_beat < COLS_PER_BEAT * ( state->logical_start[0] + state->bars_in_screen * 4); j_beat++ )
        {
            if (mvwprintw( tgt, line_index, 10, "%s", "ooo" ))
            {
                return 1;
            }
        }
    }
    return 0;
}

int render_note_labels( WINDOW *tgt, UiState *state )
{
    int line_index,
        oct,
        note_index;

    for (int i_note = state->logical_start[1]; i_note < state->logical_start[1] + state->notes_in_screen; i_note ++ )
    {
        line_index = state->grid_size[1] - VE_BOTT_LABELS_HEIGHT - ( i_note - state->logical_start[1] ) * LINES_PER_NOTE;
        oct = i_note / 12;
        note_index = i_note % 12;  

        if (mvwprintw( tgt, line_index, 2, "%s", ALL_NOTES[note_index] ))
        {
            return 1;
        }
        if (mvwprintw( tgt, line_index, 4, "%d", oct ))
        {
            return 1;
        }
    }

    return 0;
}


// static int key;
int handle_keys( UiState *ui_state)
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
            // (*dummyval)++;
            break;
        case KEY_DOWN:
            // (*dummyval)--;
            // Handle down arrow key
            break;
        case KEY_LEFT:
            // Handle left arrow key
            break;
        case KEY_RIGHT:
            // Handle right arrow key
            break;
        case 'q':
        case 'Q':
            ui_state->is_running = false;
            break;
        case 'e':
        case 'E':
            ui_state->is_dirty = !ui_state->is_dirty;
            break;
        default:
            break;
    }
    return 0;
}

int render_info( UiState *ui_state, MiniMidi_File *file, int *outer_size )
{
    if (mvprintw( 0, 0, "file: %s . size: %li bytes . %li events in %li ticks / %li beats.", file->filepath, file->length, file->track->n_events, file->track->total_ticks, file->track->total_beats) > 0 )
    {
        return 1;
    }

    if (mvprintw(0, outer_size[0] - TOP_RIGHT_WIDTH, ui_state->is_dirty ? "-DIRTY-"  : "-CLEAN-"))
    {
        return 1;
    }

    return 0;
}

int render_midi_editor( UiState *ui_state, MiniMidi_File *file, WINDOW * target, int *outer_size )
{

    // find target bounds
    int size_x, size_y;
    getmaxyx(target, size_y, size_x);

    // update size things
    UiState_update_sizes(ui_state, size_x, size_y);

    // if (mvwprintw( target, 1, 1, "outer_size: [%i, %i]", outer_size[0], outer_size[1]))
    // {
    //     return 1;
    // }

    // if (mvwprintw( target, 2, 1, "inner_size: [%i, %i]", size_x, size_y ))
    // {
    //     return 1;
    // }

    render_note_labels( target, ui_state );
    render_grid( target, ui_state );

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
    UiState ui_state;
    UiState_init( &ui_state );

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
    getmaxyx(stdscr, outer_size[1], outer_size[0]);
    

    WINDOW *midieditor_derwin = derwin( stdscr, 
        outer_size[1] - TOP_BAR_HEIGHT - BOTT_BAR_HEIGHT,
        outer_size[0],
        TOP_BAR_HEIGHT,
        0
    );

    // UiState_update_sizes( &ui_state, outer_size[0], outer_size[1] - TOP_BAR_HEIGHT - BOTT_BAR_HEIGHT );

    while (ui_state.is_running)
    {
        clear();
        render_info( &ui_state, midi_file, outer_size );
        render_midi_editor( &ui_state, midi_file, midieditor_derwin, outer_size );

        box( midieditor_derwin, '|', '=' );
        wrefresh( stdscr );
        wrefresh( midieditor_derwin );
        handle_keys( &ui_state );
    }

    delwin( midieditor_derwin );
    endwin();
    
    // MiniMidi_File_print( midi_file );
    MiniMidi_File_free( midi_file );

    return 0;
}
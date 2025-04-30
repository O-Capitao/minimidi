#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <assert.h>


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

static const int LINES_PER_SEMITONE = 2;
static const int COLS_PER_BEAT = 4;

// time sig
static const int BEAT_PER_BAR = 4;

// Grid Subcomponent
static const int GRID_LEFT_LABELS_WIDTH = 8;
static const int GRID_BOTT_LABELS_HEIGHT = 3;
static const int GRID_PADDING_TOP = 2;


static const int TOP_BAR_HEIGHT = 1;
static const int TOP_RIGHT_WIDTH = 10;
static const int BOTT_BAR_HEIGHT = 1;

// aux for debugging
static int WIN_SIZE[2];

/***
 * AUX STRUCTS!
 */
typedef struct UiState
{
    // ZOOM
    int beats_in_grid;
    int semitones_in_grid;
    
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
    self->beats_in_grid = 0;
    self->semitones_in_grid = 0;
    //
    self->logical_start[0] = 0;
    self->logical_start[1] = 0;

    self->key_press_displacement[0] = 1;
    self->key_press_displacement[1] = 1;

    self->grid_size[0] = 0;
    self->grid_size[1] = 0;
    
}
void UiState_update_sizes( UiState *self, int sz_cols, int sz_lines )
{
    // how many notes can we see on the screen?
    self->grid_size[0] = sz_cols - GRID_LEFT_LABELS_WIDTH - 2;
    self->grid_size[1] = sz_lines - GRID_BOTT_LABELS_HEIGHT - 2;

    self->semitones_in_grid = self->grid_size[1] / LINES_PER_SEMITONE;
    self->beats_in_grid = self->grid_size[0] / ( COLS_PER_BEAT);

}

/***
 * RENDER!
 */
int render_grid( WINDOW *tgt, UiState *st)
{
    int line_index, col_index, beat_counter;
    
    chtype note_delim = '_';
    chtype bar_delim = ':';

    int err;

    for (int i_note = st->logical_start[1]; i_note < st->logical_start[1] + st->semitones_in_grid; i_note ++ )
    {
        line_index = GRID_PADDING_TOP + ((st->grid_size[1] - GRID_BOTT_LABELS_HEIGHT)- LINES_PER_SEMITONE * ( i_note - st->logical_start[1] ));
        assert(line_index > 0 && line_index < st->grid_size[1]);
        // draw horizontal line
        for (int j1 = GRID_LEFT_LABELS_WIDTH; j1 < st->grid_size[0] - 1; j1 += 2 )
        {
            if ( (err = mvwaddch( tgt, line_index, j1, note_delim )) )
            {
                return 1;
            }
        }

        // offset for "x" markers -> time
        line_index--;
        
        // make columns        
        for (int j_beat = st->logical_start[0]; j_beat < st->logical_start[0] + st->beats_in_grid; j_beat++ )
        {
            col_index = ( j_beat - st->logical_start[0] ) * COLS_PER_BEAT + GRID_LEFT_LABELS_WIDTH;
            
            if ( !(j_beat % BEAT_PER_BAR ))
            {
                beat_counter = j_beat / BEAT_PER_BAR;

                if ((err = mvwaddch( tgt, line_index, col_index, bar_delim )))
                {
                    return err;
                }

                // render beat counter lbls
                if ((err = mvwprintw( tgt, st->grid_size[1], col_index, "%d", beat_counter )))
                {
                    return err;
                }
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

    for (int i_note = state->logical_start[1]; i_note < state->logical_start[1] + state->semitones_in_grid; i_note ++ )
    {
        line_index = GRID_PADDING_TOP + ((state->grid_size[1] - GRID_BOTT_LABELS_HEIGHT)- LINES_PER_SEMITONE * ( i_note - state->logical_start[1] ));
        assert(line_index > 0 && line_index < state->grid_size[1]);

        oct = i_note / 12;
        note_index = i_note % 12;  

        int err;
        if (( err = mvwprintw( tgt, line_index, 3, "%s", ALL_NOTES[note_index]) ))
        {
            return err;
        }
        if (( err = mvwprintw( tgt, line_index, 5, "%d", oct )))
        {
            return err;
        }
    }

    return 0;
}

int render_bar_labels( WINDOW *tgt, UiState *state )
{
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

    // for debug
    WIN_SIZE[0] = size_x;
    WIN_SIZE[1] = size_y;

    // update size things
    UiState_update_sizes(ui_state, size_x, size_y);
    if (render_note_labels( target, ui_state )) return 1;
    if (render_grid( target, ui_state )) return 1;

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
            if (ui_state->logical_start[1] < MAX_NOTE_VAL ){
                ui_state->logical_start[1]++;
            }

            break;
        case KEY_DOWN:
            if (ui_state->logical_start[1] > 0){
                ui_state->logical_start[1]--;
            }
            
            // Handle down arrow key
            break;
        case KEY_LEFT:
            if (ui_state->logical_start[0] > 0)
            {
                ui_state->logical_start[0]--;
            }
            break;
        case KEY_RIGHT:
            ui_state->logical_start[0]++;
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

void quit( WINDOW *ed, MiniMidi_File *f, bool is_error )
{
    delwin( ed );
    endwin();
    
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

    // Read the file passed in by arg
    MiniMidi_File *midi_file = MiniMidi_File_read_from_file( argv[1] );
    UiState ui_state;
    UiState_init( &ui_state );
    int ERRSTATUS = 0;

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

    while (ui_state.is_running)
    {
        clear();
        if( render_info( &ui_state, midi_file, outer_size ) )
        {
            ERRSTATUS = 1;
            break;
        }

        if ( render_midi_editor( &ui_state, midi_file, midieditor_derwin, outer_size ) )
        {
            ERRSTATUS = 1;
            break;
        }

        box( midieditor_derwin, '|', '=' );
        wrefresh( stdscr );
        wrefresh( midieditor_derwin );
        handle_keys( &ui_state );
    }

    quit(midieditor_derwin, midi_file, ERRSTATUS ? true: false);

    printf("\nFinished! Used a WINDOW of size {cols=%d, lines=%d}.\n", WIN_SIZE[0], WIN_SIZE[1]);
    return 0;
}

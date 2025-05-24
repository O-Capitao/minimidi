#include <ncurses.h>

#include "minimidi-tui.h"

/**
 * Constants
 */
static const char *ALL_NOTES[] = {"A", "Bb", "B", "C", "Db", "D", "Eb", "E", "F", "Gb", "G", "Ab" };
static const int OCT_RANGE = 8;
static const int MAX_NOTE_VAL = OCT_RANGE * 12;

// vertical zoom is fixed.
static const int LINES_PER_SEMITONE = 2;

// Grid Subcomponent
static const int GRID_LEFT_LABELS_WIDTH = 8;
static const int GRID_BOTT_LABELS_HEIGHT = 3;
static const int GRID_PADDING_TOP = 2;

static const int TOP_BAR_HEIGHT = 1;
static const int TOP_RIGHT_WIDTH = 10;
static const int BOTT_BAR_HEIGHT = 1;

// Graphical elements:
static const chtype note_delim = '_';
static const chtype bar_delim = ':';

/**
 * PRIVATE
 */
int _init_ncurses( MiniMidi_TUI *self )
{
    // Start UI
	initscr();			        /* Start curses mode 		*/
	
    // check window initialization
    if (!stdscr) return 1;
    
    raw();				        /* Line buffering disabled	*/
	keypad(stdscr, TRUE);		/* We get F1, F2 etc..		*/
	noecho();			        /* Don't echo() while we do getch */
    curs_set(0);                /* Hide Cursor*/

    getmaxyx(stdscr, self->outer_size[1], self->outer_size[0]);

    self->grid_derwin = derwin( stdscr, 
        self->outer_size[1] - TOP_BAR_HEIGHT - BOTT_BAR_HEIGHT,
        self->outer_size[0],
        TOP_BAR_HEIGHT,
        0 );

    getmaxyx( self->grid_derwin, self->grid_size[1], self->grid_size[0]);
    assert(self->outer_size[0] == self->grid_size[0]);

    return 0;
}

int _handle_input( MiniMidi_TUI *self )
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
            if (self->logical_start[1] < MAX_NOTE_VAL ){
                self->logical_start[1]++;
            }

            break;
        case KEY_DOWN:
            if (self->logical_start[1] > 0){
                self->logical_start[1]--;
            }
            
            // Handle down arrow key
            break;
        case KEY_LEFT:
            if (self->logical_start[0] > 0)
            {
                self->logical_start[0]--;
            }
            break;
        case KEY_RIGHT:
            self->logical_start[0]++;
            break;
        case 'q':
        case 'Q':
            self->is_running = false;
            break;
        case 'e':
        case 'E':
            self->is_dirty = !self->is_dirty;
            break;
        case KEY_RESIZE:
            clear();
            // mvprintw(0, 0, "COLS = %d, LINES = %d", COLS, LINES);
            // for (int i = 0; i < COLS; i++)
            //     mvaddch(1, i, '*');
            refresh();
            break;


        default:
            break;
    }
    return 0;
}

int _update_sizes( MiniMidi_TUI *self )
{
    getmaxyx(stdscr, self->outer_size[1], self->outer_size[0]);
    getmaxyx(self->grid_derwin, self->grid_size[1], self->grid_size[0]);


    self->logical_size[1] = self->grid_size[1] / LINES_PER_SEMITONE;

    // logical size:
    // - how many beats fit into the screen
    int bars_in_screen = self->grid_size[0] / ( self->cols_in_beat * self->beats_in_bar +1 );
    self->logical_size[0] = ( self->grid_size[0] - bars_in_screen ) / self->cols_in_beat;

    return 0;
}

int _render_note_labels( MiniMidi_TUI *self )
{
    int line_index,
        oct,
        note_index;

    
    for (int i_note = self->logical_start[1]; i_note < self->logical_start[1] + self->logical_size[1]; i_note ++ )
    {
        line_index = GRID_PADDING_TOP + (( self->grid_size[1] - GRID_BOTT_LABELS_HEIGHT) - LINES_PER_SEMITONE * ( i_note - self->logical_start[1] ));
        assert(line_index > 0 && line_index < self->grid_size[1]);

        oct = i_note / 12;
        note_index = i_note % 12;  

        int err;
        if (( err = mvwprintw( self->grid_derwin, line_index, 3, "%s", ALL_NOTES[note_index]) ))
        {
            return err;
        }
        if (( err = mvwprintw( self->grid_derwin, line_index, 5, "%d", oct )))
        {
            return err;
        }
    }

    return 0;
}

int _render_info( MiniMidi_TUI *self)
{
    if (mvprintw( 0, 0, "file: %s . size: %li bytes . %li events in %li ticks / %li beats.", 
            self->file->filepath, 
            self->file->length,
            self->file->track->n_events,
            self->file->track->total_ticks,
            self->file->track->total_beats) > 0 )
    {
        return 1;
    }

    if (mvprintw(0, self->outer_size[0] - TOP_RIGHT_WIDTH, self->is_dirty ? "-DIRTY-"  : "-CLEAN-"))
    {
        return 1;
    }

    return 0;
}

// int _new_and_improved__render_grid( MiniMidi_TUI *self )
// {
//     int line_i, col_i, beat_counter, err;

//     for (int i_note = self->logical_start[1]; i_note < self->logical_start[1] + self->logical_size[1]; i_note ++ )
//     {
//         line_i = GRID_PADDING_TOP + ((self->grid_size[1] - GRID_BOTT_LABELS_HEIGHT)- LINES_PER_SEMITONE * ( i_note - self->logical_start[1] ));
//     }
// }


int _render_grid( MiniMidi_TUI *self )
{
    int err;
    int line_index, aux_line_index, col_index, beat_counter, bar_counter;
    // MiniMidi_Event_List_Node *cursor;

    // MiniMidi_get_events_in_tick_range(
    //     self->file,
    //     self->midi_events_list,
    //     self->file->header->ppqn * self->logical_start[0],
    //     self->file->header->ppqn * (self->logical_start[0] + self->logical_size[0]) );
    // assert( self->logical_start[0] > 0 && self->logical_start[1] > 0 );

    for (int i_note = self->logical_start[1]; i_note < self->logical_start[1] + self->logical_size[1]; i_note ++ )
    {
        // get a terminal line nr to draw on
        line_index = GRID_PADDING_TOP + ((self->grid_size[1] - GRID_BOTT_LABELS_HEIGHT)- LINES_PER_SEMITONE * ( i_note - self->logical_start[1] ));
        
        assert(line_index > 0 && line_index < self->grid_size[1]);
        aux_line_index = line_index - 1;
        beat_counter = self->logical_start[0];

        // cycle through drawable cols
        for (int j = GRID_LEFT_LABELS_WIDTH + 1; j < self->grid_size[0] - 1; j ++ )  /* the 1s account for box */
        {

            // draw note lines (horizontal grid)
            if (j % 2 == 0)
            {
                if ( (err = mvwaddch( self->grid_derwin, line_index, j, note_delim )) )
                    return 1;
            }


            // draw beat / bar delims
            if ( j % self->cols_in_beat == 0 )
            {
                beat_counter ++;

                if (beat_counter % ( self->beats_in_bar + 1) == 0) /** + 1 for the vertical delimiter which uses 1 col  */
                {
                    if ( (err = mvwaddch( self->grid_derwin, aux_line_index, j, bar_delim )) )
                        return 1;
                }
            }
            
        }
    }

    // do a stupid block for test
    mvwaddch(self->grid_derwin, 10, 10, ' '|A_REVERSE);
    return 0;
}
/**
 * PUBLIC
 */
int MiniMidi_TUI_init( MiniMidi_TUI *self, MiniMidi_File *file )
{
    self->is_dirty = false;
    self->is_running = true;
    //
    self->logical_size[0] = 0;
    self->logical_size[1] = 0;
    //
    self->logical_start[0] = 0;
    self->logical_start[1] = 0;
    //
    self->grid_size[0] = 0;
    self->grid_size[1] = 0;
    //
    self->outer_size[0] = 0;
    self->outer_size[1] = 0;
    
    // ZOOM ETERNAL
    self->cols_in_beat = 3;

    // TIME SIG
    self->beats_in_bar = 4;
    //
    self->file = file;
    self->midi_events_list = MiniMidi_Event_LList_init();

  
    if ( _init_ncurses(self) ) return 1;

    // struct sigaction sa;
    // memset(&sa, 0, sizeof(struct sigaction));
    // sa.sa_handler = handle_winch;
    // sigaction(SIGWINCH, &sa, NULL);

    return 0;
}


int MiniMidi_TUI_update( MiniMidi_TUI *self )
{
    // just handle_input here?
    _handle_input(self);
    return 0;
}


int MiniMidi_TUI_render( MiniMidi_TUI *self )
{
    clear();
    _update_sizes( self );

    if (_render_info( self )) return 1;
    if (_render_note_labels( self )) return 1;
    if (_render_grid( self )) return 1;

    box( self->grid_derwin, '|', '=' );

    wrefresh( stdscr );
    wrefresh( self->grid_derwin );
    
    return 0;
}

int MiniMidi_TUI_destroy( MiniMidi_TUI *self)
{
    delwin( self->grid_derwin );
    endwin();
    free(self);

    return 0;
}

#include <ncurses.h>

#include "minimidi-tui.h"

/**
 * Constants
 */
static const char *ALL_NOTES[] = { "C", "Db", "D", "Eb", "E", "F", "Gb", "G", "Ab", "A", "Bb", "B" };
static const int OCT_RANGE = 8;
static const int MAX_NOTE_VAL = OCT_RANGE * 12;

// vertical zoom is fixed.
static const int LINES_PER_SEMITONE = 2;

// Grid Subcomponent
static const int GRID_LEFT_LABELS_WIDTH = 7;

static const int TOP_BAR_HEIGHT = 1;
static const int TOP_RIGHT_WIDTH = 10;
static const int BOTT_BAR_HEIGHT = 1;

// Graphical elements:
static const chtype note_delim = '_';
static const chtype bar_delim = '\'';

/**
 * PRIVATE
 */
enum COLOR_PAIRS {
    RED_ON_BLK = 1,
    GREEN_ON_BLK = 2,
    BLACK_ON_CYAN = 3,
    BLACK_ON_GREEN = 4
};
/**
 * ! COORDINATE TRANSFORMS
 * 
 *  beat <-> col in grid
 *  note <-> row in grid
 *
 */

 /**
  * How many from start of screen until 1st
  * fully displayed bar
  */
int __calc_1st_bar_offset_logical( int x0, int beat_per_bar ) {
    int rem = beat_per_bar - x0 % beat_per_bar;
    return rem == 4 ? 0 : rem;
}

int _coords__beat_2_grid_col( int start_beat, int beat, int col_per_beat, int beats_in_bar )
{
    return (beat - start_beat) * col_per_beat;
}

int _coords__grid_col_2_beat( int start_beat, int col, int col_per_beat, int beats_in_bar )
{
    return (col / col_per_beat) + start_beat;
}

int _coords__note_2_grid_row( int start_note, int note, int row_per_note, int l_y_grid )
{
    return l_y_grid - 2 /*box*/ - ( note - start_note ) * row_per_note;
}

int _coords__grid_row_2_note( int start_note, int row, int row_per_note, int l_y_grid )
{
    return start_note - ( row + 2 - l_y_grid ) / row_per_note;
}

int _update_sizes( MiniMidi_TUI *self )
{
    getmaxyx(stdscr, self->outer_size[1], self->outer_size[0]);
    getmaxyx(self->grid_derwin, self->grid_size[1], self->grid_size[0]);

    self->logical_size[0] = _coords__grid_col_2_beat( self->logical_start[0], self->grid_size[0], self->cols_in_beat, self->beats_in_bar );
    self->logical_size[1] = ( self->grid_size[1] - 2 ) / LINES_PER_SEMITONE;

    return 0;
}
/**
* When app starts:
* snap window to show events instead of (C0, 1st beat) corner
*/
int _snap_to_first_events( MiniMidi_TUI *self )
{
    // find 1st NOTE_ON evt
    MiniMidi_Event *e;
    // bool found = false;
    int ind = 0;

    while (ind < self->file->track->n_events ) {

        e = &(self->file->track->event_arr[ind++]);

        // find first NOTE_ON since 1st event
        // might be something else?
        if (e->status_code == MIDI_NOTE_ON) {
            break;
        }
    }

    self->logical_start[0] = e->abs_ticks / self->file->header->ppqn;
    
    int note_int = ( e->note.octave * 12 ) + (int)( e->note.note );
    
    self->logical_start[1] = ( note_int > self->logical_size[1] / 2 ) ?
        note_int - self->logical_size[1] / 2
        : 0;

    return 0;
}

int _init_ncurses( MiniMidi_TUI *self )
{
    // Start UI
	initscr();			        /* Start curses mode 		*/
	
    // Check if terminal supports color
    if (!has_colors()) {
        endwin();
        printf("Your terminal does not support color\n");
        return 1;
    }

    // check window initialization
    if (!stdscr) return 1;
    
    raw();				        /* Line buffering disabled	*/
	keypad(stdscr, TRUE);		/* We get F1, F2 etc..		*/
	noecho();			        /* Don't echo() while we do getch */
    curs_set(0);                /* Hide Cursor*/
    
    start_color();
    use_default_colors();  // Use terminal theme colors

        // Define color pairs (pair_number, foreground, background)
    init_pair( RED_ON_BLK,    COLOR_RED,   COLOR_WHITE );
    init_pair( GREEN_ON_BLK,  COLOR_GREEN, COLOR_BLACK );
    init_pair( BLACK_ON_CYAN, COLOR_BLACK, COLOR_CYAN );
    init_pair( BLACK_ON_GREEN, COLOR_BLACK, COLOR_MAGENTA );


    getmaxyx(stdscr, self->outer_size[1], self->outer_size[0]);

    self->grid_derwin = derwin( stdscr, 
        self->outer_size[1] - TOP_BAR_HEIGHT - BOTT_BAR_HEIGHT,
        self->outer_size[0],
        TOP_BAR_HEIGHT,
        0 );

    getmaxyx( self->grid_derwin, self->grid_size[1], self->grid_size[0]);
    assert(self->outer_size[0] == self->grid_size[0]);
    
    _update_sizes( self );
    _snap_to_first_events( self );

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
        case '+':
            self->cols_in_beat++;
            break;
        case '-':
            if (self->cols_in_beat>1) self->cols_in_beat--;
            break;

        default:
            break;
    }
    return 0;
}

int _render_note_labels( MiniMidi_TUI *self )
{
    int line_index,
        oct,
        note_index;

    
    for (int i_note = self->logical_start[1]; i_note < self->logical_start[1] + self->logical_size[1]; i_note ++ )
    {
        line_index = _coords__note_2_grid_row( self->logical_start[1], i_note, LINES_PER_SEMITONE, self->grid_size[1] );
        
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



int _render_grid( MiniMidi_TUI *self ){

    int err;
    int line_index, aux_line_index, beat_counter, bar_counter;
    bool is_new_beat = false;
    int col_offset = GRID_LEFT_LABELS_WIDTH + 1;
    int col_in_grid = col_offset;
    char bar_number_srt[10];
    int bar_offset = self->logical_start[0] / self->beats_in_bar;

    for (int i_note = self->logical_start[1]; i_note < self->logical_start[1] + self->logical_size[1]; i_note ++ ){

        line_index = _coords__note_2_grid_row( self->logical_start[1], i_note, LINES_PER_SEMITONE, self->grid_size[1] );
        
        assert(line_index > 0 && line_index < self->grid_size[1]);
        
        aux_line_index = line_index - 1;        // where bar delimiters are drawed into
        beat_counter = self->logical_start[0];  // keep track of actual beats, not just cols
        bar_counter = 0;
        

        // cycle through drawable cols
        for (int j = col_offset; j < self->grid_size[0] - 1 /* box */; j ++ ){

            // dash under even beats
            if ( beat_counter % 2 == 0 ){
                if ( (err = mvwaddch( self->grid_derwin, line_index, j, note_delim )) )
                    return 1;
            }

            col_in_grid = j - col_offset;
            
            // beat is incremented every cols_in_beat
            is_new_beat = col_in_grid != 0 && ( (col_in_grid + 1) % self->cols_in_beat ) == 0;
            
            if (is_new_beat) {
                beat_counter++;

                if ( beat_counter % self->beats_in_bar == 0) {
                
                    bar_counter++;
                
                    if ( (err = mvwaddch( self->grid_derwin, aux_line_index, j + 1, bar_delim )) )
                        return 1;

                    // annotate the bar num for the 1st line only
                    if (i_note == (self->logical_start[1] + self->logical_size[1] - 1) 
                        && j < self->grid_size[0] - 10
                    ){
                        
                        wattron( self->grid_derwin, COLOR_PAIR(2));
                        snprintf( bar_number_srt, 10, "BAR%i", bar_offset + bar_counter );
                        mvwprintw(self->grid_derwin, aux_line_index, j + 2, bar_number_srt);
                        wattroff( self->grid_derwin, COLOR_PAIR(2));
                    }
                }
            }  
        }
    }

    return 0;
}


int _render_midi( MiniMidi_TUI *self )
{
    static int rendered_ONs[2][100];
    int rendered_ONs_ctr = 0;

    for (int i = 0; i < 2; i++){
        for (int j = 0; j < 2; j++){
            rendered_ONs[i][j] = 0;
        }
    }


    MiniMidi_Event_List_Node *cursor, *aux;

    MiniMidi_get_events_in_range(
        self->file,
        self->midi_events_list,
        self->file->header->ppqn * self->logical_start[0],
        self->file->header->ppqn * (self->logical_start[0] + self->logical_size[0]),
        self->logical_start[1],
        self->logical_start[1] + self->logical_size[1]
    );


    cursor = self->midi_events_list->first;
    int cursor_beat, cursor_note, beat_col, note_line, cursor_beat_aux, beat_col_aux;

    while (cursor)
    {
        cursor_beat = cursor->value->abs_ticks / self->file->header->ppqn;
        cursor_note = ( cursor->value->note.octave * 12 ) + (int)( cursor->value->note.note );

        note_line = _coords__note_2_grid_row( self->logical_start[1], cursor_note, LINES_PER_SEMITONE, self->grid_size[1] );
        beat_col = GRID_LEFT_LABELS_WIDTH + 1 + _coords__beat_2_grid_col( self->logical_start[0], cursor_beat, self->cols_in_beat, self->beats_in_bar );

        // draw this fucker
        if ( cursor->value->status_code == MIDI_NOTE_ON )
        {
            rendered_ONs[0][rendered_ONs_ctr] = beat_col;
            rendered_ONs[1][rendered_ONs_ctr] = note_line;
            rendered_ONs_ctr ++;

            // paint leading edge of event
            wattron( self->grid_derwin,  COLOR_PAIR (BLACK_ON_CYAN )); // wattron( self->grid_derwin, COLOR_PAIR(2));
            mvwaddch( self->grid_derwin, note_line, beat_col, ' ' ); // |A_REVERSE);
            wattroff( self->grid_derwin,  COLOR_PAIR (BLACK_ON_CYAN ));

            // paint remaining until corresponding note_off
            aux = (cursor->value)->next;

            if (aux == NULL){

            }

            cursor_beat_aux = aux->value->abs_ticks / self->file->header->ppqn + 1;

            // check if next is in bounds:
            if (cursor_beat_aux < self->logical_start[0] + self->logical_size[0]){
                beat_col_aux = GRID_LEFT_LABELS_WIDTH + 1 + _coords__beat_2_grid_col( self->logical_start[0], cursor_beat_aux, self->cols_in_beat, self->beats_in_bar );
            } else {
                beat_col_aux = self->grid_size[0];
            }

            // paint from beat_col + 1 until beat_col_aux
            wattron( self->grid_derwin, COLOR_PAIR(BLACK_ON_GREEN));
            for (int b = beat_col + 1; b < beat_col_aux; b++) {
                mvwaddch( self->grid_derwin, note_line, b, ' ' );
            }
            wattroff( self->grid_derwin, COLOR_PAIR(BLACK_ON_GREEN ));

        } else if ( cursor->value->status_code == MIDI_NOTE_OFF )
        {

        }

        cursor = cursor->next;
    }


    return 0;
}
/**
 * PUBLIC
 */
int MiniMidi_TUI_init( MiniMidi_TUI *self, MiniMidi_File *file, MiniMidi_Log *_logger )
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
    self->cols_in_beat = 4;

    // TIME SIG
    self->beats_in_bar = 4;
    //
    self->file = file;
    self->midi_events_list = MiniMidi_Event_LList_init();
    self->logger = _logger;
  
    if ( _init_ncurses(self) ) return 1;

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
    if (_render_midi( self )) return 1;

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

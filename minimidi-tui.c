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
        case '+':
            self->cols_in_beat++;
            break;
        case '-':
            if (self->cols_in_beat>1) self->cols_in_beat--;
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


bool _is_in_bounds( MiniMidi_TUI *self, int beat, int note )
{
    return beat > self->logical_start[0]
        && beat < self->logical_start[0] + self->logical_size[0] 
        && note > self->logical_start[1] 
        && note < self->logical_start[1] + self->logical_size[1];
}


/**
 * ! COORDINATE TRANSFORMS
 * 
 *  beat <-> col in grid
 *  note <-> row in grid
 *
 */
int _note_number_to_row_in_grid( MiniMidi_TUI *self, int note )
{
    int notes_above = self->logical_size[1] - note;
    return self->grid_size[1] - notes_above * LINES_PER_SEMITONE + 1;
}

int _midi_note_to_row_in_grid( MiniMidi_TUI *self, MidiNote *note )
{
    return _note_number_to_row_in_grid( self, 12 * note->octave + (int)(note->note) );
}


int _beat_to_col_in_grid( MiniMidi_TUI *self, int beat )
{
    int beats_on_screen = (beat - self->logical_start[0]);
    int delimiters_on_screen = beats_on_screen / self->beats_in_bar;

    return GRID_LEFT_LABELS_WIDTH + 1 + beats_on_screen * self->cols_in_beat + delimiters_on_screen;
}

int _render_note_labels( MiniMidi_TUI *self )
{
    int line_index,
        oct,
        note_index;

    
    for (int i_note = self->logical_start[1]; i_note < self->logical_start[1] + self->logical_size[1]; i_note ++ )
    {
        line_index = _note_number_to_row_in_grid( self, i_note );
        //GRID_PADDING_TOP + (( self->grid_size[1] - GRID_BOTT_LABELS_HEIGHT) - LINES_PER_SEMITONE * ( i_note - self->logical_start[1] ));
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

int _render_grid( MiniMidi_TUI *self )
{
    int err;
    int line_index, aux_line_index, beat_counter;

    for (int i_note = self->logical_start[1]; i_note < self->logical_start[1] + self->logical_size[1]; i_note ++ )
    {
        // get a terminal line nr to draw on
        line_index = _note_number_to_row_in_grid( self, i_note );
        //GRID_PADDING_TOP + ((self->grid_size[1] - GRID_BOTT_LABELS_HEIGHT)- LINES_PER_SEMITONE * ( i_note - self->logical_start[1] ));
        
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

    // mvwaddch(self->grid_derwin, 10, 10, ' '|A_REVERSE);
    return 0;
}


int _render_midi( MiniMidi_TUI *self )
{

    MiniMidi_Event_List_Node *cursor;

    MiniMidi_get_events_in_range(
        self->file,
        self->midi_events_list,
        self->file->header->ppqn * self->logical_start[0],
        self->file->header->ppqn * (self->logical_start[0] + self->logical_size[0]),
        self->logical_start[1],
        self->logical_start[1] + self->logical_size[1]
    );


    cursor = self->midi_events_list->first;
    int cursor_beat, cursor_note, beat_col, note_line;

    while (cursor)
    {
        cursor_beat = cursor->value->abs_ticks / self->file->header->ppqn;
        cursor_note = ( cursor->value->note.octave * 12 ) + (int)( cursor->value->note.note );

        if ( _is_in_bounds( self, cursor_beat, cursor_note ) )
        {
            beat_col = _beat_to_col_in_grid( self, cursor_beat );
            note_line = _midi_note_to_row_in_grid( self, &(cursor->value->note) );
            
            // draw this fucker
            if ( cursor->value->status_code == MIDI_NOTE_ON )
            {

                // MiniMidi_Log_dumb_append( self->logger, MiniMidi_Log_format_string_static("Writing evt: beat:%d, note:%d", beat_col, note_line));
                mvwaddch( self->grid_derwin, note_line, beat_col, 'x' |A_REVERSE);

            } else if ( cursor->value->status_code == MIDI_NOTE_OFF )
            {

            }
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
    self->cols_in_beat = 3;

    // TIME SIG
    self->beats_in_bar = 4;
    //
    self->file = file;
    self->midi_events_list = MiniMidi_Event_LList_init();
    self->logger = _logger;
  
    if ( _init_ncurses(self) ) return 1;

    // log stuff
    char log_helper[ LOG_LINE_MAX_LEN ];

    // for (int i = 0; i < self->file->track->n_events; i ++)
    // {
    //     MiniMidi_Event_to_string_log( 
    //         &(self->file->track->event_arr[i]),
    //         log_helper );
        
    //     MiniMidi_Log_dumb_append(self->logger, log_helper);
    // }

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

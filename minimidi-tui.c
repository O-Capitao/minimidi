#include <ncurses.h>
#include <time.h>
#include <unistd.h>

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
  * How many from start of screen until 1st
  * fully displayed bar
  */
int __calc_1st_bar_offset_logical( int x0, int beat_per_bar ) {
    int rem = beat_per_bar - x0 % beat_per_bar;
    return rem == 4 ? 0 : rem;
}

// we end the "playback" part of the file
// by default in the end of the bar
// 
// returns: last playable tick
int __calc_end_of_playback( int last_tick, int ppqn, int beat_in_bar ) {
    int ticks_per_bar = ppqn * beat_in_bar;
    int bars_in_playback = last_tick / ticks_per_bar;

    return ticks_per_bar * ( bars_in_playback + 1 );
}

int _coords__note_2_grid_row( int start_note, int note, int row_per_note, int l_y_grid )
{
    return l_y_grid - 2 /*box*/ - ( note - start_note ) * row_per_note;
}

int _coords__grid_row_2_note( int start_note, int row, int row_per_note, int l_y_grid )
{
    return start_note - ( row + 2 - l_y_grid ) / row_per_note;
}

int _update_sizes( MM_TUI *self )
{
    getmaxyx(stdscr, self->outer_size[1], self->outer_size[0]);
    getmaxyx(self->grid_derwin, self->grid_size[1], self->grid_size[0]);

    self->logical_size[0] = self->grid_size[0] * self->ticks_per_col;
    self->logical_size[1] = ( self->grid_size[1] - 2 ) / LINES_PER_SEMITONE;

    // move increment is always one bar, figure oput later how to handle cleanly
    self->move_increment = 2 * self->file->header->ppqn;


    if (self->is_playing){

        int _ticks_offset =
            ( self->cursor_position_ticks >= self->logical_size[0] / 2 )
            ? self->logical_size[0] / 2
            : self->cursor_position_ticks;

        self->logical_start[0] = self->cursor_position_ticks - _ticks_offset;

        } else {
        self->is_render_requested = true;
    }
    

    return 0;
}
/**
* When app starts:
* snap window to show events instead of (C0, 1st beat) corner
*/
int _snap_to_first_events( MM_TUI *self )
{
    // find 1st NOTE_ON evt
    MM_Event *e;
    
    int ind = 0;

    while (ind < self->file->track->n_events ) {

        e = &(self->file->track->event_arr[ind++]);

        // find first NOTE_ON since 1st event
        // might be something else?
        if (e->status_code == MIDI_NOTE_ON) {
            break;
        }
    }

    // set logical start to start of last bar
    int bars_before = e->abs_ticks / (self->file->header->ppqn * self->beats_in_bar );
    self->logical_start[0] = bars_before * self->file->header->ppqn * self->beats_in_bar;
    
    int note_int = ( e->note.octave * 12 ) + (int)( e->note.note );
    
    self->logical_start[1] = ( note_int > self->logical_size[1] / 2 ) ?
        note_int - self->logical_size[1] / 2
        : 0;

    return 0;
}

int _init_ncurses( MM_TUI *self )
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
    nodelay(stdscr, 1);
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

    self->playback_derwin = derwin( stdscr,
        4,
        25,
        5,
        10
    );

    getmaxyx( self->grid_derwin, self->grid_size[1], self->grid_size[0]);
    assert(self->outer_size[0] == self->grid_size[0]);
    
    _update_sizes( self );
    _snap_to_first_events( self );

    // init playback panel
    self->is_playing = false;

    return 0;
}

int _handle_input( MM_TUI *self )
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
            if (self->logical_start[0] > self->move_increment) // dont allow to go bellow zero
            {
                self->logical_start[0] -= self->move_increment;
            } else {
                self->logical_start[0] = 0;
            }

            if (!self->is_playing){
                self->cursor_position_ticks = self->logical_start[0];
            }

            break;
        case KEY_RIGHT:
            self->logical_start[0] += self->move_increment;

            log_debug("minimidi-tui.c > _handle_input() : moving by %i, new start at %i", self->move_increment, self->logical_start[0]);
    
            break;
        // PLAY THAT FUNKY MUSIC WHITE BOY
        case ' ':
            log_debug("minimidi-tui.c > _handle_input() : pressed SPACE");
            self->is_playing = !self->is_playing;
            MM_AudioCommand cmd;
            cmd.cmd_type = self->is_playing ?
                MM_CMD_PLAY : MM_CMD_PAUSE;
            MM_Ring_Buffer__push( self->cmd_queue, &cmd );

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
            self->ticks_per_col /= 2;
            break;
        case '-':
            self->ticks_per_col *= 2;
            break;

        default:
            break;
    }
    return 0;
}

int _render_note_labels( MM_TUI *self )
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

int _render_info( MM_TUI *self)
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

int _draw_bar_label( MM_TUI *self, int bar_n, int line_index, int col_index ){
    static char bar_number_srt[10];
    
    wattron( self->grid_derwin, COLOR_PAIR(2));
    snprintf( bar_number_srt, 10, "BAR%i", bar_n);
    mvwprintw(self->grid_derwin, line_index, col_index, bar_number_srt);
    wattroff( self->grid_derwin, COLOR_PAIR(2));

    return 0;
}


int _render_grid( MM_TUI *self ){
   
    int err;
    int line_index, aux_line_index, beat_counter, bar_counter, col_in_grid;
    int ppqn = self->file->header->ppqn;

    bool is_new_beat = false;

    // count 1 extra bar because BAR is not zero-based
    int bar_offset = (self->logical_start[0] / ppqn) / self->beats_in_bar + 1;
    int x_ticks = 0;



    for (int i_note = self->logical_start[1]; i_note < self->logical_start[1] + self->logical_size[1]; i_note ++ ){

        line_index = _coords__note_2_grid_row( self->logical_start[1], i_note, LINES_PER_SEMITONE, self->grid_size[1] );
        
        assert(line_index > 0 && line_index < self->grid_size[1]);
        
        aux_line_index = line_index - 1;                // where bar delimiters are drawed into
        beat_counter = self->logical_start[0] / ppqn;  //  keep track of actual beats, not just cols
        bar_counter = 0;
        
        // cycle through drawable cols
        for (int j = GRID_LEFT_LABELS_WIDTH; j < self->grid_size[0] - 1 /* box */; j ++ ){
            

            
            // dash under even beats
            if ( beat_counter % 2 == 0 ){
                if ( (err = mvwaddch( self->grid_derwin, line_index, j, note_delim )) )
                    return 1;
            }

            col_in_grid = j - GRID_LEFT_LABELS_WIDTH + 1;

            x_ticks = self->logical_start[0] + col_in_grid * self->ticks_per_col;

            // beat is incremented every cols_in_beat
            is_new_beat = ( x_ticks - beat_counter * ppqn ) >=  ppqn;


            
            if (is_new_beat) {

                beat_counter++;



                if ( beat_counter % self->beats_in_bar == 0) {
 
                    bar_counter++;

                    if ((err = mvwaddch( self->grid_derwin, aux_line_index, j, bar_delim )))
                        return 1;
                
                    // annotate the bar num for the 1st line only
                    if (i_note == (self->logical_start[1] + self->logical_size[1] - 1) && j < self->grid_size[0] - 10 ){


                        
                        _draw_bar_label( self, bar_offset + bar_counter, aux_line_index, j + 2 );
                    }
                }
            }
        }
    }

    // finish by drawing the label for BAR 1 if it's visible
    if (self->logical_start[0] % (4 * self->file->header->ppqn) == 0 ){
        _draw_bar_label( 
            self, 
            1, 
            _coords__note_2_grid_row( self->logical_start[1], self->logical_start[1] + self->logical_size[1] - 1, LINES_PER_SEMITONE, self->grid_size[1]) - 1,
            GRID_LEFT_LABELS_WIDTH + 1 );
    }

    return 0;
}


int _render_midi( MM_TUI *self )
{
    MM_Event_LList_Node *cursor;
    MM_Event *aux;

    MM_File_get_events_in_range(
        self->file,
        self->midi_events_screen_list,
        self->logical_start[0],
        self->logical_start[0] + self->logical_size[0],
        self->logical_start[1],
        self->logical_start[1] + self->logical_size[1]
    );

    cursor = self->midi_events_screen_list->first;
    int cursor_tick, cursor_note, tgt_col, note_line, cursor_tick_aux, tgt_col_aux;

    while (cursor)
    {
        cursor_tick = cursor->value->abs_ticks;
        cursor_note = ( cursor->value->note.octave * 12 ) + (int)( cursor->value->note.note );

        note_line = _coords__note_2_grid_row( self->logical_start[1], cursor_note, LINES_PER_SEMITONE, self->grid_size[1] );


        tgt_col = GRID_LEFT_LABELS_WIDTH + ( (cursor_tick - self->logical_start[0]) / self->ticks_per_col );



        // draw this fucker
        if ( cursor->value->status_code == MIDI_NOTE_ON )
        {
            // paint leading edge of event
            wattron( self->grid_derwin,  COLOR_PAIR (BLACK_ON_CYAN ));
            mvwaddch( self->grid_derwin, note_line, tgt_col, ' ' );
            wattroff( self->grid_derwin,  COLOR_PAIR (BLACK_ON_CYAN ));

            // paint remaining until corresponding note_off
            aux = (cursor->value)->next;
            cursor_tick_aux = aux->abs_ticks;
            
            if ( cursor_tick_aux < self->logical_start[0] + self->logical_size[0]){
                tgt_col_aux = GRID_LEFT_LABELS_WIDTH + (( cursor_tick_aux - self->logical_start[0]) / self->ticks_per_col );
            } else {
                tgt_col_aux = self->grid_size[0];
            }

            wattron( self->grid_derwin, COLOR_PAIR(BLACK_ON_GREEN));
                
            for (int b = tgt_col + 1; b < tgt_col_aux; b++) {
                mvwaddch( self->grid_derwin, note_line, b, ' ' );
            }
            wattroff( self->grid_derwin, COLOR_PAIR(BLACK_ON_GREEN ));

        } else if ( cursor->value->status_code == MIDI_NOTE_OFF )
        {
            // handle cases of no NOTE_ON in screen
            aux = (cursor->value)->prev;
            assert( aux != NULL );

            if (aux->abs_ticks < self->logical_start[0]){
                
                // paint from beat_col + 1 until beat_col_aux
                wattron( self->grid_derwin, COLOR_PAIR(BLACK_ON_GREEN));
                
                for (int b = GRID_LEFT_LABELS_WIDTH + 1; b <= tgt_col; b++) {
                    mvwaddch( self->grid_derwin, note_line, b, ' ' );
                }
                
                wattroff( self->grid_derwin, COLOR_PAIR(BLACK_ON_GREEN ));
            }
        }

        cursor = cursor->next;
    }

    return 0;
}

int _render_playback( MM_TUI *self ){

    static char aux_str[50];
    int _lines, _cols;

    if (self->is_playing){
        
        //get size of playback derwin
        getmaxyx( self->playback_derwin, _lines, _cols);
        
        // make a run of clear
        for (int i = 0; i < _cols; i++){
            for (int j = 0; j < _lines; j++){
                mvwaddch( self->playback_derwin, j,  i, ' ' );
            }
        }

        // draw stuff now :)
        box(self->playback_derwin, '|', '=');
        snprintf( aux_str, 50, "t=%f s", self->playback_time );

        mvwprintw(self->playback_derwin, 1, 3, "-PLAYING-");
        mvwprintw(self->playback_derwin, 2, 3, aux_str);

        // get cursor position and paint it in the main window
        // knowing that time = something

        // get position
        int tgt_col = GRID_LEFT_LABELS_WIDTH + ( (self->cursor_position_ticks - self->logical_start[0]) / self->ticks_per_col );

        wattron( stdscr, COLOR_PAIR(2));
        mvwaddch( stdscr, self->outer_size[1] - 5, tgt_col, '^' );
        mvwaddch( stdscr, self->outer_size[1] - 4, tgt_col, '|' );
        mvwaddch( stdscr, self->outer_size[1] - 3, tgt_col, '|' );
        wattroff( stdscr, COLOR_PAIR(2));
    }

    return 0;
}

/**
 * PUBLIC
 */
int MM_TUI_init( MM_TUI *self, MM_File *file, MM_Ring_Buffer *cmd_queue, MM_AudioEngine *audio_engine, unsigned int bpm )
{
    self->is_dirty = false;
    self->is_running = true;
    self->is_render_requested = true;
    
    // logica size of the grid!
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
    // self->cols_in_beat = 4;
    self->ticks_per_col = file->header->ppqn / 4; // start at 4 cols -> 1 beat

    // TIME SIG
    self->beats_in_bar = 4;

    //
    self->file = file;
    self->midi_events_screen_list = MM_Event_LList_init();
    self->midi_events_audio_list = MM_Event_LList_init();
    self->fps = 15;

    // INIT PLAYBACK STUFF
    self->playback_time = 0;
    self->last_playback_time = 0;
    
    self->bpm = bpm;

    self->delta_t = 1.0 /  (double)self->fps;

    self->cursor_position_ticks = 0;
    self->cmd_queue = cmd_queue;
    self->audio_engine = audio_engine;

    if ( _init_ncurses(self) ) return 1;

    log_debug("TUI Init: done");

    return 0;
}

int MM_TUI_render(MM_TUI *s ){

    if (s->is_playing || s->is_render_requested){
        clear();
        if (_render_info( s )) {
            return 1;
        }
        if (_render_note_labels( s )) {
            return 1;
        }
        if (_render_grid( s )) {
            return 1;
        }
        if (_render_midi( s )) {
            return 1;
        }
    
        if (s->is_playing){
            // show a lil panel with a clock running
            if (_render_playback( s )) {
                return 1;
            }

            box( s->grid_derwin, '|', '=' );

            wrefresh( stdscr );
            wrefresh( s->grid_derwin );
        }
    }

    return 0;
}

int MM_TUI_step( MM_TUI *self ){

    clock_t step_start, step_end;
    double ellapsed_s;

    step_start = clock();
    log_debug("Entering MM_TUI_step.");

    // get input
    _handle_input( self );
    _update_sizes( self );
    MM_TUI_render( self );

    step_end = clock();
    ellapsed_s = (double)(step_end - step_start) / (double)CLOCKS_PER_SEC;

    // assert(self->delta_t_ms > (1000 * ellapsed_s));
    log_trace("Worked for %g s", ellapsed_s);

    if (self->is_playing){
        self->last_playback_time = self->playback_time;
        self->playback_time = atomic_load_explicit(&self->audio_engine->posted_audio_time, memory_order_relaxed);
        
        // handle loop around
        if (self->playback_time < self->last_playback_time){
            self->last_playback_time = 0;
        }

        // playback cursor positioned according
        self->cursor_position_ticks = MM_Util_s_to_tick( self->playback_time, self->bpm, self->file->header->ppqn);
    }

    double _sleep_t = self->delta_t- ellapsed_s;
    usleep( (int)(_sleep_t * 1e6) );
    
    return 0;
} 

int MM_TUI_destroy( MM_TUI *self)
{
    delwin( self->grid_derwin );
    endwin();
    free(self);


    return 0;
}

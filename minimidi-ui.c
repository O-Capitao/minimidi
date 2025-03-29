#include <unistd.h>
#include <time.h>
#include <string.h>

#include "minimidi-ui.h"

#define RED_ON_BLK 1
#define BLU_ON_WHITE 2


MiniMidiUi *MiniMidiUi_init()
{
    MiniMidiUi *retval = (MiniMidiUi*)malloc(sizeof( MiniMidiUi ));

    initscr();    // Start ncurses
    
    // colors
    start_color();
    init_pair(RED_ON_BLK, COLOR_BLACK, COLOR_RED );
    init_pair(BLU_ON_WHITE, COLOR_WHITE, COLOR_BLUE );
    
    noecho();     // Don't echo any keypresses
    curs_set(0);  // Don't show cursor
    keypad(stdscr, true); // Enable special keys

    // create subwindows:
    
    // Get screen size
    // !NOTE
    // all coords are (y, x) -> (lines, cols) -> as per curses convention
    int height, width;
    getmaxyx(stdscr, height, width);

    // Layout primitives
    int top_bar_height = 2;
    int edit_width = 10;
    int bott_bar_height = 1;

    // Info Subwindow
    retval->info_sw = subwin( stdscr,
        top_bar_height,
        width - edit_width,
        0,
        0);

    retval->mode_sw = subwin( stdscr, 
        top_bar_height,
        edit_width,
        0,
        width - edit_width);

    retval->viewereditor_sw = subwin( stdscr, 
        height - top_bar_height - bott_bar_height,
        width,
        top_bar_height,
        0);

    retval->cmds_sq = subwin( stdscr, 
        bott_bar_height,
        width,
        height - bott_bar_height,
        0);
    

    return retval;
}

// int random_int(int min, int max) {
//     return min + rand() % (max - min + 1);
// }

int MiniMidiUi_run( MiniMidiUi *self )
{
    bool _running = true;
    clock_t start, end;
    double cpu_time_used;

    MiniMidi_File_print( self->file );
     // does this propagate to subwindows?

    while (_running)
    {
       
        start = clock();

        wclear(stdscr);

        if (MiniMidiUi_render_info(self) > 0) break; // TODO: error handling
        if (MiniMidiUi_render_mode(self) > 0) break;
        if (MiniMidiUi_render_vieweditor(self) > 0) break;        






        mvwprintw(self->cmds_sq, 0, 0, "4");

        box(self->viewereditor_sw, '|', '=' );

        wrefresh(stdscr);








        // wait till time for next cycle pls.
        end = clock();

        cpu_time_used = ((double) (end - start)) / CLOCKS_PER_SEC;

        if (cpu_time_used < 0.05){
            usleep(50000 - cpu_time_used);
        }
    }

    return 0;
}

int MiniMidiUi_load_file( MiniMidiUi *self, char *filepath )
{
    // send old file to trash if loadded
    if (self->file != NULL)
    {
        MiniMidi_File_free( self->file );
    }

    self->file = MiniMidi_File_read_from_file( filepath );
    return 0;
}

// TODO: check return values and handle errors
int MiniMidiUi_kill( MiniMidiUi *self)
{
    delwin(self->info_sw);
    delwin(self->mode_sw);
    delwin(self->viewereditor_sw);
    delwin(self->cmds_sq);

    endwin();
    MiniMidi_File_free( self->file );
    free(self);

    

    return 0;
}

int MiniMidiUi_render_info( MiniMidiUi *self )
{
    // int retval = 0;

    wattron(self->info_sw, A_BOLD);
    if (mvwprintw(self->info_sw, 1, 0, "File: ") > 0)
        return 1;

    wattroff(self->info_sw, A_BOLD);
    
    wattron(self->info_sw, COLOR_PAIR(BLU_ON_WHITE));
    
    if (mvwprintw(self->info_sw, 1, 6, self->file->filepath ) > 0)
        return 1;
    
    wattroff(self->info_sw, COLOR_PAIR(BLU_ON_WHITE));

    size_t filepathlen = strlen( self->file->filepath  );

    if (mvwprintw(self->info_sw, 1, 6 + filepathlen, " - %li Bytes.", self->file->length ) > 0)
        return 1;

    return 0;
}

int MiniMidiUi_render_mode( MiniMidiUi *self)
{
    wattron(self->mode_sw, A_BOLD);
    // Edit Mode SubW
    // wattron(self->mode_sw, COLOR_PAIR(RED_ON_BLK));
    if (self->IS_EDIT_MODE)
    {
        // wattron(self->mode_sw, COLOR_PAIR(RED_ON_BLK));
        mvwprintw(self->mode_sw, 1, 0, "Write");
        // wattroff(self->mode_sw, COLOR_PAIR(RED_ON_BLK));
    } else
    {
        mvwprintw(self->mode_sw, 1, 0, "Read");
    }
    // wattroff(self->mode_sw, COLOR_PAIR(RED_ON_BLK));
    wattroff(self->mode_sw, A_BOLD);

    return 0; // TODO: error handling
}

void _make_horizontal_line(size_t size, chtype *str)
{
    // str[0] = '|';
    for (int i = 0; i < size; i++ )
    {
        str[i] = ( i % 2 ) ? '_' : ' ';
    }

    str[size] = '\0';
}

int MiniMidiUi_render_vieweditor( MiniMidiUi *self )
{
    chtype lini = '_' ;
    chtype _vert_line[2] = {'|', '\0'};
    chtype _vert_dot[2] = {'.', '\0'};
    chtype endo = '\0';
// Get the bounds of the subwindow
    int rows, cols;
    getmaxyx(self->viewereditor_sw, rows, cols);
    // this is not good here.
    char* all_notes[] = {"A", "Bb", "B", "C", "Db", "D", "Eb", "E", "F", "Gb", "G", "Ab" };
    short octave_range = 8;

    int note_counter = 0;
    int octave_counter = 0;

    chtype _liner[cols];
    _make_horizontal_line(cols - 11, _liner);

    // if we were to split the horizontal space into 4 divisions of a bar
    int _bar_len = (cols - 11) / 4;
    int _bar_leftover = (cols - 11) % 4;
    int _vert_split_counter = 0;

    for ( int i = rows - 5; i > 5; i--)
    {
        if ( i % 2 == 0 )
        {
            if (note_counter == 12) { note_counter = 0; octave_counter++; }
            if (octave_counter > octave_range){ octave_counter = 0; }

            mvwaddchstr(self->viewereditor_sw, i, 7, _liner );
            mvwprintw(self->viewereditor_sw, i,2, "%s%i", all_notes[note_counter++], octave_counter );   
        } else 
        {
            for (int j = 7; j < ( cols -7 ); j++)
            {
                if (!(j % _bar_len))
                {
                    mvwaddchstr(self->viewereditor_sw, i, j, _vert_dot );
                }
            }
        }
        mvwaddchstr(self->viewereditor_sw, i, 6, _vert_line );

        
    }

    return 0;
}
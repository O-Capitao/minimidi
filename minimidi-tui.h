#ifndef MINIMIDI_TUI_H
#define MINIMIDI_TUI_H

#include <ncurses.h>
#include <stdbool.h>
#include <assert.h>


#include "minimidi.h"


#define DEBUG 0

/***
*  * MiniMidi State:
* 
*   logical coords:  {beats, semitones}
*   terminal coords: {cols, lines}
*/
typedef struct MiniMidi_TUI
{
    int cols_in_beat,
        beats_in_bar,
        logical_size[2],    // a pair { n_beats, n_semitones }
        logical_start[2],   // logical coords
        grid_size[2],       // terminal coords
        outer_size[2];

    bool is_dirty,
        is_running;
    
    // opened midi file
    MiniMidi_File *file;
    
    // list with events that should be drawn to current grid
    MiniMidi_Event_List *midi_events_list;

    // derwin pointer -> Grid Area
    WINDOW *grid_derwin;

} MiniMidi_TUI;

/***
 *  "class" methods:
 */

// init all ncurses, sizes, load file, context
int MiniMidi_TUI_init( MiniMidi_TUI *self, MiniMidi_File *file );

// act upon result of user intput
//  returns:
//      running status
int MiniMidi_TUI_update( MiniMidi_TUI *self );

// put stuff in screen
int MiniMidi_TUI_render( MiniMidi_TUI *self );

// kill it
int MiniMidi_TUI_destroy( MiniMidi_TUI *self );

#endif /* MINIMIDI_TUI_H */
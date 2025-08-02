#ifndef MINIMIDI_TUI_H
#define MINIMIDI_TUI_H

#include <ncurses.h>
#include <stdbool.h>
#include <assert.h>

#include "minimidi.h"
#include "minimidi-log.h"

#define DEBUG 0

/***
*  * MiniMidi State:
* 
*   logical coords:  {beats, semitones}
*   terminal coords: {cols, lines}
*/
typedef struct MiniMidi_TUI
{
    int ticks_per_col,      // zoom lvl
        beats_in_bar,       // time sig
        logical_size[2],    // a pair { n_ticks, n_semitones }
        logical_start[2],   // logical coords
        grid_size[2],       // terminal coords
        outer_size[2],      // terminal size
        move_increment;     // how many ticks are moved by a press of <- or ->

    bool is_dirty,
        is_running,
        is_playing;

    int bpm;

    // playback
    int playback_time,
        playback_midi_ticks,
        playback_end_tick,
        fps,
        // playback_step_time_ms,
        playback_total_time_ms;

    int delta_ticks,
        delta_t_ms;

    int _cursor_position_ticks;

    // opened midi file
    MiniMidi_File *file;
    
    // list with events that should be drawn to current grid
    MiniMidi_Event_List *midi_events_list;

    // derwin pointer -> Grid Area
    WINDOW *grid_derwin;
    WINDOW *playback_derwin;

} MiniMidi_TUI;

/***
 *  "class" methods:
 */

// init all ncurses, sizes, load file, context
int MiniMidi_TUI_init( MiniMidi_TUI *self, MiniMidi_File *file );
int MiniMidi_TUI_step( MiniMidi_TUI *self );
int MiniMidi_TUI_destroy( MiniMidi_TUI *self );

#endif /* MINIMIDI_TUI_H */
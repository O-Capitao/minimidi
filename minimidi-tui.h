#ifndef MM_TUI_H
#define MM_TUI_H

#include <ncurses.h>
#include <stdbool.h>
#include <assert.h>

#include "minimidi.h"
#include "minimidi-log.h"
#include "minimidi-audio.h"

#define DEBUG 0
// 
#define CMD_BUFFER_SIZE 128

/***
*  * MiniMidi State:
* 
*   logical coords:  {beats, semitones}
*   terminal coords: {cols, lines}
*/
typedef struct MM_TUI
{
    int ticks_per_col,      // zoom lvl
        beats_in_bar,       // time sig
        logical_size[2],    // a pair { n_ticks, n_semitones }
        logical_start[2],   // logical coords
        grid_size[2],       // terminal coords
        outer_size[2],      // terminal size
        move_increment;     // how many ticks are moved by a press of <- or ->

    bool is_dirty,           // is state changed?
        is_running,          // is program running or has the user quit?
        is_playing,          // is playback and continuous scroll happening?
        is_render_requested; // if not playing, is it necessary to re-render?

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
    MM_File *file;
    
    // list with events that should be drawn to current grid
    MM_Event_List *midi_events_list;

    // buffer of midievents
    size_t evts_in_buffer;
    MM_Event evt_buffer[CMD_BUFFER_SIZE];

    // derwin pointer -> Grid Area
    WINDOW *grid_derwin;
    WINDOW *playback_derwin;

    // playback
    MM_Synth *synth;

} MM_TUI;

/***
 *  "class" methods:
 */

// init all ncurses, sizes, load file, context
int MM_TUI_init( MM_TUI *self, MM_File *file );
int MM_TUI_step( MM_TUI *self );
int MM_TUI_render(MM_TUI *self );
int MM_TUI_destroy( MM_TUI *self );

#endif /* MM_TUI_H */
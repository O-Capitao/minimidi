#ifndef MM_TUI_H
#define MM_TUI_H

#include <ncurses.h>
#include <stdbool.h>
#include <assert.h>
#include "minimidi.h"
#include "minimidi-log.h"
#include "minimidi-audio.h"


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

    unsigned int bpm,
        fps,
        cursor_position_ticks;

    double delta_t, playback_time, last_playback_time;

    // opened midi file
    MM_File *file;
    
    // list with events that should be drawn to current grid
    MM_Event_LList *midi_events_screen_list;

    // derwin pointer -> Grid Area
    WINDOW *grid_derwin;
    WINDOW *playback_derwin;

    // transport to audio thread
    MM_Ring_Buffer *cmd_queue;
    MM_AudioEngine *audio_engine;

} MM_TUI;

/***
 *  "class" methods:    
 */
int MM_TUI_init   ( MM_TUI *self, MM_File *file, MM_Ring_Buffer *cmd_queue, MM_AudioEngine *audio_engine, unsigned int bpm );
int MM_TUI_step   ( MM_TUI *self );
int MM_TUI_render ( MM_TUI *self );
int MM_TUI_destroy( MM_TUI *self );

#endif /* MM_TUI_H */
#ifndef MM_TUI_H
#define MM_TUI_H

#include <ncurses.h>
#include <panel.h>
#include <stdbool.h>
#include <assert.h>
#include "minimidi-proj.h"
#include "minimidi.h"
#include "minimidi-log.h"
#include "minimidi-audio.h"

/**
 * @brief A Rectangle in the terminal.
 * 
 */
typedef struct {
    size_t lines, cols;
} MM_TUI_TerminalCoords;

typedef struct {
    size_t ticks;
    size_t semitones;
} MM_TUI_Track_LogicalCoords;

/**
 * @brief structure holding editable data for drawing the track viewer.
 */
typedef struct {
    
    WINDOW *win;

    bool is_dirty;
    bool is_playing;

    MM_Midi_File *file;
    MM_Track *track;
    MM_MidiEvent_LList *midi_events_screen_list;
    MM_MidiEvent_LList *midi_events_audio_list;

    MM_TUI_TerminalCoords box_size,
        box_position;
    MM_TUI_Track_LogicalCoords logical_box_size,
        logical_box_position;

    unsigned int ticks_per_col,
        move_increment,
        cursor_position_ticks;

} MM_TUI_TrackComponent;

/**
 * @brief structure holding data defining the general info header
 *          - project name
 *          - PPQN
 *          - 
 */
typedef struct {
    
    WINDOW *win;

    bool is_dirty;
    MM_Project *project;
    MM_TUI_TerminalCoords box_size, box_position;
} MM_TUI_GeneralInfoComponent;

/**
 * @brief Readonly TUI Component showing Track properties.
 * 
 */
typedef struct {
    
    WINDOW *win;
    PANEL *pnl;

    MM_Track *track;
    MM_TUI_TerminalCoords box_size,
        box_position;

    bool is_shown;
} MM_TUI_TrackPropertiesComponent;

/**
 * @brief Readonly TUI Component showing time / scrubber.
 * 
 */
typedef struct {

    WINDOW *win;

    bool is_playing;
    MM_Project *project;
    MM_Sequence *active_sequence;
    MM_TUI_TerminalCoords box_size, box_position;
} MM_TUI_TimeInfoComponent;


/**
 * @brief MiniMidi TUI component
 *
 */
typedef struct
{
    MM_Project *project;

    MM_TUI_TerminalCoords box;

    MM_TUI_TrackComponent track_component;
    MM_TUI_GeneralInfoComponent general_info_component;
    MM_TUI_TrackPropertiesComponent track_properties_component;
    MM_TUI_TimeInfoComponent time_info_component;

    bool is_dirty,           // is state changed?
        is_running,          // is program running or has the user quit?
        is_playing,          // is playback and continuous scroll happening?
        is_render_requested; // if not playing, is it necessary to re-render?

    unsigned int bpm,
        fps;

    double delta_t, playback_time, last_playback_time;

    // transport to audio thread
    MM_Ring_Buffer *cmd_queue;
    MM_AudioEngine *audio_engine;

} MM_TUI;

int MM_TUI_init   ( MM_TUI *self, MM_Project *project, MM_Ring_Buffer *cmd_queue, MM_AudioEngine *audio_engine);
int MM_TUI_step   ( MM_TUI *self );
int MM_TUI_destroy( MM_TUI *self );

#endif /* MM_TUI_H */
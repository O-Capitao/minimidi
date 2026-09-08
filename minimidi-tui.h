#ifndef MM_TUI_H
#define MM_TUI_H

#include "minimidi-audio.h"
#include "minimidi-proj.h"
#include "minimidi-rb.h"

#include <ncurses.h>
#include <panel.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct MM_TUI {
    MM_Project *project;
    MM_Ring_Buffer *cmd_queue;
    MM_AudioEngine *audio_engine;
    WINDOW *grid_derwin;
    WINDOW *playback_derwin;
    WINDOW *modal_win;
    PANEL *modal_panel;
    size_t selected_track_index;
    uint64_t logical_start_tick;
    uint64_t logical_width_ticks;
    unsigned int ticks_per_col;
    unsigned int beats_in_bar;
    unsigned int fps;
    int logical_start_note;
    int visible_notes;
    int rows;
    int cols;
    bool is_running;
    bool is_playing;
    bool modal_shown;
    bool curses_initialized;
} MM_TUI;

int MM_TUI_init(MM_TUI *tui, MM_Project *project,
                MM_Ring_Buffer *cmd_queue, MM_AudioEngine *audio_engine);
int MM_TUI_step(MM_TUI *tui);
int MM_TUI_render(MM_TUI *tui);
int MM_TUI_destroy(MM_TUI *tui);

#endif

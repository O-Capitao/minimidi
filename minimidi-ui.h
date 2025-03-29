#ifndef MINIMIDI_UI_H
#define MINIMIDI_UI_H

#include <ncurses.h>

#include "minimidi.h"
#include "globals.h"

/***
 * AvaibleCommands Window:
 * 
 */
typedef enum
{
    GO_TO_EDIT_MODE = 0,
    GO_TO_VIEW_MODE = 1,
    OCT_UP          = 2,
    OCT_DOWN        = 3
} Command;

typedef struct MiniMidiUi
{
    WINDOW *info_sw, *mode_sw, *viewereditor_sw, *cmds_sq;
    MiniMidi_File *file;
    
    // some flags
    bool IS_EDIT_MODE;

} MiniMidiUi;

MiniMidiUi *MiniMidiUi_init();
int MiniMidiUi_run( MiniMidiUi *self );
int MiniMidiUi_kill( MiniMidiUi *self );

int MiniMidiUi_load_file( MiniMidiUi *self, char *filepath );

int MiniMidiUi_render_info( MiniMidiUi *self );
int MiniMidiUi_render_mode( MiniMidiUi *self );
int MiniMidiUi_render_vieweditor( MiniMidiUi *self );

#endif
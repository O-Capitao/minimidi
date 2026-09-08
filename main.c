#include "minimidi-audio.h"
#include "minimidi-log.h"
#include "minimidi-proj.h"
#include "minimidi-rb.h"
#include "minimidi-tui.h"
#include "minimidi-transport.h"

#include <stdio.h>

int main(int argc, char **argv)
{
    MM_Project project;
    MM_AudioEngine audio;
    MM_TUI tui;
    MM_Ring_Buffer *commands = NULL;
    int project_ready = 0;
    int audio_ready = 0;
    int tui_ready = 0;
    int result = 1;

    if (argc != 2) {
        fprintf(stderr, "usage: %s PROJECT.yaml\n", argv[0]);
        return 2;
    }
    if (log_init("minimidi.log") != 0) {
        fprintf(stderr, "cannot initialize minimidi.log\n");
        return 1;
    }
    if (MM_Project_init(&project, argv[1]) != 0) {
        fprintf(stderr, "cannot load project '%s'\n", argv[1]);
        goto cleanup;
    }
    project_ready = 1;
    commands = MM_Ring_Buffer__init(64, sizeof(MM_AudioCommand));
    if (!commands) {
        fprintf(stderr, "cannot allocate audio command queue\n");
        goto cleanup;
    }
    if (MM_AudioEngine_init(&audio, &project, commands) != 0) {
        fprintf(stderr, "cannot initialize audio output\n");
        goto cleanup;
    }
    audio_ready = 1;
    if (MM_TUI_init(&tui, &project, commands, &audio) != 0) {
        fprintf(stderr, "cannot initialize terminal UI\n");
        goto cleanup;
    }
    tui_ready = 1;
    result = 0;
    while (tui.is_running) {
        if (MM_TUI_step(&tui) != 0) {
            result = 1;
            break;
        }
    }

cleanup:
    if (audio_ready && MM_AudioEngine_destroy(&audio) != 0) result = 1;
    if (tui_ready) MM_TUI_destroy(&tui);
    MM_Ring_Buffer__free(commands);
    if (project_ready) MM_Project_free(&project);
    log_deinit();
    return result;
}

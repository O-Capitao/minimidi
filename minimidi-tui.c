#include "minimidi-tui.h"

#include "minimidi-log.h"

#include <inttypes.h>
#include <stdatomic.h>
#include <stdio.h>
#include <string.h>

#define HEADER_HEIGHT 3
#define STATUS_HEIGHT 3
#define LABEL_WIDTH 6
#define MIN_ROWS 12
#define MIN_COLS 40

enum {
    MM_COLOR_GRID = 1,
    MM_COLOR_NOTE,
    MM_COLOR_CURSOR,
    MM_COLOR_SELECTED
};

static uint64_t scale_tick(uint64_t tick, unsigned int source_ppqn,
                           unsigned int project_ppqn)
{
    uint64_t whole = tick / source_ppqn;
    uint64_t remainder = tick % source_ppqn;
    return whole * project_ppqn
         + (remainder * project_ppqn + source_ppqn / 2) / source_ppqn;
}

static void destroy_windows(MM_TUI *tui)
{
    if (tui->modal_panel) {
        del_panel(tui->modal_panel);
        tui->modal_panel = NULL;
    }
    if (tui->modal_win) { delwin(tui->modal_win); tui->modal_win = NULL; }
    if (tui->status_win) { delwin(tui->status_win); tui->status_win = NULL; }
    if (tui->track_win) { delwin(tui->track_win); tui->track_win = NULL; }
    if (tui->header_win) { delwin(tui->header_win); tui->header_win = NULL; }
}

static int create_windows(MM_TUI *tui)
{
    int track_height;
    int modal_height = 7;
    int modal_width = 34;
    destroy_windows(tui);
    getmaxyx(stdscr, tui->rows, tui->cols);
    if (tui->rows < MIN_ROWS || tui->cols < MIN_COLS) return 0;
    track_height = tui->rows - HEADER_HEIGHT - STATUS_HEIGHT;
    tui->header_win = derwin(stdscr, HEADER_HEIGHT, tui->cols, 0, 0);
    tui->track_win = derwin(stdscr, track_height, tui->cols, HEADER_HEIGHT, 0);
    tui->status_win = derwin(stdscr, STATUS_HEIGHT, tui->cols,
                             HEADER_HEIGHT + track_height, 0);
    if (modal_width > tui->cols - 4) modal_width = tui->cols - 4;
    tui->modal_win = newwin(modal_height, modal_width,
                            (tui->rows - modal_height) / 2,
                            (tui->cols - modal_width) / 2);
    if (!tui->header_win || !tui->track_win || !tui->status_win || !tui->modal_win)
        return -1;
    tui->modal_panel = new_panel(tui->modal_win);
    if (!tui->modal_panel) return -1;
    if (!tui->modal_shown) hide_panel(tui->modal_panel);
    return 0;
}

static void push_command(MM_TUI *tui, CommandType type)
{
    MM_AudioCommand command;
    command.cmd_type = type;
    if (!MM_Ring_Buffer__push(tui->cmd_queue, &command)) beep();
}

static void handle_input(MM_TUI *tui)
{
    int key = getch();
    uint64_t bar = (uint64_t)tui->project->ppqn * tui->project->file->beat_per_bar;
    if (key == ERR) return;
    if (key >= '1' && key <= '4') {
        size_t index = (size_t)(key - '1');
        if (index < tui->project->n_tracks) tui->selected_track_index = index;
        return;
    }
    switch (key) {
        case 'q': case 'Q': tui->is_running = false; break;
        case ' ':
            tui->is_playing = !atomic_load_explicit(&tui->audio_engine->posted_playing,
                                                     memory_order_relaxed);
            push_command(tui, tui->is_playing ? MM_CMD_PLAY : MM_CMD_PAUSE);
            break;
        case 's': case 'S':
            tui->is_playing = false;
            push_command(tui, MM_CMD_STOP);
            break;
        case KEY_HOME:
            tui->view_start_tick = 0;
            push_command(tui, MM_CMD_BACK_TO_BEGINNING);
            break;
        case KEY_LEFT:
            tui->view_start_tick = tui->view_start_tick > bar
                ? tui->view_start_tick - bar : 0;
            break;
        case KEY_RIGHT:
            tui->view_start_tick += bar;
            break;
        case KEY_UP:
            if (tui->lowest_note < 127) tui->lowest_note++;
            break;
        case KEY_DOWN:
            if (tui->lowest_note > 0) tui->lowest_note--;
            break;
        case '+':
            if (tui->ticks_per_col > 1) tui->ticks_per_col /= 2;
            if (tui->ticks_per_col == 0) tui->ticks_per_col = 1;
            break;
        case '-':
            if (tui->ticks_per_col <= UINT32_MAX / 2) tui->ticks_per_col *= 2;
            break;
        case 't': case 'T':
            tui->modal_shown = !tui->modal_shown;
            if (tui->modal_panel) {
                if (tui->modal_shown) show_panel(tui->modal_panel);
                else hide_panel(tui->modal_panel);
            }
            break;
        case KEY_RESIZE:
            create_windows(tui);
            break;
        default: break;
    }
}

static int note_row(int note, int lowest, int height)
{
    int visible = height - 2;
    int offset = note - lowest;
    return offset >= 0 && offset < visible ? height - 2 - offset : -1;
}

static void render_grid(MM_TUI *tui, MM_Sequence *sequence,
                        MM_SequenceTrack *track)
{
    int height, width, row, col;
    uint64_t view_end;
    uint64_t tick;
    size_t clip_index;
    getmaxyx(tui->track_win, height, width);
    werase(tui->track_win);
    box(tui->track_win, 0, 0);
    mvwprintw(tui->track_win, 0, 2, " Track %zu: %s / sequence: %s ",
              tui->selected_track_index + 1,
              tui->project->tracks_arr[tui->selected_track_index].name,
              sequence->name);
    if (width <= LABEL_WIDTH + 2 || height <= 2) return;
    view_end = tui->view_start_tick
             + (uint64_t)(width - LABEL_WIDTH - 1) * tui->ticks_per_col;

    for (row = 1; row < height - 1; row++) {
        int note = tui->lowest_note + (height - 2 - row);
        if (note <= 127) mvwprintw(tui->track_win, row, 1, "%3d", note);
    }
    wattron(tui->track_win, COLOR_PAIR(MM_COLOR_GRID));
    for (tick = (tui->view_start_tick / tui->project->ppqn) * tui->project->ppqn;
         tick <= view_end; tick += tui->project->ppqn) {
        if (tick < tui->view_start_tick) continue;
        col = LABEL_WIDTH + (int)((tick - tui->view_start_tick) / tui->ticks_per_col);
        if (col >= width - 1) break;
        for (row = 1; row < height - 1; row++) mvwaddch(tui->track_win, row, col, '.');
    }
    wattroff(tui->track_win, COLOR_PAIR(MM_COLOR_GRID));

    wattron(tui->track_win, COLOR_PAIR(MM_COLOR_NOTE));
    for (clip_index = 0; clip_index < track->n_clips; clip_index++) {
        const MM_Clip *clip = &track->clips[clip_index];
        uint64_t cycle_start;
        cycle_start = clip->start_tick;
        if (clip->loop && tui->view_start_tick > cycle_start)
            cycle_start += ((tui->view_start_tick - cycle_start)
                         / clip->source_duration_ticks) * clip->source_duration_ticks;
        while (cycle_start < clip->end_tick && cycle_start < view_end) {
            size_t event_index;
            for (event_index = 0; event_index < clip->midi->track.n_events; event_index++) {
                MM_MidiEvent *event = &clip->midi->track.event_arr[event_index];
                uint64_t start, end;
                int start_col, end_col;
                if (event->status_code != MIDI_NOTE_ON || !event->next) continue;
                start = cycle_start + scale_tick(event->abs_ticks,
                    clip->midi->header.ppqn, tui->project->ppqn);
                end = cycle_start + scale_tick(event->next->abs_ticks,
                    clip->midi->header.ppqn, tui->project->ppqn);
                if (start >= clip->end_tick || start >= view_end || end < tui->view_start_tick) continue;
                if (end > clip->end_tick) end = clip->end_tick;
                row = note_row(event->note_number, tui->lowest_note, height);
                if (row < 1) continue;
                start_col = start <= tui->view_start_tick ? LABEL_WIDTH
                    : LABEL_WIDTH + (int)((start - tui->view_start_tick) / tui->ticks_per_col);
                end_col = end >= view_end ? width - 2
                    : LABEL_WIDTH + (int)((end - tui->view_start_tick) / tui->ticks_per_col);
                if (end_col < start_col) end_col = start_col;
                for (col = start_col; col <= end_col && col < width - 1; col++)
                    mvwaddch(tui->track_win, row, col, ' ' | A_REVERSE);
            }
            if (!clip->loop || clip->source_duration_ticks >= clip->end_tick - cycle_start)
                break;
            cycle_start += clip->source_duration_ticks;
        }
    }
    wattroff(tui->track_win, COLOR_PAIR(MM_COLOR_NOTE));

    tick = atomic_load_explicit(&tui->audio_engine->posted_sequence_tick,
                                memory_order_relaxed);
    if (tick >= tui->view_start_tick && tick < view_end) {
        col = LABEL_WIDTH + (int)((tick - tui->view_start_tick) / tui->ticks_per_col);
        wattron(tui->track_win, COLOR_PAIR(MM_COLOR_CURSOR));
        for (row = 1; row < height - 1; row++) mvwaddch(tui->track_win, row, col, '|');
        wattroff(tui->track_win, COLOR_PAIR(MM_COLOR_CURSOR));
    }
}

static void render_modal(MM_TUI *tui)
{
    MM_Track *track = &tui->project->tracks_arr[tui->selected_track_index];
    if (!tui->modal_shown) return;
    werase(tui->modal_win);
    box(tui->modal_win, 0, 0);
    mvwprintw(tui->modal_win, 0, 2, " Track properties ");
    mvwprintw(tui->modal_win, 2, 2, "number:   %zu", tui->selected_track_index + 1);
    mvwprintw(tui->modal_win, 3, 2, "name:     %s", track->name);
    mvwprintw(tui->modal_win, 4, 2, "waveform: %s", mm_wave_to_str(track->wave));
    mvwprintw(tui->modal_win, 5, 2, "gain:     %.2f", track->gain);
}

static int render(MM_TUI *tui)
{
    size_t sequence_index;
    MM_Sequence *sequence;
    double time;
    uint64_t sequence_tick;
    if (tui->rows < MIN_ROWS || tui->cols < MIN_COLS) {
        erase();
        mvprintw(0, 0, "Terminal too small (minimum %dx%d)", MIN_COLS, MIN_ROWS);
        refresh();
        return 0;
    }
    sequence_index = atomic_load_explicit(&tui->audio_engine->posted_sequence_index,
                                          memory_order_relaxed);
    if (sequence_index >= tui->project->n_sequences) sequence_index = 0;
    sequence = &tui->project->sequence_arr[sequence_index];
    tui->is_playing = atomic_load_explicit(&tui->audio_engine->posted_playing,
                                           memory_order_relaxed);
    time = atomic_load_explicit(&tui->audio_engine->posted_audio_time,
                                memory_order_relaxed);
    sequence_tick = atomic_load_explicit(&tui->audio_engine->posted_sequence_tick,
                                         memory_order_relaxed);
    if (tui->is_playing) {
        int track_height, track_width;
        uint64_t visible_ticks;
        getmaxyx(tui->track_win, track_height, track_width);
        (void)track_height;
        visible_ticks = track_width > LABEL_WIDTH + 1
            ? (uint64_t)(track_width - LABEL_WIDTH - 1) * tui->ticks_per_col : 0;
        if (visible_ticks && (sequence_tick < tui->view_start_tick
            || sequence_tick >= tui->view_start_tick + visible_ticks))
            tui->view_start_tick = sequence_tick > visible_ticks / 2
                ? sequence_tick - visible_ticks / 2 : 0;
    }

    werase(tui->header_win);
    box(tui->header_win, 0, 0);
    mvwprintw(tui->header_win, 1, 2,
              "%s | %u bpm | %u PPQN | %u Hz | tracks 1-%zu",
              tui->project->file->name, tui->project->file->tempo,
              tui->project->ppqn, tui->project->file->sample_rate,
              tui->project->n_tracks);
    render_grid(tui, sequence, &sequence->tracks[tui->selected_track_index]);
    werase(tui->status_win);
    box(tui->status_win, 0, 0);
    mvwprintw(tui->status_win, 1, 2,
              "%s  time %.2fs  section %zu/%zu '%s'  tick %" PRIu64
              "  [space] play/pause [s] stop [t] track info [q] quit",
              tui->is_playing ? "PLAY" : "STOP", time, sequence_index + 1,
              tui->project->n_sequences, sequence->name, sequence_tick);
    render_modal(tui);
    wnoutrefresh(tui->header_win);
    wnoutrefresh(tui->track_win);
    wnoutrefresh(tui->status_win);
    update_panels();
    doupdate();
    return 0;
}

int MM_TUI_init(MM_TUI *tui, MM_Project *project,
                MM_Ring_Buffer *cmd_queue, MM_AudioEngine *audio_engine)
{
    if (!tui || !project || !cmd_queue || !audio_engine || project->n_tracks == 0)
        return -1;
    memset(tui, 0, sizeof(*tui));
    tui->project = project;
    tui->cmd_queue = cmd_queue;
    tui->audio_engine = audio_engine;
    tui->ticks_per_col = project->ppqn / 4;
    if (tui->ticks_per_col == 0) tui->ticks_per_col = 1;
    tui->lowest_note = 36;
    tui->is_running = true;
    if (!initscr()) return -1;
    tui->curses_initialized = true;
    raw();
    noecho();
    keypad(stdscr, TRUE);
    nodelay(stdscr, TRUE);
    curs_set(0);
    if (has_colors()) {
        start_color();
        use_default_colors();
        init_pair(MM_COLOR_GRID, COLOR_BLUE, -1);
        init_pair(MM_COLOR_NOTE, COLOR_CYAN, -1);
        init_pair(MM_COLOR_CURSOR, COLOR_GREEN, -1);
        init_pair(MM_COLOR_SELECTED, COLOR_BLACK, COLOR_CYAN);
    }
    if (create_windows(tui) != 0) {
        MM_TUI_destroy(tui);
        return -1;
    }
    return 0;
}

int MM_TUI_step(MM_TUI *tui)
{
    int rows, cols;
    if (!tui || !tui->curses_initialized) return -1;
    getmaxyx(stdscr, rows, cols);
    if (rows != tui->rows || cols != tui->cols) {
        if (create_windows(tui) != 0) return -1;
    }
    handle_input(tui);
    if (render(tui) != 0) return -1;
    napms(16);
    return 0;
}

int MM_TUI_destroy(MM_TUI *tui)
{
    if (!tui) return -1;
    destroy_windows(tui);
    if (tui->curses_initialized) {
        endwin();
        tui->curses_initialized = false;
    }
    return 0;
}

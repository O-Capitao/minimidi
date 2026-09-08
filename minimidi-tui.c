#include "minimidi-tui.h"

#include "minimidi-log.h"

#include <inttypes.h>
#include <stdatomic.h>
#include <stdio.h>
#include <string.h>

#define TOP_BAR_HEIGHT 1
#define BOTT_BAR_HEIGHT 1
#define TOP_RIGHT_WIDTH 10
#define GRID_LEFT_LABELS_WIDTH 7
#define LINES_PER_SEMITONE 2
#define MIN_ROWS 12
#define MIN_COLS 40

static const char *ALL_NOTES[] = {
    "C", "Db", "D", "Eb", "E", "F", "Gb", "G", "Ab", "A", "Bb", "B"
};

enum {
    RED_ON_BLK = 1,
    GREEN_ON_BLK,
    BLACK_ON_CYAN,
    BLACK_ON_GREEN
};

static uint64_t scale_tick(uint64_t tick, unsigned int source_ppqn,
                           unsigned int project_ppqn)
{
    uint64_t whole = tick / source_ppqn;
    uint64_t remainder = tick % source_ppqn;
    return whole * project_ppqn
         + (remainder * project_ppqn + source_ppqn / 2) / source_ppqn;
}

static uint64_t active_sequence_tick(const MM_TUI *tui)
{
    return atomic_load_explicit(&tui->audio_engine->posted_sequence_tick,
                                memory_order_relaxed);
}

static size_t active_sequence_index(const MM_TUI *tui)
{
    size_t index = atomic_load_explicit(&tui->audio_engine->posted_sequence_index,
                                        memory_order_relaxed);
    return index < tui->project->n_sequences ? index : 0;
}

static void destroy_windows(MM_TUI *tui)
{
    if (tui->modal_panel) {
        del_panel(tui->modal_panel);
        tui->modal_panel = NULL;
    }
    if (tui->modal_win) {
        delwin(tui->modal_win);
        tui->modal_win = NULL;
    }
    if (tui->playback_derwin) {
        delwin(tui->playback_derwin);
        tui->playback_derwin = NULL;
    }
    if (tui->grid_derwin) {
        delwin(tui->grid_derwin);
        tui->grid_derwin = NULL;
    }
}

static void update_sizes(MM_TUI *tui)
{
    int grid_rows;
    int grid_cols;
    uint64_t cursor;

    if (!tui->grid_derwin) return;
    getmaxyx(tui->grid_derwin, grid_rows, grid_cols);
    tui->visible_notes = (grid_rows - 2) / LINES_PER_SEMITONE;
    if (tui->visible_notes < 1) tui->visible_notes = 1;
    tui->logical_width_ticks = grid_cols > GRID_LEFT_LABELS_WIDTH + 1
        ? (uint64_t)(grid_cols - GRID_LEFT_LABELS_WIDTH - 1) * tui->ticks_per_col
        : 0;

    if (!tui->is_playing || tui->logical_width_ticks == 0) return;
    cursor = active_sequence_tick(tui);
    tui->logical_start_tick = cursor > tui->logical_width_ticks / 2
        ? cursor - tui->logical_width_ticks / 2 : 0;
}

static int create_windows(MM_TUI *tui)
{
    int modal_width = 34;

    destroy_windows(tui);
    getmaxyx(stdscr, tui->rows, tui->cols);
    if (tui->rows < MIN_ROWS || tui->cols < MIN_COLS) return 0;

    tui->grid_derwin = derwin(stdscr,
        tui->rows - TOP_BAR_HEIGHT - BOTT_BAR_HEIGHT,
        tui->cols, TOP_BAR_HEIGHT, 0);
    tui->playback_derwin = derwin(stdscr, 4, 25, 5, 10);
    if (modal_width > tui->cols - 4) modal_width = tui->cols - 4;
    tui->modal_win = newwin(7, modal_width, (tui->rows - 7) / 2,
                            (tui->cols - modal_width) / 2);
    if (!tui->grid_derwin || !tui->playback_derwin || !tui->modal_win)
        return -1;
    tui->modal_panel = new_panel(tui->modal_win);
    if (!tui->modal_panel) return -1;
    if (!tui->modal_shown) hide_panel(tui->modal_panel);
    update_sizes(tui);
    return 0;
}

static void push_command(MM_TUI *tui, CommandType type)
{
    MM_AudioCommand command;
    command.cmd_type = type;
    if (!MM_Ring_Buffer__push(tui->cmd_queue, &command)) beep();
}

static int note_row(const MM_TUI *tui, int note)
{
    int grid_rows;
    int grid_cols;
    int offset = note - tui->logical_start_note;

    if (offset < 0 || offset >= tui->visible_notes) return -1;
    getmaxyx(tui->grid_derwin, grid_rows, grid_cols);
    (void)grid_cols;
    return grid_rows - 2 - offset * LINES_PER_SEMITONE;
}

static void snap_to_first_event(MM_TUI *tui)
{
    MM_Sequence *sequence;
    MM_SequenceTrack *track;
    uint64_t first_tick = UINT64_MAX;
    int first_note = -1;
    size_t clip_index;

    if (!tui->project->n_sequences || tui->selected_track_index >= tui->project->n_tracks)
        return;
    sequence = &tui->project->sequence_arr[active_sequence_index(tui)];
    track = &sequence->tracks[tui->selected_track_index];
    for (clip_index = 0; clip_index < track->n_clips; clip_index++) {
        const MM_Clip *clip = &track->clips[clip_index];
        size_t event_index;
        for (event_index = 0; event_index < clip->midi->track.n_events; event_index++) {
            const MM_MidiEvent *event = &clip->midi->track.event_arr[event_index];
            uint64_t tick;
            if (event->status_code != MIDI_NOTE_ON) continue;
            tick = clip->start_tick + scale_tick(event->abs_ticks,
                clip->midi->header.ppqn, tui->project->ppqn);
            if (tick >= clip->end_tick) continue;
            if (tick < first_tick) {
                first_tick = tick;
                first_note = event->note_number;
            }
        }
    }
    if (first_note < 0) return;
    tui->logical_start_tick = first_tick
        / ((uint64_t)tui->project->ppqn * tui->beats_in_bar)
        * ((uint64_t)tui->project->ppqn * tui->beats_in_bar);
    tui->logical_start_note = first_note > tui->visible_notes / 2
        ? first_note - tui->visible_notes / 2 : 0;
}

static void handle_input(MM_TUI *tui)
{
    int key = getch();
    uint64_t bar = (uint64_t)tui->project->ppqn * tui->beats_in_bar;

    if (key == ERR) return;
    if (key >= '1' && key <= '4') {
        size_t index = (size_t)(key - '1');
        if (index < tui->project->n_tracks && index != tui->selected_track_index) {
            tui->selected_track_index = index;
            snap_to_first_event(tui);
        }
        return;
    }
    switch (key) {
        case 'q': case 'Q':
            tui->is_running = false;
            break;
        case ' ':
            tui->is_playing = !atomic_load_explicit(
                &tui->audio_engine->posted_playing, memory_order_relaxed);
            push_command(tui, tui->is_playing ? MM_CMD_PLAY : MM_CMD_PAUSE);
            break;
        case 's': case 'S':
            tui->is_playing = false;
            tui->logical_start_tick = 0;
            push_command(tui, MM_CMD_STOP);
            break;
        case KEY_HOME:
            tui->logical_start_tick = 0;
            push_command(tui, MM_CMD_BACK_TO_BEGINNING);
            break;
        case KEY_LEFT:
            tui->logical_start_tick = tui->logical_start_tick > bar
                ? tui->logical_start_tick - bar : 0;
            break;
        case KEY_RIGHT:
            tui->logical_start_tick += bar;
            break;
        case KEY_UP:
            if (tui->logical_start_note < 127) tui->logical_start_note++;
            break;
        case KEY_DOWN:
            if (tui->logical_start_note > 0) tui->logical_start_note--;
            break;
        case '+':
            if (tui->ticks_per_col > 1) tui->ticks_per_col /= 2;
            if (tui->ticks_per_col == 0) tui->ticks_per_col = 1;
            update_sizes(tui);
            break;
        case '-':
            if (tui->ticks_per_col <= UINT32_MAX / 2) tui->ticks_per_col *= 2;
            update_sizes(tui);
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
        default:
            break;
    }
}

static void render_info(MM_TUI *tui, const MM_Sequence *sequence)
{
    char info[512];
    const MM_Track *track = &tui->project->tracks_arr[tui->selected_track_index];
    const char *state = tui->is_playing ? "-PLAYING-" : "-STOPPED-";
    int available = tui->cols - TOP_RIGHT_WIDTH;

    snprintf(info, sizeof(info),
             "project: %s . track: %zu %s . sequence: %s.",
             tui->project->file->name, tui->selected_track_index + 1,
             track->name, sequence->name);
    if (available > 0) mvaddnstr(0, 0, info, available);
    if (tui->cols >= TOP_RIGHT_WIDTH)
        mvaddnstr(0, tui->cols - TOP_RIGHT_WIDTH, state, TOP_RIGHT_WIDTH);
}

static void render_note_labels(MM_TUI *tui)
{
    int offset;
    for (offset = 0; offset < tui->visible_notes; offset++) {
        int note = tui->logical_start_note + offset;
        int row;
        if (note > 127) break;
        row = note_row(tui, note);
        if (row > 0) {
            mvwprintw(tui->grid_derwin, row, 3, "%s", ALL_NOTES[note % 12]);
            mvwprintw(tui->grid_derwin, row, 5, "%d", note / 12 - 1);
        }
    }
}

static void render_grid(MM_TUI *tui)
{
    int grid_cols;
    int offset;
    int col;
    uint64_t view_end;
    uint64_t bar_ticks = (uint64_t)tui->project->ppqn * tui->beats_in_bar;
    uint64_t first_bar;
    uint64_t bar_tick;

    grid_cols = getmaxx(tui->grid_derwin);
    view_end = tui->logical_start_tick + tui->logical_width_ticks;
    for (offset = 0; offset < tui->visible_notes; offset++) {
        int note = tui->logical_start_note + offset;
        int row;
        if (note > 127) break;
        row = note_row(tui, note);
        for (col = GRID_LEFT_LABELS_WIDTH; col < grid_cols - 1; col++) {
            uint64_t tick = tui->logical_start_tick
                          + (uint64_t)(col - GRID_LEFT_LABELS_WIDTH)
                          * tui->ticks_per_col;
            if ((tick / tui->project->ppqn) % 2 == 0)
                mvwaddch(tui->grid_derwin, row, col, '_');
        }
    }

    first_bar = tui->logical_start_tick / bar_ticks * bar_ticks;
    if (first_bar < tui->logical_start_tick) first_bar += bar_ticks;
    for (bar_tick = first_bar; bar_tick <= view_end; bar_tick += bar_ticks) {
        uint64_t bar_number = bar_tick / bar_ticks + 1;
        col = GRID_LEFT_LABELS_WIDTH
            + (int)((bar_tick - tui->logical_start_tick) / tui->ticks_per_col);
        if (col >= grid_cols - 1) break;
        wattron(tui->grid_derwin, COLOR_PAIR(GREEN_ON_BLK));
        for (offset = 0; offset < tui->visible_notes; offset++) {
            int row = note_row(tui, tui->logical_start_note + offset) - 1;
            if (row > 0) mvwaddch(tui->grid_derwin, row, col, '\'');
        }
        if (col + 2 < grid_cols - 6)
            mvwprintw(tui->grid_derwin, 1, col + 2, "BAR%" PRIu64, bar_number);
        wattroff(tui->grid_derwin, COLOR_PAIR(GREEN_ON_BLK));
        if (UINT64_MAX - bar_tick < bar_ticks) break;
    }
}

static void render_midi(MM_TUI *tui, const MM_SequenceTrack *track)
{
    int grid_rows;
    int grid_cols;
    uint64_t view_end;
    size_t clip_index;

    getmaxyx(tui->grid_derwin, grid_rows, grid_cols);
    (void)grid_rows;
    view_end = tui->logical_start_tick + tui->logical_width_ticks;
    for (clip_index = 0; clip_index < track->n_clips; clip_index++) {
        const MM_Clip *clip = &track->clips[clip_index];
        uint64_t cycle_start = clip->start_tick;
        if (clip->loop && tui->logical_start_tick > cycle_start)
            cycle_start += ((tui->logical_start_tick - cycle_start)
                         / clip->source_duration_ticks) * clip->source_duration_ticks;
        while (cycle_start < clip->end_tick && cycle_start < view_end) {
            size_t event_index;
            for (event_index = 0; event_index < clip->midi->track.n_events; event_index++) {
                const MM_MidiEvent *event = &clip->midi->track.event_arr[event_index];
                uint64_t start;
                uint64_t end;
                int row;
                int start_col;
                int end_col;
                int col;
                if (event->status_code != MIDI_NOTE_ON || !event->next) continue;
                start = cycle_start + scale_tick(event->abs_ticks,
                    clip->midi->header.ppqn, tui->project->ppqn);
                end = cycle_start + scale_tick(event->next->abs_ticks,
                    clip->midi->header.ppqn, tui->project->ppqn);
                if (start >= clip->end_tick || start >= view_end
                    || end < tui->logical_start_tick) continue;
                if (end > clip->end_tick) end = clip->end_tick;
                row = note_row(tui, event->note_number);
                if (row <= 0) continue;
                start_col = start <= tui->logical_start_tick
                    ? GRID_LEFT_LABELS_WIDTH
                    : GRID_LEFT_LABELS_WIDTH
                    + (int)((start - tui->logical_start_tick) / tui->ticks_per_col);
                end_col = end >= view_end ? grid_cols - 2
                    : GRID_LEFT_LABELS_WIDTH
                    + (int)((end - tui->logical_start_tick) / tui->ticks_per_col);
                if (start_col >= grid_cols - 1 || end_col < GRID_LEFT_LABELS_WIDTH)
                    continue;
                if (end_col < start_col) end_col = start_col;
                wattron(tui->grid_derwin, COLOR_PAIR(BLACK_ON_CYAN));
                mvwaddch(tui->grid_derwin, row, start_col, ' ');
                wattroff(tui->grid_derwin, COLOR_PAIR(BLACK_ON_CYAN));
                wattron(tui->grid_derwin, COLOR_PAIR(BLACK_ON_GREEN));
                for (col = start_col + 1; col <= end_col && col < grid_cols - 1; col++)
                    mvwaddch(tui->grid_derwin, row, col, ' ');
                wattroff(tui->grid_derwin, COLOR_PAIR(BLACK_ON_GREEN));
            }
            if (!clip->loop
                || clip->source_duration_ticks >= clip->end_tick - cycle_start)
                break;
            cycle_start += clip->source_duration_ticks;
        }
    }
}

static void render_cursor(MM_TUI *tui)
{
    int grid_rows;
    int grid_cols;
    uint64_t tick = active_sequence_tick(tui);
    int col;

    if (tick < tui->logical_start_tick
        || tick >= tui->logical_start_tick + tui->logical_width_ticks) return;
    getmaxyx(tui->grid_derwin, grid_rows, grid_cols);
    col = GRID_LEFT_LABELS_WIDTH
        + (int)((tick - tui->logical_start_tick) / tui->ticks_per_col);
    if (col < GRID_LEFT_LABELS_WIDTH || col >= grid_cols - 1) return;
    wattron(tui->grid_derwin, COLOR_PAIR(GREEN_ON_BLK));
    mvwaddch(tui->grid_derwin, grid_rows - 4, col, '^');
    mvwaddch(tui->grid_derwin, grid_rows - 3, col, '|');
    mvwaddch(tui->grid_derwin, grid_rows - 2, col, '|');
    wattroff(tui->grid_derwin, COLOR_PAIR(GREEN_ON_BLK));
}

static void render_playback(MM_TUI *tui)
{
    double time = atomic_load_explicit(&tui->audio_engine->posted_audio_time,
                                       memory_order_relaxed);
    werase(tui->playback_derwin);
    box(tui->playback_derwin, '|', '=');
    mvwprintw(tui->playback_derwin, 1, 3, "-PLAYING-");
    mvwprintw(tui->playback_derwin, 2, 3, "t=%.3f s", time);
}

static void render_track_properties(MM_TUI *tui)
{
    const MM_Track *track;
    if (!tui->modal_shown) return;
    track = &tui->project->tracks_arr[tui->selected_track_index];
    werase(tui->modal_win);
    box(tui->modal_win, 0, 0);
    mvwprintw(tui->modal_win, 0, 2, " Track properties ");
    mvwprintw(tui->modal_win, 2, 2, "number:   %zu", tui->selected_track_index + 1);
    mvwprintw(tui->modal_win, 3, 2, "name:     %s", track->name);
    mvwprintw(tui->modal_win, 4, 2, "waveform: %s", mm_wave_to_str(track->wave));
    mvwprintw(tui->modal_win, 5, 2, "gain:     %.2f", track->gain);
}

int MM_TUI_render(MM_TUI *tui)
{
    size_t sequence_index;
    MM_Sequence *sequence;

    if (!tui || !tui->curses_initialized) return -1;
    erase();
    if (tui->rows < MIN_ROWS || tui->cols < MIN_COLS) {
        mvprintw(0, 0, "Terminal too small (minimum %dx%d)", MIN_COLS, MIN_ROWS);
        refresh();
        return 0;
    }
    if (!tui->grid_derwin || !tui->playback_derwin) return -1;

    tui->is_playing = atomic_load_explicit(&tui->audio_engine->posted_playing,
                                           memory_order_relaxed);
    update_sizes(tui);
    sequence_index = active_sequence_index(tui);
    sequence = &tui->project->sequence_arr[sequence_index];

    render_info(tui, sequence);
    werase(tui->grid_derwin);
    box(tui->grid_derwin, '|', '=');
    render_note_labels(tui);
    render_grid(tui);
    render_midi(tui, &sequence->tracks[tui->selected_track_index]);
    render_cursor(tui);
    if (tui->is_playing) render_playback(tui);
    render_track_properties(tui);

    wnoutrefresh(stdscr);
    wnoutrefresh(tui->grid_derwin);
    if (tui->is_playing) wnoutrefresh(tui->playback_derwin);
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
    tui->beats_in_bar = project->file->beat_per_bar;
    tui->fps = 30;
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
        init_pair(RED_ON_BLK, COLOR_RED, COLOR_WHITE);
        init_pair(GREEN_ON_BLK, COLOR_GREEN, COLOR_BLACK);
        init_pair(BLACK_ON_CYAN, COLOR_BLACK, COLOR_CYAN);
        init_pair(BLACK_ON_GREEN, COLOR_BLACK, COLOR_MAGENTA);
    }
    if (create_windows(tui) != 0) {
        MM_TUI_destroy(tui);
        return -1;
    }
    snap_to_first_event(tui);
    log_debug("%s", "TUI Init: done");
    return 0;
}

int MM_TUI_step(MM_TUI *tui)
{
    int rows;
    int cols;
    if (!tui || !tui->curses_initialized) return -1;
    getmaxyx(stdscr, rows, cols);
    if (rows != tui->rows || cols != tui->cols) {
        if (create_windows(tui) != 0) return -1;
    }
    handle_input(tui);
    if (MM_TUI_render(tui) != 0) return -1;
    napms((int)(1000 / tui->fps));
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

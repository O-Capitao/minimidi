#include "tui/minimidi-tui.h"

#include "common/minimidi-log.h"

#include <inttypes.h>
#include <locale.h>
#include <math.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TOP_BAR_HEIGHT 3
#define BOTT_BAR_HEIGHT 1
#define GRID_LEFT_LABELS_WIDTH 7
#define LINES_PER_SEMITONE 2
#define PIANO_HEADER_HEIGHT 2
#define MIN_PANE_GRID_ROWS (6 * LINES_PER_SEMITONE + 2)
#define KEY_CTRL_UP (KEY_MAX + 1)
#define KEY_CTRL_DOWN (KEY_MAX + 2)
#define MIN_ROWS 12
#define MIN_COLS 40

static const char *ALL_NOTES[] = {
    "C", "Db", "D", "Eb", "E", "F", "Gb", "G", "Ab", "A", "Bb", "B"
};

static const char *MINIMIDI_LOGO[] = {
    "#   #    #    #   #    #    #   #    #    ####     #  ",
    "## ##         ##  #         ## ##         #   #       ",
    "# # #    #    # # #    #    # # #    #    #   #    #  ",
    "#   #    #    #  ##    #    #   #    #    #   #    #  ",
    "#   #    #    #   #    #    #   #    #    ####     #  "
};

enum {
    RED_ON_BLK = 1,
    GREEN_ON_BLK,
    BLACK_ON_CYAN,
    BLACK_ON_GREEN,
    BLACK_ON_YELLOW,
    BLACK_ON_RED,
    WHITE_ON_BLK,
    BLACK_ON_YELLOW_CURSOR,
    METER_GREEN,
    METER_YELLOW,
    METER_RED,
    SPLASH_WHITE,
    FAINT_GRAY,
    GRAY_BLOCK
};

#define MIXER_MIN_GAIN_DB (-60.0)
#define MIXER_MAX_GAIN_DB 6.0
#define MIXER_METER_RANGE_DB 60.0

static double gain_to_db(double gain)
{
    if (gain <= 0.001) return MIXER_MIN_GAIN_DB;
    return fmax(MIXER_MIN_GAIN_DB, 20.0 * log10(gain));
}

static double db_to_gain(double db)
{
    return db <= MIXER_MIN_GAIN_DB ? 0.0 : pow(10.0, db / 20.0);
}

static double output_peak_to_db(double peak)
{
    return peak <= 0.000001 ? -120.0 : 20.0 * log10(peak);
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

static void snap_to_first_event(MM_TUI *tui);

static void save_focused_pane(MM_TUI *tui)
{
    if (!tui->n_panes || tui->mixer_shown) return;
    MM_TUI_TrackPane *pane = &tui->panes[tui->focused_pane];
    pane->track_index = tui->selected_track_index;
    pane->start_tick = tui->logical_start_tick;
    pane->start_note = tui->logical_start_note;
}

static void load_focused_pane(MM_TUI *tui)
{
    MM_TUI_TrackPane *pane = &tui->panes[tui->focused_pane];
    tui->grid_derwin = pane->window;
    tui->selected_track_index = pane->track_index;
    tui->logical_start_tick = pane->start_tick;
    tui->logical_start_note = pane->start_note;
}

static void set_timeline_start(MM_TUI *tui, uint64_t tick)
{
    tui->logical_start_tick = tick;
    for (size_t i = 0; i < MM_MAX_TRACKS; i++) {
        if (tui->panes[i].initialized) tui->panes[i].start_tick = tick;
    }
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
    for (size_t i = 0; i < MM_MAX_TRACKS; i++) {
        if (tui->panes[i].window) {
            delwin(tui->panes[i].window);
            tui->panes[i].window = NULL;
        }
    }
    tui->grid_derwin = NULL;
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

    int header_rows = PIANO_HEADER_HEIGHT;
    int available;
    size_t pane_count;

    save_focused_pane(tui);
    destroy_windows(tui);
    getmaxyx(stdscr, tui->rows, tui->cols);
    if (tui->rows < MIN_ROWS || tui->cols < MIN_COLS) return 0;

    available = tui->rows - header_rows - BOTT_BAR_HEIGHT;
    pane_count = (size_t)(available / MIN_PANE_GRID_ROWS);
    if (pane_count < 1) pane_count = 1;
    if (pane_count > tui->project->n_tracks) pane_count = tui->project->n_tracks;
    if (pane_count > MM_MAX_TRACKS) pane_count = MM_MAX_TRACKS;
    tui->n_panes = pane_count;
    if (tui->focused_pane >= pane_count) tui->focused_pane = pane_count - 1;
    for (size_t i = 0; i < pane_count; i++) {
        int top = header_rows + (int)i * available / (int)pane_count;
        int bottom = header_rows + (int)(i + 1) * available / (int)pane_count;
        MM_TUI_TrackPane *pane = &tui->panes[i];
        pane->window = derwin(stdscr, bottom - top, tui->cols, top, 0);
        if (!pane->window) return -1;
        if (!pane->initialized) {
            pane->track_index = i;
            tui->grid_derwin = pane->window;
            tui->selected_track_index = i;
            tui->logical_start_tick = 0;
            tui->logical_start_note = 0;
            update_sizes(tui);
            snap_to_first_event(tui);
            pane->start_tick = tui->logical_start_tick;
            pane->start_note = tui->logical_start_note;
            pane->initialized = true;
        }
    }
    load_focused_pane(tui);
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
            tick = clip->start_tick + clip->project_event_ticks[event_index];
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
    if (tui->start_screen) {
        tui->start_screen = false;
        return;
    }
    if (tui->mixer_shown && tui->bpm_editing) {
        if (key >= '0' && key <= '9' && tui->bpm_input_length < sizeof(tui->bpm_input) - 1) {
            tui->bpm_input[tui->bpm_input_length++] = (char)key;
            tui->bpm_input[tui->bpm_input_length] = '\0';
        } else if (key == KEY_BACKSPACE || key == 127 || key == '\b') {
            if (tui->bpm_input_length) tui->bpm_input[--tui->bpm_input_length] = '\0';
        } else if (key == 27) {
            tui->bpm_editing = false;
        } else if (key == '\n' || key == KEY_ENTER) {
            unsigned int bpm = (unsigned int)strtoul(tui->bpm_input, NULL, 10);
            if (bpm >= 20 && bpm <= 300) {
                atomic_store_explicit(&tui->audio_engine->tempo, bpm, memory_order_relaxed);
            } else beep();
            tui->bpm_editing = false;
        }
        return;
    }
    if (tui->mixer_shown && key >= '0' && key <= '9') {
        tui->bpm_editing = true;
        tui->bpm_input_length = 1;
        tui->bpm_input[0] = (char)key;
        tui->bpm_input[1] = '\0';
        return;
    }
    if (!tui->mixer_shown && key >= '1' && key <= '4') {
        size_t index = (size_t)(key - '1');
        if (index < tui->project->n_tracks && index != tui->selected_track_index) {
            tui->selected_track_index = index;
            snap_to_first_event(tui);
        }
        return;
    }
    switch (key) {
        case KEY_CTRL_UP:
        case KEY_CTRL_DOWN:
            if (!tui->mixer_shown && tui->n_panes > 1) {
                save_focused_pane(tui);
                if (key == KEY_CTRL_UP && tui->focused_pane > 0)
                    tui->focused_pane--;
                if (key == KEY_CTRL_DOWN && tui->focused_pane + 1 < tui->n_panes)
                    tui->focused_pane++;
                load_focused_pane(tui);
                update_sizes(tui);
            }
            break;
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
            set_timeline_start(tui, 0);
            push_command(tui, MM_CMD_STOP);
            break;
        case KEY_HOME:
            set_timeline_start(tui, 0);
            push_command(tui, MM_CMD_BACK_TO_BEGINNING);
            break;
        case KEY_LEFT:
            if (tui->mixer_shown && tui->bpm_focused) {
                unsigned int bpm = atomic_load_explicit(&tui->audio_engine->tempo, memory_order_relaxed);
                if (bpm > 20) bpm--;
                atomic_store_explicit(&tui->audio_engine->tempo, bpm, memory_order_relaxed);
            } else if (tui->mixer_shown) {
                if (tui->selected_track_index > 0) tui->selected_track_index--;
            } else set_timeline_start(tui, tui->logical_start_tick > bar
                ? tui->logical_start_tick - bar : 0);
            break;
        case KEY_RIGHT:
            if (tui->mixer_shown && tui->bpm_focused) {
                unsigned int bpm = atomic_load_explicit(&tui->audio_engine->tempo, memory_order_relaxed);
                if (bpm < 300) bpm++;
                atomic_store_explicit(&tui->audio_engine->tempo, bpm, memory_order_relaxed);
            } else if (tui->mixer_shown) {
                if (tui->selected_track_index + 1 < tui->project->n_tracks)
                    tui->selected_track_index++;
            } else if (tui->logical_start_tick <= UINT64_MAX - bar)
                set_timeline_start(tui, tui->logical_start_tick + bar);
            break;
        case KEY_UP:
            if (tui->mixer_shown) {
                double gain = atomic_load_explicit(
                    &tui->audio_engine->tracks_arr[tui->selected_track_index].gain,
                    memory_order_relaxed);
                double db = gain_to_db(gain);
                db = db < MIXER_MAX_GAIN_DB ? fmin(MIXER_MAX_GAIN_DB, db + 1.0) : db;
                gain = db_to_gain(db);
                atomic_store_explicit(
                    &tui->audio_engine->tracks_arr[tui->selected_track_index].gain,
                    gain, memory_order_relaxed);
                tui->project->tracks_arr[tui->selected_track_index].gain = gain;
            } else if (tui->logical_start_note < 127) tui->logical_start_note++;
            break;
        case KEY_DOWN:
            if (tui->mixer_shown) {
                double gain = atomic_load_explicit(
                    &tui->audio_engine->tracks_arr[tui->selected_track_index].gain,
                    memory_order_relaxed);
                double db = gain_to_db(gain);
                db = db > MIXER_MIN_GAIN_DB ? fmax(MIXER_MIN_GAIN_DB, db - 1.0) : db;
                gain = db_to_gain(db);
                atomic_store_explicit(
                    &tui->audio_engine->tracks_arr[tui->selected_track_index].gain,
                    gain, memory_order_relaxed);
                tui->project->tracks_arr[tui->selected_track_index].gain = gain;
            } else if (tui->logical_start_note > 0) tui->logical_start_note--;
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
        case 'm': case 'M':
            save_focused_pane(tui);
            tui->mixer_shown = !tui->mixer_shown;
            if (!tui->mixer_shown) {
                load_focused_pane(tui);
                update_sizes(tui);
            }
            tui->modal_shown = false;
            if (tui->modal_panel) hide_panel(tui->modal_panel);
            break;
        case '\t':
            if (tui->mixer_shown) tui->bpm_focused = !tui->bpm_focused;
            break;
        case KEY_RESIZE:
            create_windows(tui);
            break;
        default:
            break;
    }
}

/* Size columns across every row, then spread spare space between columns. */
static void render_header(int cols, int rows, char fields[3][3][1024])
{
    int widths[3] = {0};
    int total = 0;
    int col = 0;
    int available = cols - 4; /* two gaps of at least two columns */

    if (available < 0) return;
    for (int i = 0; i < 3; i++) {
        for (int row = 0; row < rows; row++) {
            size_t length = strlen(fields[row][i]);
            int width = length > (size_t)available ? available : (int)length;
            if (width > widths[i]) widths[i] = width;
        }
        total += widths[i];
    }
    while (total > available) {
        int widest = 0;
        for (int i = 1; i < 3; i++) {
            if (widths[i] > widths[widest]) widest = i;
        }
        widths[widest]--;
        total--;
    }
    for (int i = 0; i < 3; i++) {
        for (int row = 0; row < rows; row++) {
            if (widths[i] > 0) mvaddnstr(row, col, fields[row][i], widths[i]);
        }
        col += widths[i];
        if (i < 2) col += (cols - total) / 2 + (i == 1 ? (cols - total) % 2 : 0);
    }
}

static void render_info(MM_TUI *tui, const MM_Sequence *sequence)
{
    char fields[3][3][1024] = {{{0}}};
    const MM_Track *track = &tui->project->tracks_arr[tui->selected_track_index];
    const MM_SequenceTrack *sequence_track = &sequence->tracks[tui->selected_track_index];
    uint64_t track_length_ticks = 0;
    attr_t header_style = has_colors() ? COLOR_PAIR(FAINT_GRAY) : 0;
    if (!has_colors() || COLORS < 256) header_style |= A_DIM;

    for (size_t i = 0; i < sequence_track->n_clips; i++) {
        if (sequence_track->clips[i].end_tick > track_length_ticks)
            track_length_ticks = sequence_track->clips[i].end_tick;
    }
    attron(header_style);
    snprintf(fields[0][0], sizeof(fields[0][0]), "Project: %s", tui->project->file->name);
    snprintf(fields[0][1], sizeof(fields[0][1]), "File: %s", tui->project->file->filepath);

    snprintf(fields[1][0], sizeof(fields[1][0]), "Sequence: %s", sequence->name);
    snprintf(fields[1][1], sizeof(fields[1][1]), "Length: %g beats", sequence->length_beats);
    snprintf(fields[1][2], sizeof(fields[1][2]), "Sequences: %zu", tui->project->n_sequences);

    snprintf(fields[2][0], sizeof(fields[2][0]), "Track %zu / %zu",
             tui->selected_track_index + 1, tui->project->n_tracks);
    snprintf(fields[2][1], sizeof(fields[2][1]), "Name: %s", track->name);
    snprintf(fields[2][2], sizeof(fields[2][2]), "Length: %g beats",
             (double)track_length_ticks / tui->project->ppqn);
    render_header(tui->cols, tui->mixer_shown ? 3 : 2, fields);
    attroff(header_style);
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

static void render_midi(MM_TUI *tui, const MM_SequenceTrack *track, bool focused)
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
                size_t end_event_index;
                int row;
                int start_col;
                int end_col;
                int col;
                if (event->status_code != MIDI_NOTE_ON || !event->next) continue;
                end_event_index = (size_t)(event->next - clip->midi->track.event_arr);
                start = cycle_start + clip->project_event_ticks[event_index];
                end = cycle_start + clip->project_event_ticks[end_event_index];
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
                attr_t onset_style = has_colors()
                    ? COLOR_PAIR(focused ? BLACK_ON_CYAN : GRAY_BLOCK) : A_REVERSE;
                attr_t body_style = has_colors()
                    ? COLOR_PAIR(focused ? BLACK_ON_GREEN : GRAY_BLOCK) : A_REVERSE;
                wattron(tui->grid_derwin, onset_style);
                mvwaddch(tui->grid_derwin, row, start_col, ' ');
                wattroff(tui->grid_derwin, onset_style);
                wattron(tui->grid_derwin, body_style);
                for (col = start_col + 1; col <= end_col && col < grid_cols - 1; col++)
                    mvwaddch(tui->grid_derwin, row, col, ' ');
                wattroff(tui->grid_derwin, body_style);
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
    for (int row = 2; row < grid_rows - 1; row++) {
        chtype underlying = mvwinch(tui->grid_derwin, row, col);
        int pair = PAIR_NUMBER(underlying);
        int cursor_pair = pair == BLACK_ON_CYAN || pair == BLACK_ON_GREEN
            ? BLACK_ON_YELLOW_CURSOR : WHITE_ON_BLK;
        mvwaddch(tui->grid_derwin, row, col,
                 COLOR_PAIR(cursor_pair) | ' ');
    }
}

static void render_transport_footer(MM_TUI *tui)
{
    uint64_t tick = active_sequence_tick(tui);
    uint64_t ticks_per_beat = tui->project->ppqn;
    uint64_t ticks_per_bar = ticks_per_beat * tui->beats_in_bar;
    uint64_t bar_number = tick / ticks_per_bar + 1;
    uint64_t beat_number = (tick % ticks_per_bar) / ticks_per_beat + 1;
    uint64_t beat_tick = tick % ticks_per_beat;
    double elapsed = atomic_load_explicit(&tui->audio_engine->posted_audio_time,
                                          memory_order_relaxed);
    unsigned int minutes = (unsigned int)(elapsed / 60.0);
    unsigned int seconds = (unsigned int)elapsed % 60;
    unsigned int centiseconds = (unsigned int)(elapsed * 100.0) % 100;
    unsigned int bpm = atomic_load_explicit(&tui->audio_engine->tempo,
                                            memory_order_relaxed);
    int bpm_col;

    attron(A_REVERSE);
    mvhline(tui->rows - 1, 0, ' ', tui->cols);
    /* ncursesw decodes these UTF-8 symbols using the terminal locale. */
    mvaddstr(tui->rows - 1, 1, MB_CUR_MAX > 1
             ? (tui->is_playing ? "▶" : "■")
             : (tui->is_playing ? ">" : "#"));
    mvprintw(tui->rows - 1, 3, "%03" PRIu64 "|%02" PRIu64 "|%03" PRIu64
             "    %02u:%02u.%02u", bar_number, beat_number, beat_tick,
             minutes, seconds, centiseconds);
    bpm_col = 30;
    if (bpm_col < tui->cols - 1) {
        if (tui->bpm_focused) attron(A_BOLD);
        mvprintw(tui->rows - 1, bpm_col, "BPM %u", bpm);
        if (tui->bpm_focused) attroff(A_BOLD);
        if (tui->bpm_editing && bpm_col + 10 < tui->cols)
            mvprintw(tui->rows - 1, bpm_col + 10, "Edit:%s", tui->bpm_input);
    }
    attroff(A_REVERSE);
    wnoutrefresh(stdscr);
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
    mvwprintw(tui->modal_win, 4, 2, "waveform: %s",
              track->config_file->wave_name ? track->config_file->wave_name
                                            : mm_wave_to_str(track->wave));
    mvwprintw(tui->modal_win, 5, 2, "gain:     %.2f", track->gain);
}

static void render_mixer(MM_TUI *tui)
{
    const int slot_count = MM_MAX_TRACKS;
    const int meter_width = 8;
    const int track_area_width = tui->cols - meter_width;
    const int meter_left = track_area_width;
    size_t i;
    int slot_top = TOP_BAR_HEIGHT + 1;
    int slot_bottom = tui->rows - 2;
    mvaddnstr(TOP_BAR_HEIGHT, 0, "Mixer  Up/Down: gain dB  Left/Right: track  Tab: BPM  m: piano roll",
              tui->cols - 1);
    for (i = 0; i < MM_MAX_TRACKS; i++) {
        int left = (int)i * track_area_width / slot_count;
        int right = (int)(i + 1) * track_area_width / slot_count - 1;
        int width;
        int center;
        int bottom = tui->rows - 5;
        int bar_height = bottom - (slot_top + 3) + 1;
        int row;

        if (i + 1 == MM_MAX_TRACKS) right = tui->cols - 1;
        width = right - left + 1;
        center = left + width / 2;
        if (width < 1) continue;
        if (i < tui->project->n_tracks) {
            double gain = atomic_load_explicit(&tui->audio_engine->tracks_arr[i].gain,
                                               memory_order_relaxed);
            double gain_db = gain_to_db(gain);
            if (bar_height < 1) bar_height = 1;
            mvaddnstr(slot_top + 1, left + 2, tui->project->tracks_arr[i].name,
                      width > 4 ? width - 4 : 1);
            for (row = 0; row < bar_height; row++) {
                int y = bottom - row;
                double row_db = MIXER_MIN_GAIN_DB
                              + (double)(row + 1) / bar_height * -MIXER_MIN_GAIN_DB;
                if (gain_db >= row_db) {
                    wattron(stdscr, COLOR_PAIR(BLACK_ON_GREEN));
                    mvaddch(y, center, ' ');
                    wattroff(stdscr, COLOR_PAIR(BLACK_ON_GREEN));
                } else mvaddch(y, center, '|');
            }
            mvprintw(bottom + 1, center - 3, "%5.1fdB", gain_db);
            if (i == tui->selected_track_index) mvaddch(bottom + 2, center, '^');
        } else {
            mvprintw(slot_top + 1, left + 2, "Track %zu", i + 1);
            mvprintw(slot_top + 3, left + 2, "inactive");
        }

        mvaddch(slot_top, left, ACS_ULCORNER);
        mvaddch(slot_top, right, ACS_URCORNER);
        mvaddch(slot_bottom, left, ACS_LLCORNER);
        mvaddch(slot_bottom, right, ACS_LRCORNER);
        mvhline(slot_top, left + 1, ACS_HLINE, width - 2);
        mvhline(slot_bottom, left + 1, ACS_HLINE, width - 2);
        mvvline(slot_top + 1, left, ACS_VLINE, slot_bottom - slot_top - 1);
        mvvline(slot_top + 1, right, ACS_VLINE, slot_bottom - slot_top - 1);
    }

    {
        double peak = atomic_load_explicit(&tui->audio_engine->posted_output_peak,
                                           memory_order_relaxed);
        double peak_db = output_peak_to_db(peak);
        int meter_x = meter_left + meter_width / 2;
        int bottom = tui->rows - 5;
        int bar_height = bottom - (slot_top + 3) + 1;
        int filled;
        int row;
        if (bar_height < 1) bar_height = 1;
        filled = peak_db <= -MIXER_METER_RANGE_DB ? 0
            : (int)ceil((peak_db + MIXER_METER_RANGE_DB)
                        / MIXER_METER_RANGE_DB * bar_height);
        if (filled > bar_height) filled = bar_height;
        mvaddstr(slot_top + 1, meter_left + 1, "dBFS");
        for (row = 0; row < bar_height; row++) {
            int y = bottom - row;
            double row_db = -MIXER_METER_RANGE_DB
                          + (double)(row + 1) / bar_height * MIXER_METER_RANGE_DB;
            short color = row_db >= -3.0 ? METER_RED
                        : row_db >= -12.0 ? METER_YELLOW : METER_GREEN;
            if (row < filled) {
                wattron(stdscr, COLOR_PAIR(color));
                mvaddch(y, meter_x, ' ');
                wattroff(stdscr, COLOR_PAIR(color));
            } else mvaddch(y, meter_x, '|');
        }
        mvprintw(bottom + 1, meter_left, "%5.1fdB", peak_db);
        mvaddch(slot_top, meter_left, ACS_ULCORNER);
        mvaddch(slot_top, tui->cols - 1, ACS_URCORNER);
        mvaddch(slot_bottom, meter_left, ACS_LLCORNER);
        mvaddch(slot_bottom, tui->cols - 1, ACS_LRCORNER);
        mvvline(slot_top + 1, meter_left, ACS_VLINE, slot_bottom - slot_top - 1);
        mvvline(slot_top + 1, tui->cols - 1, ACS_VLINE, slot_bottom - slot_top - 1);
    }
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

    if (tui->start_screen) {
        int logo_height = (int)(sizeof(MINIMIDI_LOGO) / sizeof(MINIMIDI_LOGO[0]));
        int logo_width = (int)strlen(MINIMIDI_LOGO[0]);
        int x = tui->cols > logo_width ? (tui->cols - logo_width) / 2 : 0;
        int y = tui->rows > logo_height ? (tui->rows - logo_height) / 2 - 1 : 0;
        erase();
        for (int col = 1; col < tui->cols - 1; col += 3) {
            unsigned int seed = (unsigned int)col * 17U + tui->splash_frame / 12U;
            int dot_y = 1 + (int)(seed % (unsigned int)(tui->rows - 2));
            if (has_colors()) attron(COLOR_PAIR(GREEN_ON_BLK) | A_DIM);
            else attron(A_DIM);
            mvaddch(dot_y, col, '.');
            if (has_colors()) attroff(COLOR_PAIR(GREEN_ON_BLK) | A_DIM);
            else attroff(A_DIM);
        }
        for (int row = 0; row < logo_height && y + row < tui->rows - 1; row++) {
            if (has_colors()) attron(COLOR_PAIR(SPLASH_WHITE) | A_BOLD);
            else attron(A_BOLD);
            mvaddnstr(y + row, x, MINIMIDI_LOGO[row], tui->cols - x);
            if (has_colors()) attroff(COLOR_PAIR(SPLASH_WHITE) | A_BOLD);
            else attroff(A_BOLD);
        }
        if (tui->rows > 1) {
            const char *prompt = "Press any key to continue";
            int prompt_x = tui->cols > (int)strlen(prompt)
                ? (tui->cols - (int)strlen(prompt)) / 2 : 0;
            mvaddnstr(tui->rows - 2, prompt_x, prompt, tui->cols - prompt_x);
        }
        tui->splash_frame++;
        refresh();
        return 0;
    }

    tui->is_playing = atomic_load_explicit(&tui->audio_engine->posted_playing,
                                           memory_order_relaxed);
    update_sizes(tui);
    sequence_index = active_sequence_index(tui);
    sequence = &tui->project->sequence_arr[sequence_index];

    render_info(tui, sequence);
    if (tui->mixer_shown) {
        render_mixer(tui);
        render_transport_footer(tui);
        wnoutrefresh(stdscr);
        doupdate();
        return 0;
    }

    save_focused_pane(tui);
    wnoutrefresh(stdscr);
    for (size_t i = 0; i < tui->n_panes; i++) {
        MM_TUI pane_view = *tui;
        MM_TUI_TrackPane *pane = &tui->panes[i];
        char label[1024];
        uint64_t length_ticks = 0;
        const MM_SequenceTrack *track = &sequence->tracks[pane->track_index];
        pane_view.grid_derwin = pane->window;
        pane_view.selected_track_index = pane->track_index;
        pane_view.logical_start_tick = pane->start_tick;
        pane_view.logical_start_note = pane->start_note;
        update_sizes(&pane_view);
        pane->start_tick = pane_view.logical_start_tick;
        werase(pane->window);
        box(pane->window, '|', '=');
        render_note_labels(&pane_view);
        render_grid(&pane_view);
        if (i != tui->focused_pane) render_cursor(&pane_view);
        for (size_t clip = 0; clip < track->n_clips; clip++) {
            if (track->clips[clip].end_tick > length_ticks)
                length_ticks = track->clips[clip].end_tick;
        }
        snprintf(label, sizeof(label), " %sTrack %zu / %zu | %s | Length: %g beats ",
                 i == tui->focused_pane ? "> " : "", pane->track_index + 1,
                 tui->project->n_tracks, tui->project->tracks_arr[pane->track_index].name,
                 (double)length_ticks / tui->project->ppqn);
        mvwaddnstr(pane->window, 0, 2, label, tui->cols - 4);
        if (i != tui->focused_pane) {
            attr_t style = has_colors() ? COLOR_PAIR(FAINT_GRAY) : 0;
            if (!has_colors() || COLORS < 256) style |= A_DIM;
            for (int row = 0; row < getmaxy(pane->window); row++)
                mvwchgat(pane->window, row, 0, -1, style & A_ATTRIBUTES,
                         has_colors() ? FAINT_GRAY : 0, NULL);
        }
        render_midi(&pane_view, track, i == tui->focused_pane);
        if (i == tui->focused_pane) render_cursor(&pane_view);
        wnoutrefresh(pane->window);
    }
    load_focused_pane(tui);
    update_sizes(tui);
    render_track_properties(tui);

    render_transport_footer(tui);
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
    tui->start_screen = true;

    setlocale(LC_CTYPE, "");
    if (!initscr()) return -1;
    tui->curses_initialized = true;
    raw();
    noecho();
    keypad(stdscr, TRUE);
    define_key("\033[1;5A", KEY_CTRL_UP);
    define_key("\033[1;5B", KEY_CTRL_DOWN);
    nodelay(stdscr, TRUE);
    curs_set(0);
    if (has_colors()) {
        start_color();
        use_default_colors();
        init_pair(RED_ON_BLK, COLOR_RED, COLOR_WHITE);
        init_pair(GREEN_ON_BLK, COLOR_GREEN, COLOR_BLACK);
        init_pair(BLACK_ON_CYAN, COLOR_BLACK, COLOR_CYAN);
        init_pair(BLACK_ON_GREEN, COLOR_BLACK, COLOR_MAGENTA);
        init_pair(BLACK_ON_YELLOW, COLOR_BLACK, COLOR_YELLOW);
        init_pair(BLACK_ON_RED, COLOR_BLACK, COLOR_RED);
        init_pair(WHITE_ON_BLK, COLOR_BLACK, COLOR_WHITE);
        init_pair(BLACK_ON_YELLOW_CURSOR, COLOR_BLACK, COLOR_YELLOW);
        init_pair(METER_GREEN, COLOR_BLACK, COLOR_GREEN);
        init_pair(METER_YELLOW, COLOR_BLACK, COLOR_YELLOW);
        init_pair(METER_RED, COLOR_BLACK, COLOR_RED);
        init_pair(SPLASH_WHITE, COLOR_WHITE, COLOR_BLACK);
        init_pair(FAINT_GRAY, COLORS >= 256 ? 244 : COLOR_WHITE, -1);
        init_pair(GRAY_BLOCK, COLOR_BLACK, COLORS >= 256 ? 244 : COLOR_WHITE);
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

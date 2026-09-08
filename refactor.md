# Multitrack Refactor Plan

## Goal and Completion Criteria

`multitrack` changes MiniMIDI's input from one MIDI file to one YAML project containing an ordered arrangement of logical tracks and sequences. The rework is complete when `./minimidi test_proj.yaml`:

- loads the project and every referenced MIDI file with actionable errors;
- plays all configured tracks together, honoring sequence and assignment timing, length, and loop settings;
- shows one selected track in the piano-roll view and switches it with number keys;
- supports up to four tracks as a policy limit without using fixed-size runtime arrays;
- preserves the stable transport, scrolling, zoom, cursor, and TUI behavior from `main`;
- exits cleanly with the audio stream stopped and all owned memory released.

Playback stops after the final configured play of the final sequence.

## Implementation Status

Implemented on the `multitrack` worktree. The YAML reader now validates and
normalizes timing, the runtime compiler builds dynamic track/sequence/clip
arrays, the MIDI loader safely flattens format-0/1 files, and the audio engine
schedules and mixes the arrangement. The TUI renders and switches logical
tracks, and initialization/shutdown follows dependency order. `make test`
covers the parser, round trip, cache, PPQN scaling, format-1 flattening,
sequencer boundaries, repeats, project end, and representative validation
failures.

## Original Branch and Working-Tree Baseline

At review time, `multitrack` is two commits ahead of the local `main`. `origin/main` also contains later TUI work (`cd10021`, `b83d439`) that is not in `multitrack`. Port those stable fixes deliberately; a blind merge will conflict with the new component types.

There are also uncommitted changes in `main.c`, `minimidi-tui.c`, and `minimidi-tui.h`. A source-only GCC check currently fails because the C file still uses `MM_TUI_TrackPropertiesComponent` and `track_component.props_component`, while the header has replaced/removed them. Establish one clean baseline before functional work.

## Architecture

```text
test_proj.yaml -> YAML parser -> MM_File_Project (file-shaped model)
                                      |
MIDI files -> MIDI parser/cache -> MM_Project (playback model)
                                      |
                  +-------------------+-------------------+
                  v                                       v
          PortAudio callback                         ncurses TUI
       (all tracks + sequencer)                  (one selected track)
                  ^                                       |
                  +------- lock-free command queue <------+
                  +------- atomic playback time ---------->
```

| Area | Files | Existing intent and state |
| --- | --- | --- |
| MIDI parsing | `minimidi.[ch]` | Safely reads SMF format 0 or 1, flattens tracks by absolute tick, normalizes velocity-zero note-ons, links note pairs, and offers filtered event lists. |
| Project file | `minimidi-proj-file.[ch]` | Uses libyaml to strictly read/write metadata, track configurations, sequences, and MIDI assignments; preserves time strings and exposes ticks/beats. |
| Runtime project | `minimidi-proj.[ch]` | Owns parsed configuration, a canonical-path MIDI cache, dynamic logical tracks, and per-sequence/per-track clip arrays. |
| Audio | `minimidi-audio.[ch]` | Uses one monophonic synth per logical track, schedules clips and repeats in integer sample/tick time, mixes waveform/gain output, and publishes atomic transport state. |
| TUI | `minimidi-tui.[ch]` | Shows the selected arranged track in a piano roll, switches with `1`-`4`, and supports transport, scrolling, zoom, cursor following, resize, and a track-properties panel. |
| Entry/lifecycle | `main.c` | Validates every initializer and unwinds audio, TUI, command queue, runtime project, and logging in dependency order. |

## Project-File Contract from `README.md` and `test_proj.yaml`

- Project fields: `name`, `tempo`, `sample_rate`, `ppqn`, and `beat_per_bar`.
- `track-config`: logical track name to `wave` (`SQUARE`, `TRIANGLE`, or `SIN`) and `gain`.
- `sequence`: ordered sections with `name`, musical `length`, optional `n_repeats`, and a map of logical tracks. `n_repeats: 0` is infinite; legacy `loop` is read-only compatibility.
- Track assignment: `midi`, optional `start` (default `0`), `loop`, and `length`.
- A track used by a sequence must exist in `track-config`; an unknown reference rejects the project.

The README specifies the file-shaped object graph as `MM_File_Project` owning arrays of `MM_File_TrackConfig` and `MM_File_Sequence`; each sequence owns `MM_File_Sequence_Track` entries, and each sequence track owns assignments plus a pointer to its matching track config. That pointer is a required validated relationship, not an optional best-effort link. The implementation names the APIs `MM_Proj_File_read`/`MM_Proj_File_write`, while the README calls them `MM_File_read`/`MM_File_write`; one public naming convention should be chosen and documented.

The write API must serialize the same hierarchy and create the destination when it does not exist. Its contract should also define overwrite behavior and guarantee that a successfully written file can be read back to an equivalent model.

The README applies compact time strings to **every** `start` and `length`. `B` denotes bars, `b` denotes beats, `t` denotes ticks, components may be omitted, and literal `0` means zero. The parsed model retains the original strings, exact canonical ticks, and derived fractional beats.

## Original Blockers (Addressed)

1. **Runtime arrays are invalid.** `MM_Project_init` allocates `sequence_arr` while `n_sequences` is still zero, never copies either count from the file model, and never builds `tracks_arr`. It then writes sequence entries out of bounds. The YAML reader returns `-1`, but the caller only treats values greater than zero as errors.
2. **The MIDI cache cannot retain entries.** `midi_map_put` receives the uthash head by value, so insertion cannot update `p->midi_map`. Cache teardown also frees entries without calling `MM_File_free` on their values.
3. **Loaded MIDI event lists are lost.** `MM_File_init` populates a local linked list instead of `f->events`. File-open/read/allocation failures are not safely returned, and the parser assumes exactly one track chunk occupying the remainder of the file.
4. **Sequencing is incomplete.** The runtime sequence has no clips/assignments. Loop counters and next break times are not advanced correctly, track state is not reloaded at boundaries, and advancing past the final sequence is unchecked.
5. **Audio currently produces silence.** `MM_AudioTrack_produce` immediately returns zero. The mixer accumulator is uninitialized, only clips positive peaks, ignores gain/waveform, and the callback uses `BUFFER_SIZE` rather than PortAudio's `frames_per_buffer`. YAML sample rate is ignored.
6. **The TUI is not connected to project tracks.** There is no selected-track index or number-key handling. Component `load_track`/`update` methods are stubs, rendering is placeholder-only, transport input is commented out, and the current `T` handler falls through to quit.
7. **Lifecycle and ownership are unsafe.** `main` ignores initializer results, never destroys the audio engine or command queue, and can free project data while the PortAudio callback still references it. Linked-list destruction mixes embedded and heap ownership.
8. **There is no regression harness.** The Makefile builds only the application; stale object files can obscure source failures, and no parser, scheduler, or audio tests exist.

## Ordered Work Plan

### 1. Freeze Contracts and Restore a Buildable Baseline

- Resolve the current TUI header/source mismatch and decide which modal/component work is retained.
- Reconcile the multitrack component design with the later stable changes on `origin/main`.
- Add an explicit C standard and dependency discovery for PortAudio, ncurses/panel, libyaml, and uthash. Declare or replace POSIX-only helpers such as `strdup` correctly.
- Make a clean rebuild the first gate; do not rely on checked-in/stale binaries.

### 2. Define and Validate the YAML Contract

- Document required fields, defaults, ranges, duplicate-name behavior, and whether unknown keys are errors.
- Implement one strict time parser for every sequence/assignment `start` and `length`, using project `beat_per_bar` and `ppqn`. Preserve the original string and produce a canonical 64-bit tick value (plus derived beat values if the public file model retains the README's proposed `*_beats` fields). Reject malformed, out-of-order, repeated, fractional, or overflowing components.
- Reject unknown track references, unsupported waveforms, invalid gains/tempos/rates, empty projects, and more than the configured track limit.
- Resolve relative MIDI paths against the YAML file's directory, not the process working directory.
- Check every allocation and libyaml emitter/parser result; free partial models on failure. Define whether writing replaces an existing file, verify creation of a missing destination, and add read/write/read round-trip coverage for the complete nested model.

### 3. Compile File Data into a Runtime Arrangement

- Populate counts before allocating arrays and create one stable logical-track index per `track-config` entry.
- Extend `MM_Sequence` to contain runtime clips per track: MIDI pointer, start tick, end/duration tick, loop mode, and current event position. A missing track in a sequence should explicitly mean silence.
- Repair the MIDI cache API so it updates the head, deduplicates paths, owns loaded files, and frees each file exactly once.
- Keep the four-track maximum in validation/configuration (for example, `MM_MAX_TRACKS`), while allocating and iterating by `n_tracks` everywhere else.

### 4. Make MIDI Loading a Safe Foundation

- Zero-initialize outputs and return errors for missing, truncated, malformed, unsupported-format, or unsupported-division files.
- Store the event list in `MM_Midi_File`, clarify list-node ownership, and use non-recursive cleanup.
- Correctly skip meta/SysEx events, handle running status and note-on velocity zero, bounds-check all reads, and avoid unsigned octave/frequency indexing errors.
- Either explicitly support only format 0 or implement all declared SMF tracks; never silently parse only the first.

### 5. Implement a Deterministic Sequencer

- Keep arrangement time in integer ticks/sample positions; convert to seconds only at interfaces.
- At each sequence boundary, increment/reset loop counters, handle leftover callback frames, update duration, and transition without reading beyond the array.
- Activate/deactivate clips at assignment boundaries, repeat looped clips, consume every event due at the current time, and emit note-offs when clips or sequences end.
- Define pause, stop, restart, seek, infinite-loop, and final-sequence behavior as explicit state transitions.

### 6. Complete the Real-Time Audio Path

- Implement event consumption and sample generation in `MM_AudioTrack_produce`; apply track waveform and gain.
- Decide whether a logical track is intentionally monophonic. If not, allocate voices outside the callback and support overlapping notes safely.
- Mix from a zeroed accumulator, limit both polarities, use the callback's frame count, and honor/validate the configured sample rate.
- Keep allocation, file I/O, blocking, and verbose logging out of the callback. Prepare sequence/clip state before starting PortAudio.

### 7. Reconnect the TUI

- Add `selected_track_index` and bind `1`-`4` to existing tracks; ignore invalid selections safely and derive behavior from `n_tracks` so a future limit increase is localized.
- Load the selected logical track and the relevant sequence/clip events into the existing piano-roll renderer. Show the selected track name/number, waveform, gain, sequence, loop, and transport time.
- Restore play/pause, navigation, zoom, cursor-following, and frame pacing from the stable UI, adapted to project time.
- Finish one modal/panel ownership model, call the proper panel refresh functions, handle terminal resize/minimum size, and propagate component errors.

### 8. Fix Initialization, Shutdown, and Diagnostics

- Give every initializer a consistent success/error contract and make `main` unwind only successfully initialized resources.
- Shutdown in dependency order: stop/close PortAudio, destroy TUI windows, free the queue, free runtime/MIDI data, then close logging.
- Replace placeholder messages with errors that include project path, sequence, track, assignment, and underlying cause.

### 9. Add Automated Verification

- Add `make test` for time parsing, YAML defaults/errors/round trips, track linking and the four-track limit, path resolution, cache deduplication, and MIDI failure cases.
- Test the sequencer without PortAudio using a deterministic tick/sample driver: simultaneous tracks, omitted tracks, clip offsets, finite/infinite loops, boundary leftovers, and project end.
- Test mixer/event logic offline and track selection as UI-independent state. Keep an interactive TUI/audio smoke test for `test_proj.yaml`.
- Add a warning-clean build plus AddressSanitizer/UndefinedBehaviorSanitizer runs; use a leak checker for repeated load/shutdown cycles.

## Behavior Decisions

1. Sequence `n_repeats` is the total number of plays. It defaults to `1`; `0` means infinite. The reader accepts legacy `loop`, while the writer emits `n_repeats`.
2. Assignment `start` is placement within its sequence; there is no MIDI source-offset property.
3. Assignment `length` truncates playback. If omitted, it is the MIDI file's natural duration.
4. Assignments on a logical track cannot overlap. Each logical track is monophonic.
5. Playback stops at the end of the final sequence.
6. MIDI tempo events are ignored in favor of project `tempo`. MIDI ticks are rescaled to project `ppqn`.
7. SMF format 0 and format 1 are accepted; format-1 tracks are flattened.
8. The parsed model retains source strings and exposes canonical ticks plus derived fractional beats.

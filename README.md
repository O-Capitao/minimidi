# MiniMIDI

MiniMIDI is a small, terminal-based MIDI arranger and software synthesizer
written in C. A YAML project describes a set of synth tracks and an ordered
arrangement of Standard MIDI Files. MiniMIDI loads that arrangement, renders
each logical track with a simple oscillator, and provides playback and piano
roll navigation through an ncurses interface.

The current engine is intentionally compact: projects have at most four
tracks, each track is monophonic, and MIDI note events drive square, triangle,
or sine oscillators. Project tempo controls the whole arrangement.

## Features

- YAML project files with strict schema and range validation
- Standard MIDI File format 0 and format 1 input
- Per-track waveform and gain controls
- Multiple named sequences, finite repeats, and infinite repeats
- Timed, optionally looping MIDI clips
- Automatic MIDI path resolution relative to the project file
- PPQN conversion between source MIDI files and the project timeline
- Real-time mono audio through PortAudio
- Responsive ncurses piano-roll view and transport controls
- YAML read/write API and an offline render path used by the tests

## Architecture

MiniMIDI separates file parsing from the runtime arrangement and keeps the UI
off the real-time audio path.

```text
 PROJECT.yaml                         .MID files
      |                                   |
      v                                   v
 libyaml parser                    MIDI parser
 minimidi-proj-file.[ch]           minimidi.[ch]
      |                                   |
      +--------> runtime compiler <-------+
                 minimidi-proj.[ch]
                         |
                         v
                  immutable project
                    /           \
                   v             v
          ncurses UI          PortAudio callback
        minimidi-tui.[ch]     minimidi-audio.[ch]
                   |             |
                   +-- commands ->|  lock-free ring buffer
                   |<- status ----+  atomic snapshots
```

The main components are:

| Component | Responsibility |
| --- | --- |
| `main.c` | Owns startup, shutdown, the main UI loop, and subsystem lifetimes. |
| `minimidi-proj-file.[ch]` | Reads, validates, and writes YAML. It retains the source spelling of time values and MIDI paths while also calculating ticks and resolved paths. |
| `minimidi.[ch]` | Parses SMF headers and tracks, flattens MIDI tracks into a tick-sorted note-event array, and pairs note-on/note-off events for display. |
| `minimidi-proj.[ch]` | Compiles the file model into playback-ready sequences and clips, caches each distinct MIDI file, rescales durations, sorts clips, and rejects invalid arrangements. |
| `minimidi-audio.[ch]` | Advances the sequence timeline in the PortAudio callback, consumes transport commands, dispatches MIDI notes, synthesizes each track, and mixes mono audio. It also exposes an offline renderer. |
| `minimidi-tui.[ch]` | Draws the active sequence as a piano roll, follows the playhead, and sends transport commands. |
| `minimidi-rb.[ch]` | Implements the fixed-capacity single-producer/single-consumer command queue shared by the UI and audio callback. |
| `minimidi-log.[ch]` | Writes diagnostics to `minimidi.log`. |

Project data is fully loaded before audio starts and remains stable during
playback. The UI never mutates audio state directly: it pushes small commands
to the ring buffer. The callback publishes time, sequence, tick, and playback
state through atomics for the UI to read.

## Requirements

- A C11 compiler (GCC is the default)
- `make` and `pkg-config`
- ncurses and panel development files
- PortAudio development files
- libyaml development files
- A terminal at least 40 columns by 12 rows
- A working default audio output device

The Makefile looks up these pkg-config modules:
`portaudio-2.0`, `ncurses`, `panel`, and `yaml-0.1`.

## Build and run

```sh
make compile
./minimidi test_proj.yaml
```

The executable accepts exactly one argument: the path to a YAML project.
MiniMIDI starts stopped at the beginning of the first sequence. Diagnostics
are printed to standard error and written to `minimidi.log` in the current
working directory.

Other useful targets are:

```sh
make test       # build and run the automated parser/sequencer tests
make sanitize   # run the tests with AddressSanitizer and UBSan
make clean      # remove the executable, objects, and test binary
```

For a manual end-to-end check, run `make clean && make compile`, launch
`./minimidi test_proj.yaml`, and exercise playback and the TUI controls.

## Project input

A project is a YAML mapping with seven required top-level properties. Unknown
or duplicate properties are rejected.

```yaml
name: "My Project"
tempo: 120
sample_rate: 44100
ppqn: 960
beat_per_bar: 4

track-config:
  bass:
    wave: SQUARE
    gain: 0.80
  lead:
    wave: TRIANGLE
    gain: 0.60

sequence:
  - name: "intro"
    length: "2B"
    n_repeats: 1
    tracks:
      bass:
        - midi: "bassline1.MID"
          start: "0"
          loop: true
          length: "2B"
      lead:
        - midi: "midi_test_002.MID"
          start: "0B2b"
          loop: false
          length: "2b"

  - name: "verse"
    length: "4B"
    n_repeats: 4
    tracks:
      bass:
        - midi: "midi_test_003.MID"
          start: "0"
          loop: true
          length: "4B"
```

### Project properties

| Property | Meaning and validation |
| --- | --- |
| `name` | Non-empty project name displayed by the TUI. |
| `tempo` | Project tempo in BPM, from 1 through 500. MIDI tempo events do not override it. |
| `sample_rate` | PortAudio output rate, from 8,000 through 384,000 Hz. The default output device must support the selected rate. |
| `ppqn` | Project ticks per quarter note, from 1 through 32,767. |
| `beat_per_bar` | Beats per bar, from 1 through 32. This is used to convert `B` time components; the engine otherwise assumes a quarter-note beat. |
| `track-config` | Mapping of one to four unique logical track names to synth settings. |
| `sequence` | Non-empty list of named arrangement sections, played in YAML order. Sequence names must be unique. |

Each track configuration requires both of these properties:

- `wave`: one of the case-sensitive values `SQUARE`, `TRIANGLE`, or `SIN`.
- `gain`: a finite number from `0.0` through `1.0`.

A sequence requires `name`, a positive `length`, and a `tracks` mapping. It
may use any subset of the configured tracks; omitted tracks are silent for
that sequence. `n_repeats` is the total number of times the sequence plays,
not the number of additional repeats. It defaults to `1`; `0` repeats the
sequence forever and prevents later sequences from being reached. The legacy
sequence key `loop` is accepted as a numeric alias for `n_repeats`, but the
writer always emits `n_repeats`.

Each key below `tracks` must exactly match a name in `track-config` and maps to
a non-empty list of MIDI assignments. An assignment supports:

| Property | Required | Default | Meaning |
| --- | --- | --- | --- |
| `midi` | yes | — | Source Standard MIDI File. Relative paths are resolved from the directory containing the YAML file; absolute paths are preserved. |
| `start` | no | `0` | Placement on the sequence timeline. |
| `loop` | no | `false` | Repeat the source MIDI for the duration of this assignment. YAML booleans `true`/`false`, `yes`/`no`, and `1`/`0` are accepted case-insensitively where applicable. |
| `length` | no | natural MIDI duration | Playback window for the assignment. It must be positive when present. |

Assignments may appear in any YAML order; they are sorted by `start` while the
runtime project is compiled. Assignments on the same logical track must not
overlap, and every assignment must end at or before its sequence's declared
length. Touching boundaries are allowed.

`length` defines the clip's reserved window. With `loop: true`, the complete
source MIDI timeline repeats until that window ends. With `loop: false`, the
source plays once; if an explicit length is longer than the MIDI data, the
remainder of the window is silent. If `length` is omitted, it is the source's
natural duration, so a looping clip normally needs a longer explicit length
to repeat.

### Time strings

Sequence lengths and assignment `start`/`length` values use compact,
integer-only time strings:

- `B` means bars and uses `beat_per_bar`.
- `b` means beats and uses `ppqn` ticks per beat.
- `t` means raw project ticks.

Components must be ordered from largest to smallest, and each unit may occur
at most once. Components may be omitted.

| Value | Meaning with four beats per bar |
| --- | --- |
| `2B` | two bars |
| `4b` | four beats, equivalent to one bar |
| `12t` | twelve ticks |
| `0B1b2t` | one beat and two ticks |
| `0` | exactly zero; primarily useful for assignment starts |

Values such as `1.5b`, `1b1B`, repeated units, a bare number other than the
special value `0`, and zero sequence/assignment lengths are invalid.

## MIDI input and playback behavior

MiniMIDI accepts Standard MIDI Files with:

- format 0 or format 1 headers;
- metrical PPQN timing; and
- one or more `MTrk` chunks.

Format 1 tracks are flattened into one event stream and sorted by absolute
tick. If note-off and note-on events share a tick, note-off is processed first.
Source ticks are rescaled to the project PPQN, while project `tempo` determines
wall-clock playback. SMPTE time divisions and format 2 files are rejected.

Only note-on and note-off messages affect synthesis. A note-on with velocity
zero is treated as note-off. Other channel messages, meta events (including
tempo changes), and SysEx data are skipped. Their timing still contributes to
the source track duration.

There is one oscillator per logical project track, so playback is monophonic
within that track even if its MIDI contains chords or multiple channels. The
latest note-on becomes active; a note-off silences it only when it matches the
currently active note. Velocity, program changes, controllers, pitch bend,
aftertouch, envelopes, and instruments are not modeled.

The audio callback mixes all configured tracks into a single floating-point
mono channel and clamps the result to `[-1.0, 1.0]`. At a sequence boundary it
either restarts the sequence, advances to the next one, or stops after the
last finite sequence. Pressing play after the complete arrangement has ended
rewinds first.

Distinct MIDI paths are cached in the runtime project, so reusing a file in
multiple assignments does not parse or store it more than once.

## TUI controls

| Key | Behavior |
| --- | --- |
| `Space` | Play or pause without changing the current position. |
| `s` | Stop and rewind to the first sequence. |
| `Home` | Rewind to the first sequence; playback continues if it was already running. |
| `q` | Quit. |
| `1`–`4` | Select a configured track for the piano-roll view. |
| Left / Right | Move the view backward or forward by one project bar. This does not seek playback. |
| Up / Down | Move the visible MIDI-note range upward or downward. |
| `+` / `-` | Zoom the timeline in or out. |
| `t` | Show or hide the selected track's waveform and gain. |

The grid labels rows with note names and octaves and marks musical bars.
During playback, the view follows the playhead when it moves outside the
visible window. Resizing the terminal recreates the ncurses windows; terminals
smaller than 40x12 show a size warning.

## YAML library API

The project-file layer can also be used independently:

```c
MM_File_Project project;

if (MM_Proj_File_read("project.yaml", &project) == 0) {
    MM_Proj_File_write(&project, "project-copy.yaml");
    MM_Proj_File_free(&project);
}
```

`MM_Proj_File_write` creates or replaces the destination. It preserves stored
time strings and YAML MIDI path spellings, emits explicit assignment defaults,
and normalizes the legacy sequence `loop` property to `n_repeats`.

The full runtime project is constructed with `MM_Project_init` and released
with `MM_Project_free`. See the matching headers for ownership details.

## Optional MIDI inspection helper

`easylivin/midiinfo.py` can print decoded fixture events. It requires Python 3
and the third-party `mido` package:

```sh
python3 easylivin/midiinfo.py bassline1.MID
```

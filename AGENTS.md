# Repository Guidelines

## Project Structure & Module Organization

MiniMIDI is a small C application kept mostly at the repository root. `main.c` wires together the project loader, audio engine, ring buffer, logger, and ncurses UI. Each subsystem uses matching source/header pairs: `minimidi.[ch]` parses MIDI data, `minimidi-proj-file.[ch]` handles YAML project files, `minimidi-proj.[ch]` builds runtime project state, `minimidi-audio.[ch]` provides synthesis and PortAudio playback, and `minimidi-tui.[ch]` implements the terminal interface. Shared definitions live in `globals.h` and `minimidi-transport.h`.

Sample `.MID` files and `test_proj.yaml` are manual test fixtures. `test_output.yaml` is example serialized output. `easylivin/midiinfo.py` is an optional MIDI inspection helper; it requires Python and `mido`.

## Build, Test, and Development Commands

- `make compile` builds the `minimidi` executable with GCC. Install development headers for ncurses/panel, PortAudio, and libyaml first.
- `./minimidi test_proj.yaml` launches the TUI against the sample project; run it in a terminal with working audio output.
- `make clean` removes the executable and all object files generated from root and nested C sources.
- `python3 easylivin/midiinfo.py bassline1.MID` prints decoded MIDI events for fixture inspection.

The Makefile enables debug symbols, `-Wall`, and `-pedantic`. Treat new warnings as defects.

## Coding Style & Naming Conventions

Follow the existing C style: four-space indentation, braces on their own line for functions, and explicit fixed-width integer types where binary MIDI formats require them. Public types and functions use the `MM_` prefix (`MM_Project`, `MM_Project_init`); constants use uppercase snake case. Keep public declarations in the matching header, implementation details `static`, and ownership/freeing behavior apparent in API names. No automated formatter is configured, so preserve nearby formatting.

## Testing Guidelines

There is currently no automated test target or coverage requirement. Before submitting, run `make clean && make compile`, then smoke-test relevant loading, playback, and TUI paths with `test_proj.yaml`. Add small MIDI/YAML fixtures for parser regressions and name them descriptively; do not replace existing fixtures without explaining why.

## Commit & Pull Request Guidelines

Recent commits use brief, lower-case subjects such as `good clock` and `tui dev`. Prefer a clearer imperative subject describing one logical change, for example `fix sequence timing rollover`. Pull requests should summarize behavior changes, list build and manual-test results, link related issues, and include a terminal screenshot for visible TUI changes. Keep generated binaries, object files, and runtime logs out of commits.

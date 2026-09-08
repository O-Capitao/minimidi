# Vibe coding:

Write me a header file and implementation for a set of functions and type definitions in C, meant to parse a yaml data file using libyaml.

The file looks like this:

```yaml
name: "My Project"
tempo: 120         # unsigned int
sample_rate: 44000 # unsigned int
ppqn: 960   # unsiged int
beat_per_bar: 4
track-config:
  bass:
    wave: SQUARE # enum -> can also be TRIANGLE / SIN
    gain: 0.8    # double
  lead:
    wave: TRIANGLE # enum -> can also be TRIANGLE / SIN
    gain: 0.96    # double
sequence:
  - name: "intro"
    length: "2B"
    tracks:
      bass:
      - midi: "midi_test_003.MID"
        start: "0B2b"
        loop: true
        length: "2b"
      - midi: "midi_test_003.MID"
        start: "4b"
        loop: true
        length: "2b"
      lead:
      - midi: "midi_test_002.MID"
        start: "0"
        loop: true
        length: "2b"
  - name: "verse"
    length: "4B"
    n_repeats: 4   # total plays; 0 repeats forever
    tracks:
      bass:
      - midi: "midi_test_005.MID"
        start: "0" # if "start" is ommitted, assume = 0 
        loop: true
        length: "2b"
      lead:
      - midi: "midi_test_002.MID"
        start: "0"
        loop: true
        length: "2b"
```

`MM_Proj_File_read`, taking the YAML file path, parses these contents into a
`MM_File_Project`, with nested `MM_File_TrackConfig` and `MM_File_Sequence`
arrays. Relative MIDI paths are resolved from the YAML file's directory.
Each MM_File_Sequence in turn contains a number of MM_File_Sequence_Track ("bass" - "lead" - etc). NOTE: the name of the MM_File_Sequence_Track MUST match a defined MM_File_TrackConfig, other wise an error should be thrown.

Each MM_File_Sequence_Track contains a pointer to the corresponding MM_File_TrackConfig and an array of MM_File_Sequence_Track_Midi_Assignment.

## How it should work
`MM_Proj_File_write`, taking a pointer to an `MM_File_Project` and a file path,
writes the same hierarchy, replacing the destination if it exists.

Sequence `n_repeats` is the total number of plays and defaults to `1`; `0`
means repeat forever. The legacy sequence property `loop` is accepted when
reading but writers always emit `n_repeats`. Assignment `start` is placement in
the sequence. Assignment `length` truncates the clip and defaults to the MIDI
file's natural duration. Assignments on one logical track may not overlap.

Project tempo always controls playback. MIDI tempo events are ignored, format-0
and format-1 files are accepted, format-1 tracks are flattened, and MIDI ticks
are rescaled to the project PPQN.




If the file in the passed path does not exist then a new file should be created.









## Time strings

Every `start` and `length` uses an ordered compact time string. `B` means bars,
`b` means beats, and `t` means ticks, so `0B1b2t` is one beat and two ticks.
Components may be omitted (`4b`, `1B`, or `12t`), and literal `0` means zero.
Components cannot be repeated or reordered, and fractional values are rejected.

The parsed model retains each original string, stores an exact `uint64_t` tick
value, and exposes a derived fractional beat value. Bar conversion uses the
project's `beat_per_bar`, and tick conversion uses its `ppqn`.


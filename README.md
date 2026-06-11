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
    loop: 4   # play this part 4 times
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

A MM_File_read function, taking in the yaml file path should parse these contents into a set of structs: MM_File_Project, and nested within: MM_File_TrackConfig and an array of MM_File_Sequence.
Each MM_File_Sequence in turn contains a number of MM_File_Sequence_Track ("bass" - "lead" - etc). NOTE: the name of the MM_File_Sequence_Track MUST match a defined MM_File_TrackConfig, other wise an error should be thrown.

Each MM_File_Sequence_Track contains a pointer to the corresponding MM_File_TrackConfig and an array of MM_File_Sequence_Track_Midi_Assignment.

## How it should work
A MM_File_write, taking a pointer into a MM_File_Project and a file path, should write the contents into a structure such as the one shown above.




If the file in the passed path does not exist then a new file should be created.









## Notes aboout length strings, e.g.: "2B2b":
This is relevant for all "start" and "length" properties in the file:

- length strings, looking like "0B1b2t" are used as a way to compress midi time information into a compressed, yet human readable form. The logic is that the number that is shown before "B" is the number of bars, before "b" comes a number of beats. The length is the sum of the two parcels, given that each bar will contain "beat_per_bar beats. 
Either of the 2 can be ommited e.g: "4b" -> 4 beats, "1B" -> beat_per_bar * 


The parser should take care to unpack this string into a single unsigned int - "start_beats" or "length_beats" - containing the value in beats in the MM_File_Project type, and store the original string value in "length_string" for debugging. If a "0" value is found, then it translates to 0.




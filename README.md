# minimidi
Dumbest midi reader ever

## about midi:
https://ccrma.stanford.edu/~craig/14q/midifile/MidiFileFormat.html

## chunks
Each chunk has a 4-character ASCII type and a 32-bit length value

### Header

<Header Chunk> = <chunk type>       <length> <format><ntrks><division>
                the four ASCII      32 bit   16-bit  16-bit  16-bit
                characters 'MThd'
 



# design
## about writing files
> GOAL:
Write streamlined,
synthple optimized
MIDI files,

this means:

> HEADER PROPS:

- *format code* -> sepre 0
- *ntrks* -> sempre 1
- *division* -> sempre 960 (standard de facto)


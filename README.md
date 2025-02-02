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



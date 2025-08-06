#ifndef MINIMIDI_RING
#define MINIMIDI_RING

#define BUFFER_SIZE 1024

typedef struct MiniMidi_Ring_Buffer {
    int size;
    float data[BUFFER_SIZE];
    int head_index, tail_index;

} MiniMidi_Ring_Buffer;

int MiniMidi_Ring_Buffer__init          ( MiniMidi_Ring_Buffer *self );
int MiniMidi_Ring_Buffer__destroy       ( MiniMidi_Ring_Buffer *self );
int MiniMidi_Ring_Buffer__get_free_space( MiniMidi_Ring_Buffer *self );
int MiniMidi_Ring_Buffer__push          ( MiniMidi_Ring_Buffer *self, float input );
int MiniMidi_Ring_Buffer__pop           ( MiniMidi_Ring_Buffer *self, float *output );

#endif
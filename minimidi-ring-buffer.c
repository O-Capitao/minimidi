#include "minimidi-ring-buffer.h"

int MiniMidi_Ring_Buffer__init( MiniMidi_Ring_Buffer *self ) {
    self->size = BUFFER_SIZE;

    // not needed actually...
    for (int i = 0; i < BUFFER_SIZE; i++) { self->data[i] = 0; }

    self->head_index = 0;
    self->tail_index = 0;

    return 0;
}

int MiniMidi_Ring_Buffer__destroy( MiniMidi_Ring_Buffer *self ){
    return 0;
}

int MiniMidi_Ring_Buffer__get_free_space( MiniMidi_Ring_Buffer *self ){
    return 1; // nd
}

int MiniMidi_Ring_Buffer__push( MiniMidi_Ring_Buffer *self, float input ){
    return 1;
}

int MiniMidi_Ring_Buffer__pop( MiniMidi_Ring_Buffer *self, float *output ){
    return 1;
}

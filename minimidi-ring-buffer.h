#ifndef MINIMIDI_RING
#define MINIMIDI_RING

#define BUFFER_SIZE 1024

#include <stdbool.h>
#include <stddef.h>

typedef struct MiniMidi_Ring_Buffer {
    int size;
    float data[BUFFER_SIZE];
    int head, tail;
    bool is_flipped;
} MiniMidi_Ring_Buffer;

MiniMidi_Ring_Buffer *MiniMidi_Ring_Buffer__init();

int MiniMidi_Ring_Buffer__destroy   ( MiniMidi_Ring_Buffer *self );

/**
 * Push a number of elements of input_arr into internal data arr
 * returns: count of items pushed.
 * -1 if error
 *  
 */
int MiniMidi_Ring_Buffer__push_n ( MiniMidi_Ring_Buffer *self, float *input_arr, size_t n );

/***
 * Pop n items from internal storage into output_arr
 * returns: count of elements popped.
 * -1 if error
 */
int MiniMidi_Ring_Buffer__pop_n  ( MiniMidi_Ring_Buffer *self, float *output_arr, size_t n );

#endif
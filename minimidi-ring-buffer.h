#ifndef MINIMIDI_RING
#define MINIMIDI_RING


#include <stdbool.h>
#include <stddef.h>


/**
* GENERIC RB IMPL
*/
typedef struct MM_Ring_Buffer {

    int size, item_size,
        head, tail;

    void *data;
    bool is_flipped;

} MM_Ring_Buffer;

MM_Ring_Buffer *MM_Ring_Buffer__init  ( size_t item_size, size_t buffer_size );
int    MM_Ring_Buffer__destroy        ( MM_Ring_Buffer *self );
int    MM_Ring_Buffer__pop_n          ( MM_Ring_Buffer *self, void *output_arr, size_t n );
int    MM_Ring_Buffer__push_n         ( MM_Ring_Buffer *self, void *input_arr, size_t n );
size_t MM_Ring_Buffer__get_free_space ( MM_Ring_Buffer *s );

#endif
#ifndef MINIMIDI_RB_H
#define MINIMIDI_RB_H

#include <stdbool.h>
#include <stddef.h>

// If you change SAMPLE type later, do it here (or in one central place)
typedef float SAMPLE;

typedef struct MM_Ring_Buffer {
    _Atomic size_t head;      // Updated by producer
    _Atomic size_t tail;      // Updated by consumer + producer (when overwriting)
    size_t         size;      // Total capacity (power of 2 recommended)
    SAMPLE        *buffer;    // Contiguous storage
}  MM_Ring_Buffer;


MM_Ring_Buffer *MM_Ring_Buffer__init(size_t size);
void            MM_Ring_Buffer__free(MM_Ring_Buffer *rb);

size_t MM_Ring_Buffer__push_n(MM_Ring_Buffer *rb, const SAMPLE *src, size_t n);
size_t MM_Ring_Buffer__pop_n (MM_Ring_Buffer *rb, SAMPLE *dest, size_t n);


// Optional helpers
bool   MM_Ring_Buffer__is_empty(const MM_Ring_Buffer *rb);
size_t MM_Ring_Buffer__get_free_space(const MM_Ring_Buffer *rb);
size_t MM_Ring_Buffer__capacity(const MM_Ring_Buffer *rb);

#endif
#ifndef MINIMIDI_RB_H
#define MINIMIDI_RB_H

#include <stdbool.h>
#include <stddef.h>
#include <stdatomic.h>

typedef struct MM_Ring_Buffer {
    _Atomic size_t head;
    _Atomic size_t tail;

    size_t size;        // number of slots
    size_t item_size;   // size of each item
    void  *buffer;      // raw storage
} MM_Ring_Buffer;

MM_Ring_Buffer *MM_Ring_Buffer__init(size_t capacity, size_t item_size);
void            MM_Ring_Buffer__free(MM_Ring_Buffer *rb);

bool   MM_Ring_Buffer__push(MM_Ring_Buffer *rb, const void *item);
bool   MM_Ring_Buffer__pop (MM_Ring_Buffer *rb, void *out_item);

bool   MM_Ring_Buffer__is_empty(const MM_Ring_Buffer *rb);
size_t MM_Ring_Buffer__capacity(const MM_Ring_Buffer *rb);

#endif
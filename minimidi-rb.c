#include "minimidi-rb.h"
#include <stdlib.h>
#include <string.h>

static inline size_t _next_index(size_t i, size_t size) {
    return (i + 1) % size;
}

MM_Ring_Buffer *MM_Ring_Buffer__init(size_t capacity, size_t item_size) {
    MM_Ring_Buffer *rb = malloc(sizeof(MM_Ring_Buffer));
    if (!rb) return NULL;

    rb->size = capacity + 1;  // one slot unused (classic ringbuffer trick)
    rb->item_size = item_size;
    rb->buffer = malloc(rb->size * item_size);

    atomic_store(&rb->head, 0);
    atomic_store(&rb->tail, 0);

    return rb;
}

void MM_Ring_Buffer__free(MM_Ring_Buffer *rb) {
    if (!rb) return;
    free(rb->buffer);
    free(rb);
}

bool MM_Ring_Buffer__push(MM_Ring_Buffer *rb, const void *item) {
    size_t head = atomic_load_explicit(&rb->head, memory_order_relaxed);
    size_t tail = atomic_load_explicit(&rb->tail, memory_order_acquire);

    size_t next = _next_index(head, rb->size);

    if (next == tail) {
        return false; // full
    }

    memcpy((char*)rb->buffer + head * rb->item_size, item, rb->item_size);

    atomic_store_explicit(&rb->head, next, memory_order_release);
    return true;
}

bool MM_Ring_Buffer__pop(MM_Ring_Buffer *rb, void *out_item) {
    size_t tail = atomic_load_explicit(&rb->tail, memory_order_relaxed);
    size_t head = atomic_load_explicit(&rb->head, memory_order_acquire);

    if (tail == head) {
        return false; // empty
    }

    memcpy(out_item, (char*)rb->buffer + tail * rb->item_size, rb->item_size);

    atomic_store_explicit(&rb->tail, _next_index(tail, rb->size), memory_order_release);
    return true;
}

bool MM_Ring_Buffer__is_empty(const MM_Ring_Buffer *rb) {
    return atomic_load(&rb->head) == atomic_load(&rb->tail);
}

size_t MM_Ring_Buffer__capacity(const MM_Ring_Buffer *rb) {
    return rb->size - 1;
}

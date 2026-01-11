#include "minimidi-rb.h"
#include <stdatomic.h>
// #include <stdbool.h>
// #include <stddef.h>
#include <stdlib.h>
#include <string.h>  // for memcpy

// Change this typedef to match your audio sample format (e.g. float, int16_t, int32_t, ...)
// typedef float SAMPLE;



/**
 * Initialize a new ring buffer
 * @param size  Must be power of 2 for optimal performance (though not strictly required)
 * @return      Allocated ring buffer or NULL on failure
 */
MM_Ring_Buffer *MM_Ring_Buffer__init(size_t size) {
    if (size == 0) return NULL;

    MM_Ring_Buffer *rb = malloc(sizeof(MM_Ring_Buffer));
    if (!rb) return NULL;

    rb->buffer = malloc(sizeof(SAMPLE) * size);
    if (!rb->buffer) {
        free(rb);
        return NULL;
    }

    rb->size = size;
    atomic_init(&rb->head, 0);
    atomic_init(&rb->tail, 0);
    return rb;
}

/**
 * Free the ring buffer and its internal storage
 */
void MM_Ring_Buffer__free(MM_Ring_Buffer *rb) {
    if (rb) {
        free(rb->buffer);
        free(rb);
    }
}

/**
 * Push up to n samples into the ring buffer.
 * If there's not enough space, oldest samples are overwritten (tail advanced).
 * Always writes as many samples as possible (up to n).
 *
 * @return  Number of samples actually written (normally == n)
 */
size_t MM_Ring_Buffer__push_n(MM_Ring_Buffer *rb, const SAMPLE *src, size_t n) {
    if (n == 0) return 0;

    size_t written = 0;
    size_t capacity = rb->size;

    while (written < n) {
        size_t head = atomic_load_explicit(&rb->head, memory_order_relaxed);
        size_t tail = atomic_load_explicit(&rb->tail, memory_order_acquire);

        size_t used;
        if (head >= tail) {
            used = head - tail;
        } else {
            used = head + capacity - tail;
        }

        size_t free = capacity - used;
        if (free == 0) {
            // Buffer full → try to advance tail (overwrite oldest)
            size_t next_tail = (tail + 1) % capacity;
            if (atomic_compare_exchange_strong_explicit(
                    &rb->tail, &tail, next_tail,
                    memory_order_release, memory_order_relaxed)) {
                // Successfully discarded one old sample → continue
                continue;
            }
            // CAS failed → consumer moved tail meanwhile → retry loop
            continue;
        }

        // We have some space → calculate how much we can write now
        size_t to_write = n - written;
        if (to_write > free) to_write = free;

        // Compute contiguous write range (with wrap)
        size_t space_to_end = capacity - head;
        size_t part1 = (to_write <= space_to_end) ? to_write : space_to_end;
        size_t part2 = to_write - part1;

        // Write part 1
        memcpy(&rb->buffer[head], &src[written], part1 * sizeof(SAMPLE));

        // Write part 2 (if wrap)
        if (part2 > 0) {
            memcpy(rb->buffer, &src[written + part1], part2 * sizeof(SAMPLE));
        }

        // Advance head atomically
        size_t new_head = (head + to_write) % capacity;
        atomic_store_explicit(&rb->head, new_head, memory_order_release);

        written += to_write;
    }

    return written;
}

/**
 * Pop up to n samples from the ring buffer into dest.
 * Returns how many were actually popped (may be < n if not enough data).
 */
size_t MM_Ring_Buffer__pop_n(MM_Ring_Buffer *rb, SAMPLE *dest, size_t n) {
    if (n == 0) return 0;

    size_t read = 0;
    size_t capacity = rb->size;

    while (read < n) {
        size_t tail = atomic_load_explicit(&rb->tail, memory_order_relaxed);
        size_t head = atomic_load_explicit(&rb->head, memory_order_acquire);

        size_t available;
        if (head >= tail) {
            available = head - tail;
        } else {
            available = head + capacity - tail;
        }

        if (available == 0) {
            break;  // Buffer empty
        }

        size_t to_read = n - read;
        if (to_read > available) to_read = available;

        // Compute contiguous read range (with wrap)
        size_t space_to_end = capacity - tail;
        size_t part1 = (to_read <= space_to_end) ? to_read : space_to_end;
        size_t part2 = to_read - part1;

        // Read part 1
        memcpy(&dest[read], &rb->buffer[tail], part1 * sizeof(SAMPLE));

        // Read part 2 (if wrap)
        if (part2 > 0) {
            memcpy(&dest[read + part1], rb->buffer, part2 * sizeof(SAMPLE));
        }

        // Advance tail atomically
        size_t new_tail = (tail + to_read) % capacity;
        atomic_store_explicit(&rb->tail, new_tail, memory_order_release);

        read += to_read;
    }

    return read;
}

/* Optional helpers */

bool MM_Ring_Buffer__is_empty(const MM_Ring_Buffer *rb) {
    return atomic_load(&rb->head) == atomic_load(&rb->tail);
}

size_t MM_Ring_Buffer__count_approx(const MM_Ring_Buffer *rb) {
    size_t h = atomic_load(&rb->head);
    size_t t = atomic_load(&rb->tail);
    size_t cap = rb->size;
    return (h >= t) ? h - t : h + cap - t;
}

size_t MM_Ring_Buffer__capacity(const MM_Ring_Buffer *rb) {
    return rb->size;
}

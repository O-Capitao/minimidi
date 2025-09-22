#include "minimidi-ring-buffer.h"
#include "minimidi-log.h"

#include <string.h>
#include <assert.h>
#include <stdlib.h>

size_t MM_Ring_Buffer__get_free_space( MM_Ring_Buffer *s ){
    
    if (s->head == s->tail){
        return s->is_flipped ? 0 : s->size;
    }

    return s->is_flipped ?
        s->size + s->tail - s->head - 1 
        : s->size - (s->tail - s->head); 
}


MM_Ring_Buffer *MM_Ring_Buffer__init( size_t item_size, size_t buffer_size ) {

    snprintf( MiniMidi_Log_log_line, sizeof(MiniMidi_Log_log_line), "minimidi-ring-buffer > MM_Ring_Buffer__init()");
    MiniMidi_Log_writeline();
   
    MM_Ring_Buffer *b = (MM_Ring_Buffer*)malloc(sizeof( MM_Ring_Buffer));

    b->size = buffer_size;
    b->item_size = item_size;
    b->head = b->tail = 0;
    b->data = (void *)malloc( buffer_size * item_size );

    snprintf( MiniMidi_Log_log_line, sizeof(MiniMidi_Log_log_line), "minimidi-ring-buffer > MM_Ring_Buffer__init() completed.");
    MiniMidi_Log_writeline();

    return b;
}

int MM_Rng_Buffer__destroy( MM_Ring_Buffer *self){
    free( self->data );
    free( self );
    return 0;
}

int MM_Ring_Buffer__pop_n( MM_Ring_Buffer *s, void *output_arr, size_t n ){
    // pop n items from the buffer into the output array,
    // return the number of items moved if OK, -1 if error
    int poppable = s->size - MM_Ring_Buffer__get_free_space(s);
    int effective_to_pop = n > poppable ? poppable : n;

    if (s->head < s->tail || !s->is_flipped) {
        memcpy(output_arr, (char*)s->data + s->head * s->item_size, effective_to_pop * s->item_size);
    } else {
        int _head_to_end = s->size - s->head;
        memcpy(output_arr, (char*)s->data + s->head * s->item_size, _head_to_end * s->item_size);
        memcpy((char*)output_arr + _head_to_end * s->item_size, s->data, (effective_to_pop - _head_to_end) * s->item_size);
    }

    s->head = (s->head + effective_to_pop) % s->size;
    if (s->head == s->tail) {
        s->is_flipped = false;
    }

    return effective_to_pop;
}

int MM_Ring_Buffer__push_n( MM_Ring_Buffer *s, void *input_arr, size_t n ){
    size_t _av = MM_Ring_Buffer__get_free_space(s);

    if ( n > _av ){
        // no available size...
        return -1;
    }

    if (!s->is_flipped){
        size_t _av_till_end = s->size - s->tail;
        // index flip
        if ( n > _av_till_end ){
            memcpy( (char*)s->data + s->tail * s->item_size, input_arr, _av_till_end * s->item_size );
            memcpy( s->data, (char*)input_arr + _av_till_end * s->item_size, (n - _av_till_end) * s->item_size );
            s->tail = n - _av_till_end;
            s->is_flipped = true;
        } else {
            memcpy( (char*)s->data + s->tail * s->item_size, input_arr, n * s->item_size );
            s->tail += n;
        }
    } else {
        memcpy( (char*)s->data + s->tail * s->item_size, input_arr, n * s->item_size );
        s->tail += n;
    }
    return 0;
}

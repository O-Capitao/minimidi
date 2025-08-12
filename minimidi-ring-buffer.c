#include "minimidi-ring-buffer.h"
#include <string.h>
#include <assert.h>
#include <stdlib.h>

/**
* PVT
*/
size_t _get_free_space( MiniMidi_Ring_Buffer *s ){
    
    if (s->head == s->tail){
        return s->is_flipped ? s->size : 0;
    }

    return s->is_flipped ?
        s->size + s->tail - s->head - 1 :
        s->size - (s->tail - s->head); 
}


/**
 * PUB
 */
MiniMidi_Ring_Buffer *MiniMidi_Ring_Buffer__init() {
   
    // MiniMidi_TUI *ui = (MiniMidi_TUI*)malloc( sizeof( MiniMidi_TUI ) );
    MiniMidi_Ring_Buffer *b = (MiniMidi_Ring_Buffer*)malloc(sizeof( MiniMidi_Ring_Buffer));
    b->size = BUFFER_SIZE;
    b->head = b->tail = 0;

    return b;
}

// is this needed?
int MiniMidi_Ring_Buffer__destroy( MiniMidi_Ring_Buffer *s ){
    free(s);
    return 0;
}


// should I return the amount of items pushed / popped?
int MiniMidi_Ring_Buffer__push_n( MiniMidi_Ring_Buffer *s, float *input, size_t n ){
    
    size_t _av = _get_free_space(s);
    
    
    if ( n > _av ){
        // ño available size...
        return -1;
    }

    if (!s->is_flipped){
        size_t _av_till_end = s->size - s->tail;

        // index flip
        if ( n > _av_till_end ){

            if (memcpy( s->data + s->tail, input, _av_till_end * sizeof(float) ) != 0 ){
                return -1;
            }

            if (memcpy( s->data, input + _av_till_end, (n - _av_till_end) * sizeof(float) ) != 0 ){
                return -1;
            }

            s->tail = n - _av_till_end;
            s->is_flipped = true;
        
        } else {
            if (memcpy( s->data + s->tail, input, n * sizeof(float) ) != 0 ){
                return -1;
            }
            s->tail += n;
        }
    } else {
        // array is flipped...
        if (memcpy( s->data + s->tail, input, n * sizeof(float) ) != 0 ){
            return -1;
        }
        s->tail += n;
    }

    return n;
}

int MiniMidi_Ring_Buffer__pop_n( MiniMidi_Ring_Buffer *s, float *output, size_t n ){

    // if more items than stored are requested, fn returns what is there
    int poppable = s->size - _get_free_space(s);
    
    int effective_to_pop = n > poppable ? poppable : n;


    if (!s->is_flipped){
        if (memcpy( output, s->data + s->head, effective_to_pop * sizeof(float) ) != 0 ){
            return -1;
        }
        s->head += effective_to_pop;
        
    } else {

        int _head_to_end = s->size - s->head;

        // pop last _head_to_end elements
        if (memcpy( output, s->data + s->head, _head_to_end * sizeof(float) ) != 0 ){
            return -1;
        }
        if (memcpy( output + _head_to_end, s->data, (effective_to_pop - _head_to_end) * sizeof(float) ) != 0 ){
            return -1;
        }

        s->head = effective_to_pop - _head_to_end;
        
        // sigh, unflip
        s->is_flipped = false;

    }

    return effective_to_pop;
}

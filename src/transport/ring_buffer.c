#include <transport/ring_buffer.h>

int alloc_ring_buffer(struct ring_buffer_t **dst){
    *dst = (struct ring_buffer_t *)malloc(sizeof(struct ring_buffer_t));
    (*dst)->buf = malloc(RING_BUFFER_SIZE);
    (*dst)->st = 0;
    (*dst)->ed = 0;
    return 0;
}


int free_ring_buffer(struct ring_buffer_t **dst){
    free((*dst)->buf);
    free(*dst);
    *dst = NULL;
    return 0;
}

ssize_t get_buffer_size(struct ring_buffer_t *r){
    return (r->ed - r->st) & (RING_BUFFER_SIZE - 1);
}

ssize_t get_buffer_free_size(struct ring_buffer_t *r){
    return (r->st - r->ed - 1) & (RING_BUFFER_SIZE - 1);
}

int push_ring_buffer(struct ring_buffer_t *r, void *data, ssize_t size){
    if(size > get_buffer_free_size(r)) return -1;
    if(RING_BUFFER_SIZE - r->ed >= size){
        memcpy((void *)(r->buf + r->ed), data, size);
        r->ed = (r->ed + size) & (RING_BUFFER_SIZE - 1);
        return 0;
    }
    memcpy((void *)(r->buf + r->ed), data, RING_BUFFER_SIZE - r->ed);
    memcpy((void *)(r->buf), data + RING_BUFFER_SIZE - r->ed, size - (RING_BUFFER_SIZE - r->ed));
    r->ed = size - (RING_BUFFER_SIZE - r->ed);
    return 0;
}

ssize_t get_ring_buffer(struct ring_buffer_t *r, void *data, ssize_t offset, ssize_t size){
    ssize_t rsize = get_buffer_size(r);
    if(offset >= rsize) return 0;
    if(size + offset > rsize) size = rsize - offset;
    if(RING_BUFFER_SIZE >= r->st + size + offset){
        if(data != NULL){
            memcpy(data, (void *)(r->buf + r->st + offset), size);
        }
        return size;
    }
    if(data != NULL){
        if(r->st + offset < RING_BUFFER_SIZE){
            memcpy(data, (void *)(r->buf + r->st + offset), RING_BUFFER_SIZE - r->st - offset);
            memcpy(data + RING_BUFFER_SIZE - r->st - offset, (void *)(r->buf), size - (RING_BUFFER_SIZE - r->st - offset));
        }else{
            memcpy(data, (void *)(r->buf) + r->st + offset - RING_BUFFER_SIZE, size);
        }
    }
    return size;
}

ssize_t consume_ring_buffer(struct ring_buffer_t *r, void *data, ssize_t size){
    ssize_t rsize = get_buffer_size(r);
    if(size > rsize) size = rsize;
    if(RING_BUFFER_SIZE - r->st >= size){
        if(data != NULL){
            memcpy(data, (void *)(r->buf + r->st), size);
        }
        r->st = (r->st + size) & (RING_BUFFER_SIZE - 1);
        return size;
    }
    if(data != NULL){
        memcpy(data, (void *)(r->buf + r->st), RING_BUFFER_SIZE - r->st);
        memcpy(data + RING_BUFFER_SIZE - r->st, (void *)(r->buf), size - (RING_BUFFER_SIZE - r->st));
    }
    r->st = size - (RING_BUFFER_SIZE - r->st);
    return size;
}


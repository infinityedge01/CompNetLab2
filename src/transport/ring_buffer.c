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

size_t get_buffer_free_size(struct ring_buffer_t *r){
    return (r->st - r->ed - 1) & (RING_BUFFER_SIZE - 1);
}

int push_ring_buffer(struct ring_buffer_t *r, void *data, size_t size){
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

size_t consume_ring_buffer(struct ring_buffer_t *r, void *data, size_t size){
    size_t rsize = get_buffer_free_size(r);
    if(size > rsize) size = rsize;
    if(RING_BUFFER_SIZE - r->st >= size){
        memcpy(data, (void *)(r->buf + r->st), size);
        r->st = (r->st + size) & (RING_BUFFER_SIZE - 1);
        return size;
    }
    memcpy(data, (void *)(r->buf + r->st), RING_BUFFER_SIZE - r->st);
    memcpy(data + RING_BUFFER_SIZE - r->st, (void *)(r->buf), size - (RING_BUFFER_SIZE - r->st));
    r->st = size - (RING_BUFFER_SIZE - r->st);
    return size;
}


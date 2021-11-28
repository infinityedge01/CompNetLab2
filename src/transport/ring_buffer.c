#include <transport/ring_buffer.h>

int alloc_ring_buffer(struct ring_buffer_t **dst, ssize_t size){
    *dst = (struct ring_buffer_t *)malloc(sizeof(struct ring_buffer_t));
    (*dst)->size = size;
    (*dst)->buf = malloc(size);
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
    return (r->ed - r->st) & (r->size - 1);
}

ssize_t get_buffer_free_size(struct ring_buffer_t *r){
    return (r->st - r->ed - 1) & (r->size - 1);
}

int push_ring_buffer(struct ring_buffer_t *r, void *data, ssize_t size){
    if(size > get_buffer_free_size(r)) return -1;
    if(r->size - r->ed >= size){
        memcpy((void *)(r->buf + r->ed), data, size);
        r->ed = (r->ed + size) & (r->size - 1);
        return 0;
    }
    memcpy((void *)(r->buf + r->ed), data, r->size - r->ed);
    memcpy((void *)(r->buf), data + r->size - r->ed, size - (r->size - r->ed));
    r->ed = size - (r->size - r->ed);
    return 0;
}

ssize_t get_ring_buffer(struct ring_buffer_t *r, void *data, ssize_t offset, ssize_t size){
    ssize_t rsize = get_buffer_size(r);
    if(offset >= rsize) return 0;
    if(size + offset > rsize) size = rsize - offset;
    if(r->size >= r->st + size + offset){
        if(data != NULL){
            memcpy(data, (void *)(r->buf + r->st + offset), size);
        }
        return size;
    }
    if(data != NULL){
        if(r->st + offset < r->size){
            memcpy(data, (void *)(r->buf + r->st + offset), r->size - r->st - offset);
            memcpy(data + r->size - r->st - offset, (void *)(r->buf), size - (r->size - r->st - offset));
        }else{
            memcpy(data, (void *)(r->buf) + r->st + offset - r->size, size);
        }
    }
    return size;
}

ssize_t consume_ring_buffer(struct ring_buffer_t *r, void *data, ssize_t size){
    ssize_t rsize = get_buffer_size(r);
    if(size > rsize) size = rsize;
    if(r->size - r->st >= size){
        if(data != NULL){
            memcpy(data, (void *)(r->buf + r->st), size);
        }
        r->st = (r->st + size) & (r->size - 1);
        return size;
    }
    if(data != NULL){
        memcpy(data, (void *)(r->buf + r->st), r->size - r->st);
        memcpy(data + r->size - r->st, (void *)(r->buf), size - (r->size - r->st));
    }
    r->st = size - (r->size - r->st);
    return size;
}


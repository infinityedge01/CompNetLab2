/**
* @file tcp.h
* @brief header for tcp protocol stack
*/
#ifndef TRANSPORT_RING_BUFFER_H
#define TRANSPORT_RING_BUFFER_H

#include <stdlib.h>
#include <string.h>

#define RING_BUFFER_SIZE 32768
#define WRITE_RING_BUFFER_SIZE 1048576

struct ring_buffer_t{
    unsigned char *buf;
    ssize_t size;
    ssize_t st;
    ssize_t ed;
};

int alloc_ring_buffer(struct ring_buffer_t **dst, ssize_t size);

int free_ring_buffer(struct ring_buffer_t **dst);

ssize_t get_buffer_size(struct ring_buffer_t *r);

ssize_t get_buffer_free_size(struct ring_buffer_t *r);

int push_ring_buffer(struct ring_buffer_t *r, void *data, ssize_t size);

ssize_t get_ring_buffer(struct ring_buffer_t *r, void *data, ssize_t offset, ssize_t size);

ssize_t consume_ring_buffer(struct ring_buffer_t *r, void *data, ssize_t size);

#endif
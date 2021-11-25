/**
* @file tcp.h
* @brief header for tcp protocol stack
*/
#ifndef TRANSPORT_RING_BUFFER_H
#define TRANSPORT_RING_BUFFER_H

#include <stdlib.h>
#include <string.h>

#define RING_BUFFER_SIZE 16384
struct ring_buffer_t{
    unsigned char *buf;
    size_t st;
    size_t ed;
};

int alloc_ring_buffer(struct ring_buffer_t **dst);

int free_ring_buffer(struct ring_buffer_t **dst);

size_t get_buffer_free_size(struct ring_buffer_t *r);

int push_ring_buffer(struct ring_buffer_t *r, void *data, size_t size);

size_t consume_ring_buffer(struct ring_buffer_t *r, void *data, size_t size);

#endif
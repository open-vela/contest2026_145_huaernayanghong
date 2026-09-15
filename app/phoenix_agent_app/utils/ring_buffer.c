#include "ring_buffer.h"
#include <string.h>

void ring_buffer_init(ring_buffer_t *rb, void *buffer, size_t elem_size, size_t capacity) {
    if (!rb || !buffer || elem_size == 0 || capacity == 0) {
        return;
    }
    rb->buffer = (uint8_t *)buffer;
    rb->elem_size = elem_size;
    rb->capacity = capacity;
    rb->head = 0;
    rb->tail = 0;
    rb->count = 0;
}

bool ring_buffer_push(ring_buffer_t *rb, const void *elem) {
    if (!rb || !rb->buffer || !elem) {
        return false;
    }
    if (rb->count >= rb->capacity) {
        return false;
    }
    memcpy(rb->buffer + (rb->tail * rb->elem_size), elem, rb->elem_size);
    rb->tail = (rb->tail + 1) % rb->capacity;
    rb->count++;
    return true;
}

bool ring_buffer_pop(ring_buffer_t *rb, void *elem) {
    if (!rb || !rb->buffer || !elem) {
        return false;
    }
    if (rb->count == 0) {
        return false;
    }
    memcpy(elem, rb->buffer + (rb->head * rb->elem_size), rb->elem_size);
    rb->head = (rb->head + 1) % rb->capacity;
    rb->count--;
    return true;
}

bool ring_buffer_peek(const ring_buffer_t *rb, void *elem) {
    if (!rb || !rb->buffer || !elem) {
        return false;
    }
    if (rb->count == 0) {
        return false;
    }
    memcpy(elem, rb->buffer + (rb->head * rb->elem_size), rb->elem_size);
    return true;
}

size_t ring_buffer_count(const ring_buffer_t *rb) {
    return rb ? rb->count : 0;
}

bool ring_buffer_is_full(const ring_buffer_t *rb) {
    return rb ? (rb->count >= rb->capacity) : true;
}

bool ring_buffer_is_empty(const ring_buffer_t *rb) {
    return rb ? (rb->count == 0) : true;
}

void ring_buffer_clear(ring_buffer_t *rb) {
    if (rb) {
        rb->head = 0;
        rb->tail = 0;
        rb->count = 0;
    }
}

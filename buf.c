#include <string.h>
#include <stdlib.h>
#include "buf.h"

void buf_create(struct buffer *b, unsigned int size)
{
    /* malloc a buffer with `size` elements */
    b->buf = malloc(size);
    b->head = b->tail = b->buf;
    b->size = size;
}

int buf_read(struct buffer *b, char *dest, size_t n)
{
    if ((b->tail - b->head) < n) return -1;
    memcpy(dest, b->head, n);
    b->head += n;
    if (b->head == b->tail) {
        /* no more data left. move pointers back to the start */
        b->head = b->tail = b->buf;
    }
    return n;
}

void buf_flush(struct buffer *b)
{
    /* move the buffer to the beginning of the array. */
    unsigned int tmp = (b->tail - b->head);
    memmove(b->buf, b->head, tmp);
    b->head = b->buf;
    b->tail -= tmp;
}

int buf_write(struct buffer *b, char *src, size_t n)
{
    if (b->size < n) {
        /* not enough space */
        return -1;
    }

    if (((b->buf + b->size) - b->tail) < n) {
        /* there isn't enough space at the end of the buffer.
         * need to flush it back to the beginning */
        buf_flush(b);
    }

    memcpy(b->tail, src, n);
    b->tail += n;
    return 0;
}

void buf_free(struct buffer *b)
{
    free(b->buf);
    b->head = b->tail = NULL;
    b->size = -1;
}


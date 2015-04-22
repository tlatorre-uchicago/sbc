#include <string.h>
#include <stdlib.h>
#include "buf.h"

struct buffer *buf_init(unsigned int size)
{
    /* malloc a buffer with `size` elements */
    struct buffer *b = malloc(sizeof (struct buffer));
    b->buf = malloc(size);
    b->head = b->tail = b->buf;
    b->size = size;
    return b;
}

int buf_read(struct buffer *b, char *dest, size_t n)
{
    /* read `n` bytes from buffer. If there aren't `n` bytes available,
     * returns -1. */
    if (BUF_LEN(b) < n) return -1;
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
    unsigned int tmp = BUF_LEN(b);
    memmove(b->buf, b->head, tmp);
    b->head = b->buf;
    b->tail = b->buf + tmp;
}

int buf_write(struct buffer *b, char *src, size_t n)
{
    /* write `n` bytes to the buffer from `src`. If there isn't enough space,
     * return -1. If there isn't enough bytes at the end of the buffer, move
     * the buffer back to the beginning */
    if ((b->size - BUF_LEN(b)) < n) return -1;

    if (BUF_FREE_SPACE(b) < n) {
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
    free(b);
}


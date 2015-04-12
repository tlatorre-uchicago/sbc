#include <stdlib.h>

/* a struct for buffering outgoing data */
struct buffer
{
    char *buf;
    char *head;
    char *tail;
    unsigned int size;
};

#define BYTES(x) (x.tail - x.head)
#define FREE_SPACE(x) ((x.buf + x.size) - x.tail)

void buf_create(struct buffer *b, unsigned int size);
int buf_read(struct buffer *b, char *dest, size_t n);
void buf_flush(struct buffer *b);
int buf_write(struct buffer *b, char *src, size_t n);
void buf_free(struct buffer *b);

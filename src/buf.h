#ifndef BUF_H
#define BUF_H

/* Fixed size buffer which is useful for buffering socket streams.
 *
 * Example:
 *
 * int len, bytes_received;
 *
 * char tmp[100];
 *
 * struct buffer *b = buf_init(BUFSIZE);
 *
 * while (1) {
 *     bytes_received = recv(sock, tmp, 100, 0);
 *     if (bytes_received > 0) {
 *         buf_write(b, msg, bytes_received);
 *     }
 *     if (buf_len(b) > 100) break;
 * }
 *
 */

#include <stdlib.h>

/* a struct for buffering outgoing data */
struct buffer
{
    char *buf;
    char *head;
    char *tail;
    unsigned int size;
};

#define BUF_LEN(x) (x->tail - x->head)
#define BUF_FREE_SPACE(x) ((x->buf + x->size) - x->tail)

struct buffer *buf_init(unsigned int size);
int buf_read(struct buffer *b, char *dest, size_t n);
void buf_flush(struct buffer *b);
int buf_write(struct buffer *b, char *src, size_t n);
void buf_free(struct buffer *b);

#endif

#include <stdio.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/poll.h>
#include <errno.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include "utils.h"
#include "tpoll.h"

#define MAXFDS 100
#define BUFSIZE 256
#define MAX_EVENTS 100

void escape_string(char *dest, char *src)
{
    /* escape special characters in src -> dest.
    *  note: dest should be malloc'd to atleast 2x the size of src.
    *  from http://stackoverflow.com/q/3535023 */
    char c;

    while ((c = *(src++))) {
        switch (c) {
            case '\a':
                *(dest++) = '\\';
                *(dest++) = 'a';
                break;
            case '\b':
                *(dest++) = '\\';
                *(dest++) = 'b';
                break;
            case '\t':
                *(dest++) = '\\';
                *(dest++) = 't';
                break;
            case '\n':
                *(dest++) = '\\';
                *(dest++) = 'n';
                break;
            case '\v':
                *(dest++) = '\\';
                *(dest++) = 'v';
                break;
            case '\f':
                *(dest++) = '\\';
                *(dest++) = 'f';
                break;
            case '\r':
                *(dest++) = '\\';
                *(dest++) = 'r';
                break;
            case '\\':
                *(dest++) = '\\';
                *(dest++) = '\\';
                break;
            case '\"':
                *(dest++) = '\\';
                *(dest++) = '\"';
                break;
            default:
                *(dest++) = c;
        }
    }

    *dest = '\0';
}

/* a struct for buffering outgoing data */
struct buffer
{
    char *buf;
    char *head;
    char *tail;
    unsigned int size;
};

#define BYTES(x) (x.tail - x.head)

void flush(struct buffer *b)
{
    /* move the buffer to the beginning of the array. */
    unsigned int tmp = (b->tail - b->head);
    memmove(b->buf, b->head, tmp);
    b->head = b->buf;
    b->tail -= tmp;
}

void malloc_buffer(struct buffer *b, unsigned int size)
{
    /* malloc a buffer with `size` elements */
    b->buf = malloc(size);
    b->head = b->tail = b->buf;
    b->size = size;
}

void free_buffer(struct buffer *b)
{
    free(b->buf);
    b->head = b->tail = NULL;
    b->size = -1;
}

int main(void)
{
    int sockfd;
    //int dispatchfd;

    if ((sockfd = setup_listen_socket("3490",10)) < 0) {
        fprintf(stderr, "failed to setup listening socket\n");
        exit(1);
    }

    /* connector's address info */
    struct sockaddr_storage their_addr;
    /* file descriptor to new connection */
    int new_fd;
    /* connector's ip address */
    char s[INET6_ADDRSTRLEN];
    char rbuf[BUFSIZE];
    char pbuf[BUFSIZE*2];
    int size;
    /* struct pollfd ufds[MAXFDS]; */
    struct tpoll *p = tpoll_create();
    struct tpoll_event ev, evs[MAX_EVENTS];
    struct buffer bufs[MAXFDS];
    int i;
    int rv, nfds;
    /* time pointer for status */
    time_t t;
    struct tm *timeinfo;
    char timestr[256];

    /* need to check return value here */
    tpoll_add(p, sockfd, POLLIN);

    printf("waiting for connections...\n");

    while (1) {

        /* timeout after 10 seconds */
        nfds = tpoll_poll(p, evs, MAX_EVENTS, 10000);

        if (nfds == -1) {
            perror("poll");
            continue;
        } 
            
        if (nfds == 0) {
            /* poll() timeout */
            t = time(NULL);
            timeinfo = localtime(&t);

            if (timeinfo == NULL) {
                perror("localtime");
            } else if (strftime(timestr, sizeof(timestr),
                "%F %T", timeinfo) == 0) {
                fprintf(stderr, "strftime returned 0\n");
            } else {
                printf("%s - %i client(s) connected\n", timestr, p->nfds-1);
            }
            continue;
        }

        for (i = 0; i < nfds; i++) {
            ev = evs[i];

            if (ev.fd == sockfd) {
                if (ev.events & POLLIN) {
                    /* client connected */

                    socklen_t sin_size = sizeof their_addr;

                    new_fd = accept(sockfd, (struct sockaddr *)&their_addr,
                                    &sin_size);
                    if (new_fd == -1) {
                        perror("accept");
                        continue;
                    }

                    inet_ntop(their_addr.ss_family,
                        get_in_addr((struct sockaddr *)&their_addr),
                        s, sizeof s);
                    printf("server: got connection from %s\n", s);

                        
                    if (tpoll_add(p, new_fd, POLLIN)) {
                        fprintf(stderr, "too many clients\n");
                        close(new_fd);
                    } else {
                        /* successfully added */
                        if (new_fd > MAXFDS-1) {
                            fprintf(stderr, "fd is > MAXFD\n");
                            tpoll_del(p, new_fd);
                            close(new_fd);
                        } else {
                            malloc_buffer(&bufs[new_fd],BUFSIZE);
                        }
                    }
                } else {
                    fprintf(stderr, "Listening socket got %i event",ev.events);
                }
                continue;
            }

            /* other sockets */
            if ((ev.events & POLLERR) || (ev.events & POLLHUP)) {
                fprintf(stderr,"received POLLERR/POLLHUP, closing socket\n");
                /* close socket */
                close(ev.fd);
                /* free buffer */
                free_buffer(&bufs[ev.fd]);
                /* delete fd from tpoll */
                tpoll_del(p,ev.fd);
                continue;
            }
            if (ev.events & POLLNVAL) {
                fprintf(stderr, "received POLLNVAL, deleting socket\n");
                /* shouldn't close socket, but need to remove it from
                 * pollfds.
                 * see stackoverflow.com/q/24791625 */

                /* free buffer */
                free_buffer(&bufs[ev.fd]);
                /* delete fd from tpoll */
                tpoll_del(p,ev.fd);
                continue;
            }
            if (ev.events & POLLIN) {
                /* data ready from client */

                printf("recv from %i\n",ev.fd);
                size = recv(ev.fd, rbuf, sizeof rbuf, 0);

                if (size == -1) {
                    if ((errno == EAGAIN) || (errno == EWOULDBLOCK)) {
                        /* no data */
                    } else {
                        perror("recv");
                    }
                } else if (size == 0) {
                    /* client disconnected */
                    printf("client disconnected\n");
                    close(ev.fd);
                    /* free buffer */
                    free_buffer(&bufs[ev.fd]);
                    /* delete fd from tpoll */
                    tpoll_del(p,ev.fd);
                    continue;
                } else {
                    rbuf[size] = '\0';
                    escape_string(pbuf,rbuf);
                    int len = strlen(pbuf);
                    pbuf[len++] = '\n';
                    pbuf[len] = '\0';
                    printf("received: %s", pbuf);

                    /* move data back to beginning of buffer */
                    flush(&bufs[ev.fd]);

                    if (strlen(pbuf) > (bufs[ev.fd].size - BYTES(bufs[ev.fd]))) {
                        /* not enough space left */
                        socklen_t sin_size = sizeof their_addr;

                        rv = getpeername(ev.fd,
                            (struct sockaddr *)&their_addr, &sin_size);
                        if (rv == -1) {
                            perror("getpeername");
                            s[0] = '?';
                            s[1] = '\0';
                        } else {
                            /* get ip address */
                            inet_ntop(their_addr.ss_family,
                                get_in_addr((struct sockaddr *)&their_addr),
                                s, sizeof s);
                        }
                        fprintf(stderr, "ERROR: output buffer full for %s\n",s);
                    } else {
                        memcpy(bufs[ev.fd].tail,pbuf,strlen(pbuf));
                        bufs[ev.fd].tail += strlen(pbuf);
                        /* need to send data */
                        tpoll_modify_or(p, ev.fd, POLLOUT);
                    }
                }
            }
            if (ev.events & POLLOUT) {
                if (BYTES(bufs[ev.fd]) > 0) {
                    int sent;
                    sent = send(ev.fd, bufs[ev.fd].head, BYTES(bufs[ev.fd]), 0);

                    if (sent == -1) {
                        perror("send");
                    } else {
                        bufs[ev.fd].head += sent;
                    }
                }

                if (BYTES(bufs[ev.fd]) == 0) {
                    /* no more data to send */
                    tpoll_modify_and(p, ev.fd, ~POLLOUT);
                    /* reset head and tail pointers to beginning
                     * of buffer */
                    bufs[ev.fd].head = bufs[ev.fd].tail = bufs[ev.fd].buf;
                }
            }
        }
    }

    tpoll_free(p);

    return 0;
}

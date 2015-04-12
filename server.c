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
#include "buf.h"

/* send/receive buffers for sockets are indexed by the file descriptor.
 * The buffers are accessed by indexing a fixed length array so we have
 * a maximum file descriptor */
#define MAXFD 100
/* buffer size for the send/recv buffers */
#define BUFSIZE 256
/* maximum number of file descriptor events for poll() */
#define MAX_EVENTS 100

int main(void)
{
    int sockfd;

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
    /* polling object */
    struct tpoll *p = tpoll_create();
    struct tpoll_event ev, evs[MAX_EVENTS];
    /* send/recv buffers */
    struct buffer sbuf[MAXFD];
    struct buffer rbuf[MAXFD];
    /* temporary buffer */
    char tmpbuf[BUFSIZE*2];
    int i, nfds;
    /* time pointer for status */
    time_t t;
    struct tm *timeinfo;
    char timestr[256];
    /* timespec for printing info every 10 seconds */
    struct timespec time_last, time_now;
    int size;

    clock_gettime(CLOCK_MONOTONIC, &time_now);
    time_last = time_now;

    /* need to check return value here */
    tpoll_add(p, sockfd, POLLIN);

    printf("waiting for connections...\n");

    while (1) {

        /* timeout after 10 seconds */
        nfds = tpoll_poll(p, evs, MAX_EVENTS, 1000);

        clock_gettime(CLOCK_MONOTONIC, &time_now);

        if (nfds == -1) {
            perror("poll");
            continue;
        } 
            
        if (time_now.tv_sec > time_last.tv_sec + 10) {
            time_last = time_now;
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

        /* poll() timeout */
        if (nfds == 0) continue;

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
                        if (new_fd > MAXFD-1) {
                            fprintf(stderr, "fd is > MAXFD\n");
                            tpoll_del(p, new_fd);
                            close(new_fd);
                        } else {
                            buf_create(&rbuf[new_fd],BUFSIZE);
                            buf_create(&sbuf[new_fd],BUFSIZE);
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
                /* free buffers */
                buf_free(&rbuf[ev.fd]);
                buf_free(&sbuf[ev.fd]);
                /* delete fd from tpoll */
                tpoll_del(p,ev.fd);
                continue;
            }
            if (ev.events & POLLNVAL) {
                fprintf(stderr, "received POLLNVAL, deleting socket\n");
                /* shouldn't close socket, but need to remove it from
                 * pollfds.
                 * see stackoverflow.com/q/24791625 */

                /* free buffers */
                buf_free(&rbuf[ev.fd]);
                buf_free(&sbuf[ev.fd]);
                /* delete fd from tpoll */
                tpoll_del(p,ev.fd);
                continue;
            }
            if (ev.events & POLLIN) {
                /* data ready from client */
#ifdef DEBUG
                printf("recv from %i\n",ev.fd);
#endif
                /* todo: check free_space and flush() if needed */
                buf_flush(&rbuf[ev.fd]);
                size = recv(ev.fd, rbuf[ev.fd].tail, FREE_SPACE(rbuf[ev.fd]), 0);

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
                    /* free buffers */
                    buf_free(&rbuf[ev.fd]);
                    buf_free(&sbuf[ev.fd]);
                    /* delete fd from tpoll */
                    tpoll_del(p,ev.fd);
                    continue;
                } else {
                    rbuf[ev.fd].tail += size;
                }
            }
            if (ev.events & POLLOUT) {
                if (BYTES(sbuf[ev.fd]) > 0) {
                    int sent;
                    sent = send(ev.fd, sbuf[ev.fd].head, BYTES(sbuf[ev.fd]), 0);

                    if (sent == -1) {
                        perror("send");
                    } else {
                        sbuf[ev.fd].head += sent;
                    }
                }

                if (BYTES(sbuf[ev.fd]) == 0) {
                    /* no more data to send */
                    tpoll_modify_and(p, ev.fd, ~POLLOUT);
                    /* reset head and tail pointers to beginning
                     * of buffer */
                    sbuf[ev.fd].head = sbuf[ev.fd].tail = sbuf[ev.fd].buf;
                }
            }
            /* basic I/O done, now check for messages */
            if (ev.events & POLLIN) {
                /* check for data */
                int n = buf_read(&rbuf[ev.fd],tmpbuf,BYTES(rbuf[ev.fd]));
                if (n < 0) {
                    fprintf(stderr,"buf_read failed\n");
                }
                tmpbuf[n] = '\0';
                printf("recv: %s",tmpbuf);

                char printbuf[BUFSIZE*2];
                escape_string(printbuf,tmpbuf);
                int len = strlen(printbuf);
                printbuf[len++] = '\n';
                printbuf[len] = '\0';
                printf("received: %s", printbuf);

                if (buf_write(&sbuf[ev.fd],printbuf,strlen(printbuf)) < 0) {
                    socklen_t sin_size = sizeof their_addr;

                    if (getpeername(ev.fd, (struct sockaddr *)&their_addr,
                                    &sin_size)) {
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
                    /* need to send data */
                    tpoll_modify_or(p, ev.fd, POLLOUT);
                }
            }
        }
    }

    tpoll_free(p);

    return 0;
}

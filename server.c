#include <stdio.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/poll.h>
#include <errno.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <signal.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <fcntl.h>
#include "utils.h"
#include "tpoll.h"
#include "buf.h"
#include "intset.h"

static volatile int go = 1;

void ctrlc_handler(int _) {
    go = 0;
}

/* send/receive buffers for sockets are indexed by the file descriptor.
 * The buffers are accessed by indexing a fixed length array so we have
 * a maximum file descriptor */
#define MAXFD 100
/* buffer size for the send/recv buffers */
#define BUFSIZE 256
/* maximum number of file descriptor events for poll() */
#define MAX_EVENTS 100

int set_nonblocking(int fd)
{
    /* set a socket to non-blocking mode.
     * from www.kegel.com/dkftpbench/nonblocking.html */
    int flags;

    /* If they have O_NONBLOCK, use the POSIX way to do it */
#if defined(O_NONBLOCK)
    if (-1 == (flags = fcntl(fd, F_GETFL, 0)))
        flags = 0;
    return fcntl(fd, F_SETFL, flags | O_NONBLOCK);
#else
    /* otherwise, use the old way of doing it */
    flags = 1;
    return ioctl(fd, FIOBIO, &flags);
#endif
}

int main(void)
{
    signal(SIGINT, ctrlc_handler);

    int sockfd;
    int dispfd;

    struct intset *dispset = intset_init(MAXFD);
    struct intset *sockset = intset_init(MAXFD);

    if ((sockfd = setup_listen_socket("3490",10)) < 0) {
        fprintf(stderr, "failed to setup listening socket on port 3490\n");
        exit(1);
    }

    set_nonblocking(sockfd);

    if ((dispfd = setup_listen_socket("3491",10)) < 0) {
        fprintf(stderr, "failed to setup listening socket on port 3491\n");
        exit(1);
    }

    set_nonblocking(dispfd);

    /* connector's address info */
    struct sockaddr_storage their_addr;
    /* file descriptor to new connection */
    int new_fd;
    /* connector's ip address */
    char s[INET6_ADDRSTRLEN];
    /* polling object */
    struct tpoll *p = tpoll_init();
    struct tpoll_event ev, evs[MAX_EVENTS];
    /* send/recv buffers */
    struct buffer sbuf[MAXFD];
    struct buffer rbuf[MAXFD];
    /* temporary buffer */
    char tmpbuf[BUFSIZE*2];
    /* time pointer for status */
    time_t t;
    struct tm *timeinfo;
    char timestr[256];
    /* timespec for printing info every 10 seconds */
    struct timespec time_last, time_now;

    int i, nfds, bytes;

    clock_gettime(CLOCK_MONOTONIC, &time_now);
    time_last = time_now;

    /* need to check return value here */
    tpoll_add(p, sockfd, POLLIN);
    tpoll_add(p, dispfd, POLLIN);

    printf("waiting for connections...\n");

    while (go) {

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
                printf("%s - %i client(s) connected\n", timestr, p->entries-2);
            }
            continue;
        }

        /* poll() timeout */
        if (nfds == 0) continue;

        for (i = 0; i < nfds; i++) {
            ev = evs[i];

            if (ev.fd == sockfd || ev.fd == dispfd) {
                /* client connected */
                fprintf(stderr, "client connected\n");

                socklen_t sin_size = sizeof their_addr;

                new_fd = accept(ev.fd, (struct sockaddr *)&their_addr,
                                &sin_size);

                set_nonblocking(new_fd);

                if (new_fd == -1) {
                    perror("accept");
                    continue;
                }

                inet_ntop(their_addr.ss_family,
                    get_in_addr((struct sockaddr *)&their_addr),
                    s, sizeof s);
                printf("server: got connection from %s\n", s);

                if (tpoll_add(p, new_fd, POLLIN)) {
                    fprintf(stderr, "tpoll_add() failed\n");
                    close(new_fd);
                    continue;
                }

                if (new_fd > MAXFD-1) {
                    fprintf(stderr, "fd is > MAXFD\n");
                    tpoll_del(p, new_fd);
                    close(new_fd);
                    continue;
                }

                if (intset_add(sockset,new_fd) != 0) {
                    fprintf(stderr, "failed to add fd to intset\n");
                    tpoll_del(p, new_fd);
                    close(new_fd);
                    continue;
                }

                /* create send/recv buffers */
                buf_create(&rbuf[new_fd],BUFSIZE);
                buf_create(&sbuf[new_fd],BUFSIZE);

                if (ev.fd == dispfd) {
                    if (intset_add(dispset,new_fd) != 0) {
                        fprintf(stderr, "failed to add fd to intset\n");
                        tpoll_del(p, new_fd);
                        close(new_fd);
                        buf_free(&rbuf[new_fd]);
                        buf_free(&sbuf[new_fd]);
                        continue;
                    }
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
                /* remove from intset */
                intset_del(dispset, ev.fd);
                intset_del(sockset, ev.fd);
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
                /* remove from intset */
                intset_del(dispset, ev.fd);
                intset_del(sockset, ev.fd);
                continue;
            }
            if (ev.events & POLLIN) {
                /* data ready from client */
#ifdef DEBUG
                printf("recv from %i\n",ev.fd);
#endif
                /* todo: check free_space and flush() if needed */
                buf_flush(&rbuf[ev.fd]);
                bytes = recv(ev.fd, rbuf[ev.fd].tail, FREE_SPACE(rbuf[ev.fd]), 0);

                if (bytes == -1) {
                    if ((errno == EAGAIN) || (errno == EWOULDBLOCK)) {
                        /* no data */
                    } else {
                        perror("recv");
                    }
                } else if (bytes == 0) {
                    /* client disconnected */
                    printf("client disconnected\n");
                    close(ev.fd);
                    /* free buffers */
                    buf_free(&rbuf[ev.fd]);
                    buf_free(&sbuf[ev.fd]);
                    /* delete fd from tpoll */
                    tpoll_del(p,ev.fd);
                    /* remove from intset */
                    intset_del(dispset, ev.fd);
                    intset_del(sockset, ev.fd);
                    continue;
                } else {
                    rbuf[ev.fd].tail += bytes;
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

                /* relay to all dispatchers */
                int j;
                for (j = 0; j < dispset->entries; j++) {
                    int fd = dispset->values[j];
                    if (buf_write(&sbuf[fd],printbuf,strlen(printbuf)) < 0) {
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
                        tpoll_modify_or(p, fd, POLLOUT);
                    }
                } /* for loop for dispatchers */
            }
        }
    }

    /* free remaining buffers */
    for (i = 0; i < sockset->entries; i++) {
        buf_free(&rbuf[sockset->values[i]]);
        buf_free(&sbuf[sockset->values[i]]);
    }

    tpoll_free(p);
    intset_free(dispset);
    intset_free(sockset);

    return 0;
}

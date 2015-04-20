#include <stdio.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/epoll.h>
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
#include "ptrset.h"

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

typedef enum sock_type {
    CLIENT,
    CLIENT_LISTEN,
    DISPATCH,
    DISPATCH_LISTEN,
    XL3_LISTEN,
    XL3,
    XL3_ORCA,
} sock_type_t;

struct sock {
    int fd;
    sock_type_t type;
    int id;
    struct buffer *rbuf;
    struct buffer *sbuf;
};

struct sock *sock_init(int fd, sock_type_t type, int id)
{
    struct sock *s = malloc((sizeof (struct sock)));
    s->fd = fd;
    s->type = type;
    s->id = id;
    s->rbuf = buf_init(BUFSIZE);
    s->sbuf = buf_init(BUFSIZE);
    return s;
}

void sock_free(struct sock *s)
{
    buf_free(s->rbuf);
    buf_free(s->sbuf);
    free(s);
}

int main(void)
{
    signal(SIGINT, ctrlc_handler);

    /* prevent SIGPIPE from crashing the program */
    signal(SIGPIPE, SIG_IGN);

    int sockfd;
    int dispfd;

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
    char ipaddr[INET6_ADDRSTRLEN];
    /* polling object */
    int epollfd, nfds;
    struct epoll_event ev, events[MAX_EVENTS];
    /* temporary buffer */
    char tmpbuf[BUFSIZE*2];
    /* time pointer for status */
    time_t t;
    struct tm *timeinfo;
    char timestr[256];
    /* timespec for printing info every 10 seconds */
    struct timespec time_last, time_now;

    int i, bytes;

    clock_gettime(CLOCK_MONOTONIC, &time_now);
    time_last = time_now;

    epollfd = epoll_create(10);
    if (epollfd == -1) {
        perror("epoll_create");
        exit(1);
    }

    struct ptrset *dispset = ptrset_init();
    struct ptrset *sockset = ptrset_init();

    struct sock *s;

    ev.events = EPOLLIN;
    ev.data.ptr = sock_init(sockfd, CLIENT_LISTEN, 0);
    if (epoll_ctl(epollfd, EPOLL_CTL_ADD, sockfd, &ev) == -1) {
        perror("epoll_ctl: listen_sock");
        exit(1);
    }
    ptrset_add(sockset, ev.data.ptr);

    ev.events = EPOLLIN;
    ev.data.ptr = sock_init(dispfd, DISPATCH_LISTEN, 0);
    if (epoll_ctl(epollfd, EPOLL_CTL_ADD, dispfd, &ev) == -1) {
        perror("epoll_ctl: dispfd");
        exit(1);
    }
    ptrset_add(sockset, ev.data.ptr);

    printf("waiting for connections...\n");

    while (go) {

        /* timeout after 1 second */
        nfds = epoll_wait(epollfd, events, MAX_EVENTS, 1000);

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
                printf("%s - %i client(s) connected\n", timestr, sockset->entries-2);
            }
            continue;
        }

        /* poll() timeout */
        if (nfds == 0) continue;

        for (i = 0; i < nfds; i++) {
            s = (struct sock*)events[i].data.ptr;

            if (s->type == CLIENT_LISTEN || s->type == DISPATCH_LISTEN) {
                /* client connected */
                fprintf(stderr, "client connected\n");

                socklen_t sin_size = sizeof their_addr;

                new_fd = accept(s->fd, (struct sockaddr *)&their_addr,
                                &sin_size);

                set_nonblocking(new_fd);

                if (new_fd == -1) {
                    perror("accept");
                    continue;
                }

                inet_ntop(their_addr.ss_family,
                    get_in_addr((struct sockaddr *)&their_addr),
                    ipaddr, sizeof ipaddr);
                printf("server: got connection from %s, fd=%d\n", ipaddr,new_fd);

                struct sock *new_sock;
                if (s->type == CLIENT_LISTEN) {
                    new_sock = sock_init(new_fd, CLIENT, 0);
                } else if (s->type == DISPATCH_LISTEN) {
                    new_sock = sock_init(new_fd, DISPATCH, 0);
                }

                ev.events = EPOLLIN;
                ev.data.ptr = new_sock;
                if (epoll_ctl(epollfd, EPOLL_CTL_ADD, new_fd, &ev) == -1) {
                    perror("epoll_ctl");
                    sock_free(new_sock);
                    close(new_fd);
                }

                ptrset_add(sockset, ev.data.ptr);
                if (s->type == DISPATCH_LISTEN) {
                    ptrset_add(dispset, ev.data.ptr);
                }
                continue;
            }

            /* other sockets */
            if ((events[i].events & EPOLLERR) || \
                (events[i].events & EPOLLHUP)) {
                fprintf(stderr,"received EPOLLERR/EPOLLHUP, closing socket\n");
                /* free buffers */
                sock_free(s);
                /* close socket */
                close(s->fd);
                /* delete from sets */
                ptrset_del(sockset, ev.data.ptr);
                ptrset_del(dispset, ev.data.ptr);
                continue;
            }
            if (events[i].events & EPOLLHUP) {
                fprintf(stderr, "received EINVAL, deleting socket\n");
                /* shouldn't close socket, but need to remove it from
                 * pollfds.
                 * see stackoverflow.com/q/24791625 */

                /* delete from epoll */
                if (epoll_ctl(epollfd, EPOLL_CTL_DEL, s->fd, NULL) == -1) {
                    perror("epoll_ctl: EPOLL_CTL_DEL");
                }
                /* free buffers */
                sock_free(s);
                /* delete from sets */
                ptrset_del(sockset, ev.data.ptr);
                ptrset_del(dispset, ev.data.ptr);
                continue;
            }
            if (events[i].events & EPOLLIN) {
                /* data ready from client */
                printf("recv from %i\n",s->fd);
                bytes = recv(s->fd, tmpbuf, sizeof tmpbuf, 0);

                if (bytes > 0) {
                    /* success! */
                    if (buf_write(s->rbuf, tmpbuf, bytes)) {
                        /* todo: need to do something here
                         * disconnect? */
                        fprintf(stderr, "ERROR: read buffer overflow!\n");
                    }
                } else if (bytes == -1) {
                    perror("recv");
                } else if (bytes == 0) {
                    /* client disconnected */
                    printf("client disconnected\n");
                    /* free buffers */
                    sock_free(s);
                    /* close socket */
                    close(s->fd);
                    /* delete from sets */
                    ptrset_del(sockset, ev.data.ptr);
                    ptrset_del(dispset, ev.data.ptr);
                    continue;
                }
            }
            if (events[i].events & EPOLLOUT) {
                if (BUF_LEN(s->sbuf) > 0) {
                    int sent;
                    sent = send(s->fd, s->sbuf->head, BUF_LEN(s->sbuf), 0);

                    if (sent == -1) {
                        perror("send");
                    } else {
                        s->sbuf->head += sent;
                    }
                }

                if (BUF_LEN(s->sbuf) == 0) {
                    /* no more data to send */
                    ev.events = EPOLLIN;
                    ev.data.ptr = s;
                    if (epoll_ctl(epollfd, EPOLL_CTL_MOD, s->fd, &ev) == -1) {
                        perror("epoll_ctl: mod");
                    }
                    /* reset head and tail pointers to beginning
                     * of buffer */
                    s->sbuf->head = s->sbuf->tail = s->sbuf->buf;
                }
            }
            /* basic I/O done, now check for messages.
             * note: you have to consume as much of the recv buffer here as
             * possible because otherwise if there are no POLL events for this
             * fd, the control flow will never come back here. */
            if (events[i].events & EPOLLIN) {
                /* check for data */
                int n = buf_read(s->rbuf,tmpbuf,BUF_LEN(s->rbuf));
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
                    s = (struct sock *)dispset->values[j];
                    if (buf_write(s->sbuf,printbuf,strlen(printbuf)) < 0) {
                        socklen_t sin_size = sizeof their_addr;

                        if (getpeername(s->fd, (struct sockaddr *)&their_addr,
                                        &sin_size)) {
                            perror("getpeername");
                            ipaddr[0] = '?';
                            ipaddr[1] = '\0';
                        } else {
                            /* get ip address */
                            inet_ntop(their_addr.ss_family,
                                get_in_addr((struct sockaddr *)&their_addr),
                                ipaddr, sizeof ipaddr);
                        }
                        fprintf(stderr, "ERROR: output buffer full for %s\n",ipaddr);
                    } else {
                        /* need to send data */
                        ev.events = EPOLLIN | EPOLLOUT;
                        ev.data.ptr = s;
                        fprintf(stderr, "s->fd = %d\n",s->fd);
                        if (epoll_ctl(epollfd, EPOLL_CTL_MOD, s->fd, &ev) == -1) {
                            perror("epoll_ctl: dispatch send");
                        }
                    }
                } /* for loop for dispatchers */
            }
        }
    }

    /* free remaining buffers */
    for (i = 0; i < sockset->entries; i++) {
        s = (struct sock*)sockset->values[i];
        sock_free(s);
    }

    ptrset_free(dispset);
    ptrset_free(sockset);

    return 0;
}

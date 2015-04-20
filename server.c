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
#include "XL3PacketTypes.h"

static volatile int go = 1;

void ctrlc_handler(int _) {
    go = 0;
}

/* buffer size for the send/recv buffers */
#define BUFSIZE 1000000
/* maximum number of file descriptor events for poll() */
#define MAX_EVENTS 100
/* XL3's connect on port XL3_PORT + crate */
#define XL3_PORT 44630

typedef enum sock_type {
    CLIENT,
    CLIENT_LISTEN,
    DISPATCH,
    DISPATCH_LISTEN,
    XL3_LISTEN,
    XL3,
    XL3_ORCA,
} sock_type_t;

struct xl3_cmd {
    XL3Packet msg;
    struct sock *sender;
};

struct sock {
    int fd;
    sock_type_t type;
    int id;
    struct buffer *rbuf;
    struct buffer *sbuf;
    /* queue for commands -> XL3 */
    struct ptrset *cmdqueue;
    /* last sent command */
    struct xl3_cmd *cmd;
};

struct sock *sock_init(int fd, sock_type_t type, int id)
{
    struct sock *s = malloc((sizeof (struct sock)));
    s->fd = fd;
    s->type = type;
    s->id = id;
    s->rbuf = buf_init(BUFSIZE);
    s->sbuf = buf_init(BUFSIZE);
    s->cmdqueue = ptrset_init();
    s->cmd = NULL;
    return s;
}

void sock_free(struct sock *s)
{
    buf_free(s->rbuf);
    buf_free(s->sbuf);
    ptrset_free(s->cmdqueue);
    free(s);
}

void relay_to_dispatchers(struct ptrset *dispset, char *buf, int size, int epollfd)
{
    /* relay the message in `buf` to all dispatchers */
    struct epoll_event ev;
    int i;
    for (i = 0; i < dispset->entries; i++) {
        struct sock *s = (struct sock *)dispset->values[i];

        if (buf_write(s->sbuf, buf, size) < 0) {
            fprintf(stderr, "ERROR: dispatcher send buffer full\n");
            continue;
        }

        ev.events = EPOLLIN | EPOLLOUT;
        ev.data.ptr = s;
        if (epoll_ctl(epollfd, EPOLL_CTL_MOD, s->fd, &ev) == -1) {
            perror("epoll_ctl");
        }
    }
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
    char *tmpbuf = malloc(BUFSIZE*2);
    char *printbuf = malloc(BUFSIZE*2);
    /* time pointer for status */
    time_t t;
    struct tm *timeinfo;
    char timestr[256];
    /* timespec for printing info every 10 seconds */
    struct timespec time_last, time_now;

    int i, j, k, n, bytes;

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

    for (i = 0; i < 20; i++) {
        char port[256];
        sprintf(port, "%d", XL3_PORT+i);
        if ((new_fd = setup_listen_socket(port,1)) < 0) {
            fprintf(stderr, "failed to setup listening socket for XL3 %d\n", i);
            exit(1);
        }

        set_nonblocking(new_fd);

        ev.events = EPOLLIN;
        ev.data.ptr = sock_init(new_fd, XL3_LISTEN, i);
        if (epoll_ctl(epollfd, EPOLL_CTL_ADD, new_fd, &ev) == -1) {
            perror("epoll_ctl");
            exit(1);
        }
        ptrset_add(sockset, ev.data.ptr);
    }

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

            if (s->type == CLIENT_LISTEN || s->type == DISPATCH_LISTEN || \
                s->type == XL3_LISTEN) {
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

                struct sock *new_sock;
                if (s->type == CLIENT_LISTEN) {
                    new_sock = sock_init(new_fd, CLIENT, 0);
                    printf("server: got connection from client %s, fd=%d\n", ipaddr,new_fd);
                } else if (s->type == DISPATCH_LISTEN) {
                    new_sock = sock_init(new_fd, DISPATCH, 0);
                    printf("server: got connection from dispatcher %s, fd=%d\n", ipaddr,new_fd);
                } else if (s->type == XL3_LISTEN) {
                    new_sock = sock_init(new_fd, XL3, 0);
                    printf("server: got connection from xl3 %s, fd=%d\n", ipaddr,new_fd);
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
                /* close socket */
                close(s->fd);
                /* delete from sets */
                ptrset_del(sockset, ev.data.ptr);
                ptrset_del(dispset, ev.data.ptr);
                /* free buffers */
                sock_free(s);
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
                /* delete from sets */
                ptrset_del(sockset, ev.data.ptr);
                ptrset_del(dispset, ev.data.ptr);
                /* free buffers */
                sock_free(s);
                continue;
            }
            if (events[i].events & EPOLLIN) {
                /* data ready from client */
                printf("recv from %i\n",s->fd);
                bytes = recv(s->fd, tmpbuf, BUFSIZE, 0);

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
                    /* close socket */
                    close(s->fd);
                    /* delete from sets */
                    ptrset_del(sockset, ev.data.ptr);
                    ptrset_del(dispset, ev.data.ptr);
                    /* free buffers */
                    sock_free(s);
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
                switch (s->type) {
                    case XL3:
                        while (BUF_LEN(s->rbuf) > XL3_PACKET_SIZE) {
                            n = buf_read(s->rbuf,tmpbuf,XL3_PACKET_SIZE);
                            if (n < 0) {
                                fprintf(stderr, "buf_read failed\n");
                            }
                            XL3Packet *p = (XL3Packet *)tmpbuf;

                            switch (p->header.packetType) {
                                case PING_ID:
                                    /* ping -> pong */
                                    p->header.packetType = PONG_ID;
                                    if (buf_write(s->sbuf,tmpbuf,XL3_PACKET_SIZE) == -1) {
                                        fprintf(stderr, "ERROR: failed to write PONG packet\n");
                                    }
                                    ev.events = EPOLLIN | EPOLLOUT;
                                    ev.data.ptr = s;
                                    if (epoll_ctl(epollfd, EPOLL_CTL_MOD, s->fd, &ev) == -1) {
                                        perror("epoll_ctl");
                                    }
                                    break;
                                case MESSAGE_ID:
                                    /* print message */
                                    printf("XL3 message: %s", p->payload);
                                    break;
                                case MEGA_BUNDLE_ID:
                                    /* dispatch */
                                    relay_to_dispatchers(dispset, tmpbuf, XL3_PACKET_SIZE, epollfd);
                                    break;
                                default:
                                    /* forward -> sender */
                                    if (s->cmd) {
                                        /* we are waiting for a reply */
                                        if (p->header.packetType == s->cmd->msg.header.packetType) {
                                            struct sock *reply_sock = s->cmd->sender;
                                            if (buf_write(reply_sock->sbuf, tmpbuf, XL3_PACKET_SIZE) == -1) {
                                                fprintf(stderr, "ERROR: send buffer full!\n");
                                            } else {
                                                free(s->cmd);
                                                s->cmd = (struct xl3_cmd *)ptrset_pop(s->cmdqueue);
                                                if (s->cmd) {
                                                    if (buf_write(s->sbuf, (char *)&(s->cmd->msg), XL3_PACKET_SIZE) < 0) {
                                                        fprintf(stderr, "ERROR: can't send XL3 packet\n");
                                                    }
                                                }
                                            }
                                        } else {
                                            fprintf(stderr, "WARNING: received packet %x from XL3, was expecting %x\n",
                                                    p->header.packetType,s->cmd->msg.header.packetType);
                                        }
                                    } else {
                                        fprintf(stderr, "WARNING: received %x packet from XL3, don't know what to do\n",
                                                p->header.packetType);
                                    }
                            }
                        } /* while */
                        break;
                    case CLIENT:
                        /* check for data */
                        n = buf_read(s->rbuf,tmpbuf,BUF_LEN(s->rbuf));
                        if (n < 0) {
                            fprintf(stderr,"buf_read failed\n");
                        }
                        tmpbuf[n] = '\0';
                        printf("recv: %s",tmpbuf);

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
                        break;
                    default:
                        fprintf(stderr, "unknown socket\n");
                } /* switch statement */
            } /* if events[i].events & POLLIN */
        } /* loop over nfds */
    } /* while(go) */

    /* free remaining buffers */
    for (i = 0; i < sockset->entries; i++) {
        s = (struct sock*)sockset->values[i];
        sock_free(s);
    }

    ptrset_free(dispset);
    ptrset_free(sockset);

    free(tmpbuf);
    free(printbuf);

    return 0;
}

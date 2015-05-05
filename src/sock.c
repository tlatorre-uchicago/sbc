#include <stdint.h>
#include <errno.h>
#include <stdio.h>
#include <sys/socket.h>
#include <unistd.h>
#include <sys/types.h>
#include <netdb.h>
#include <string.h>
#include <arpa/inet.h>
#include <assert.h>
#include "sock.h"
#include "ptrset.h"
#include "utils.h"

#ifdef __MACH__
#include "epoll.h"
#else
#include <sys/epoll.h>
#endif

#define READSIZE 1000

int global_setup()
{
    epollfd = epoll_create(10);
    if (epollfd == -1) {
        perror("epoll_create");
        return -1;
    }

    sockset = ptrset_init();
    return 0;
}

void global_free()
{
    ptrset_free(sockset);
}

struct sock *sock_init(int fd, sock_type_t type, int id, char *ip)
{
    struct sock *s = malloc((sizeof (struct sock)));
    s->fd = fd;
    s->type = type;
    s->id = id;
    strncpy(s->ip, ip, INET6_ADDRSTRLEN);
    s->rbuf = buf_init(BUFSIZE);
    s->sbuf = buf_init(BUFSIZE);
    s->req_queue = ptrset_init();
    s->req = NULL;
    ptrset_add(sockset, s);
    return s;
}

void sock_close(struct sock *s)
{
    int i, j;
    for (i = 0; i < sockset->entries; i++) {
        struct sock *xl3 = (struct sock *)sockset->values[i];
        if (xl3->req && xl3->req->sender == s) {
            xl3->req->sender = NULL;
        }
        for (j = 0; j < xl3->req_queue->entries; j++) {
            struct XL3_request *req = \
                (struct XL3_request *)xl3->req_queue->values[j];
            if (req->sender == s) {
                req->sender = NULL;
            }
        }
    }
    /* close the file descriptor */
    close(s->fd);
    /* delete the socket from epoll */
    epoll_ctl(epollfd, EPOLL_CTL_DEL, s->fd, 0);
    /* delete the socket from the global list */
    ptrset_del(sockset, s);
    /* free the sock struct */
    sock_free(s);
}

int sock_listen(int port, int backlog, int type, int id)
{
    /* set up and return a file descriptor for a listening socket
     * on port `port`.
     * `backlog` is the number of connections to queue up.
     * returns -1 on error */
    int sockfd;
    struct addrinfo hints, *servinfo, *p;
    int yes = 1;
    int rv;
    char node[256];
    struct epoll_event ev;
    sprintf(node, "%d", port);

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;

    if ((rv = getaddrinfo(NULL, node, &hints, &servinfo)) != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
        return -1;
    }

    /* loop through all the results and bind to the first we can */
    for (p = servinfo; p != NULL; p = p->ai_next) {
        if ((sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1) {
            perror("server: socket");
            continue;
        }

        if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) == -1) {
            perror("setsockopt");
            return -1;
        }

        if (bind(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
            close(sockfd);
            perror("server: bind");
            continue;
        }

        break;
    }

    if (p == NULL) {
        fprintf(stderr, "server: failed to bind\n");
        return -1;
    }

    freeaddrinfo(servinfo);

    if (listen(sockfd, backlog) == -1) {
        perror("listen");
        exit(1);
    }

    set_nonblocking(sockfd);

    ev.events = EPOLLIN;
    ev.data.ptr = sock_init(sockfd, type, id, "\0");

    if (epoll_ctl(epollfd, EPOLL_CTL_ADD, sockfd, &ev) == -1) {
        perror("epoll_ctl");
        sock_close(ev.data.ptr);
        return -1;
    }

    return sockfd;
}

void sock_accept(struct sock *s)
{
    struct epoll_event ev;
    struct sockaddr_storage their_addr;
    char ip_addr[INET6_ADDRSTRLEN];
    socklen_t sin_size = sizeof their_addr;

    int new_fd = accept(s->fd, (struct sockaddr *)&their_addr,
                        &sin_size);

    set_nonblocking(new_fd);

    if (new_fd == -1) {
        perror("accept");
        return;
    }

    inet_ntop(their_addr.ss_family, get_in_addr((struct sockaddr *)&their_addr), ip_addr, sizeof ip_addr);

    printf("got connection from %s\n", ip_addr);

    struct sock *new_sock;
    if (s->type == CLIENT_LISTEN) {
        new_sock = sock_init(new_fd, CLIENT, s->id, ip_addr);
    } else if (s->type == DISPATCH_LISTEN) {
        new_sock = sock_init(new_fd, DISPATCH, s->id, ip_addr);
    } else if (s->type == XL3_LISTEN) {
        new_sock = sock_init(new_fd, XL3, s->id, ip_addr);
    } else if (s->type == XL3_ORCA_LISTEN) {
        new_sock = sock_init(new_fd, XL3_ORCA, s->id, ip_addr);
    } else {
        fprintf(stderr, "sock_accept: unknown socket type %x", s->type);
        return;
    }

    ev.events = EPOLLIN;
    ev.data.ptr = new_sock;
    if (epoll_ctl(epollfd, EPOLL_CTL_ADD, new_fd, &ev) == -1) {
        perror("epoll_ctl");
        sock_close(new_sock);
        return;
    }
}
    
int sock_io(struct sock *s, uint32_t event)
{
    static char tmp[READSIZE];
    struct epoll_event ev;

    if (event & EPOLLERR) {
        fprintf(stderr, "received EPOLLERR, closing socket\n");
        sock_close(s);
        return -1;
    }
    if (event & EPOLLHUP) {
        fprintf(stderr, "received EPOLLHUP, closing socket\n");
        sock_close(s);
        return -1;
    }

    if (s->type == CLIENT_LISTEN || s->type == DISPATCH_LISTEN || s->type == XL3_LISTEN || s->type == XL3_ORCA_LISTEN) {
        sock_accept(s);
        return 0;
    }

    if (event & EPOLLIN) {
        int bytes = recv(s->fd, tmp, sizeof tmp, 0);

        if (bytes == -1) {
            perror("recv");
        } else if (bytes == 0) {
            printf("server: %s disconnected.\n", s->ip);
            sock_close(s);
            return -1;
        } else {
            /* success! */
            if (buf_write(s->rbuf, tmp, bytes) == -1) {
                fprintf(stderr, "ERROR: read buffer overflow, closing socket!\n");
                sock_close(s);
                return -1;
            }
        }
    }
    if (event & EPOLLOUT) {
        if (BUF_LEN(s->sbuf) > 0) {
            int sent = send(s->fd, s->sbuf->head, BUF_LEN(s->sbuf), 0);

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
                perror("epoll_ctl");
            }
            /* reset head and tail pointers to beginning of buffer */
            s->sbuf->head = s->sbuf->tail = s->sbuf->buf;
        }
    }
    return 0;
}

void sock_write(struct sock *s, char *buf, int size)
{
    struct epoll_event ev;

    if (buf_write(s->sbuf, buf, size) == -1) {
        fprintf(stderr, "ERROR: output buffer full!\n");
        return;
    }

    ev.events = EPOLLIN | EPOLLOUT;
    ev.data.ptr = s;
    if (epoll_ctl(epollfd, EPOLL_CTL_MOD, s->fd, &ev) == -1) {
        perror("epoll_ctl");
    }
}

void sock_free(struct sock *s)
{
    buf_free(s->rbuf);
    buf_free(s->sbuf);
    ptrset_free_all(s->req_queue);
    if (s->req) free(s->req);
    free(s);
}

void relay_to_dispatchers(char *msg, uint16_t size, uint16_t type)
{
    /* relay the message in `buf` to all dispatchers */
    static char buf[10000];

    assert(size < 10000);

    buf[0] = (size >> 8) & 0xff;
    buf[1] = size & 0xff;
    buf[2] = (type >> 8) & 0xff;
    buf[3] = type & 0xff;
    memcpy(buf+4, msg, size);

    int i;
    for (i = 0; i < sockset->entries; i++) {
        struct sock *s = (struct sock *)sockset->values[i];

        if (s->type != DISPATCH) continue;

        sock_write(s, buf, size + 4);
    }
}

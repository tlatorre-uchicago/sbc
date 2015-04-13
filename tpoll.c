#include <stdlib.h>
#include "tpoll.h"

struct tpoll *tpoll_init() {
    /* create and return a pointer to a poll object */
    struct tpoll *p = malloc(sizeof (struct tpoll));
    p->ufds = malloc(0);
    p->size = 0;
    p->entries = 0;
    return p;
}

int tpoll_in(struct tpoll *p, int fd)
{
    /* returns 1 if the file descriptor is registered, otherwise zero. */
    int i;
    for (i = 0; i < p->entries; i++) {
        if (p->ufds[i].fd == fd) return 1;
    }
    return 0;
}


void tpoll_free(struct tpoll *p) {
    /* free tpoll object */
    free(p->ufds);
    free(p);
}

int tpoll_modify(struct tpoll *p, int fd, uint32_t eventmask) {
    /* modify a file descriptors event mask */
    int i;
    for (i = 0; i < p->entries; i++) {
        if (p->ufds[i].fd == fd) {
            p->ufds[i].events = eventmask;
            return 0;
        }
    }
    return -1;
}

int tpoll_modify_or(struct tpoll *p, int fd, uint32_t eventmask) {
    /* modify a file descriptors event mask by |'ing with the current mask*/
    int i;
    for (i = 0; i < p->entries; i++) {
        if (p->ufds[i].fd == fd) {
            p->ufds[i].events |= eventmask;
            return 0;
        }
    }
    return -1;
}

int tpoll_modify_and(struct tpoll *p, int fd, uint32_t eventmask) {
    /* modify a file descriptors event mask by &'ing with the current mask*/
    int i;
    for (i = 0; i < p->entries; i++) {
        if (p->ufds[i].fd == fd) {
            p->ufds[i].events &= eventmask;
            return 0;
        }
    }
    return -1;
}

int tpoll_add(struct tpoll *p, int fd, uint32_t eventmask) {
    /* register a new file descriptor */
    if (tpoll_in(p, fd)) return -1;
    if (p->entries == p->size) {
        /* resize array. inspired by python's list implementation */
        p->size += ((p->size + 1) >> 3) + ((p->size + 1) < 9 ? 3 : 6);
        p->ufds = realloc(p->ufds, (sizeof(struct pollfd))*p->size);
    }
    p->ufds[p->entries++] = (struct pollfd){ fd, eventmask, 0 };
    return 0;
}

int tpoll_del(struct tpoll *p, int fd) {
    /* delete a file descriptor from the polling object */
    int i;
    for (i = 0; i < p->entries; i++) {
        if (p->ufds[i].fd == fd) {
            memmove(p->ufds+i, p->ufds+i+1,
                    (p->entries - i - 1)*(sizeof (struct pollfd)));
            p->entries -= 1;
            return 0;
        }
    }
    return -1;
}

int tpoll_poll(struct tpoll *p, struct tpoll_event *evs, int maxevents,
            int timeout) {
    /* poll file descriptors and return the number of fds
     * with events */
    int rv, i, n = 0;
    rv = poll(p->ufds, p->entries, timeout);
    if (rv <= 0) return rv;

    for (i = 0; i < p->entries; i++) {
        if (p->ufds[i].revents) {
            evs[n++] = (struct tpoll_event) { p->ufds[i].fd, p->ufds[i].revents };
            if (n == maxevents) break;
        }
    }
    return n;
}


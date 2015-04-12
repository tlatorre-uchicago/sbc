#include <stdlib.h>
#include "tpoll.h"

struct tpoll *tpoll_create() {
    /* create a poll object */
    struct tpoll *p = malloc(sizeof (struct tpoll));
    p->nfds = 0;
}

void tpoll_free(struct tpoll *p) {
    /* free tpoll object */
    free(p);
}

int tpoll_modify(struct tpoll *p, int fd, uint32_t eventmask) {
    /* modify a file descriptors event mask */
    int i;
    for (i = 0; i < p->nfds; i++) {
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
    for (i = 0; i < p->nfds; i++) {
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
    for (i = 0; i < p->nfds; i++) {
	if (p->ufds[i].fd == fd) {
	    p->ufds[i].events &= eventmask;
	    return 0;
	}
    }
    return -1;
}

int tpoll_add(struct tpoll *p, int fd, uint32_t eventmask) {
    /* register a new file descriptor */
    if (p->nfds == MAX_FDS) {
	/* already have maximum fds */
	return -1;
    }
    p->ufds[p->nfds].fd = fd;
    p->ufds[p->nfds].events = eventmask;
    p->ufds[p->nfds].revents = 0;
    p->nfds++;
    return 0;
}

int tpoll_del(struct tpoll *p, int fd) {
    /* delete a file descriptor from the polling object */
    int i;
    for (i = 0; i < p->nfds; i++) {
	if (p->ufds[i].fd == fd) {
	    int j;
	    for (j=i+1; j < p->nfds; j++) {
		p->ufds[j-1] = p->ufds[j];
	    }
	    p->nfds -= 1;
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
    rv = poll(p->ufds, p->nfds, timeout);
    if (rv <= 0) {
	return rv;
    }
    for (i = 0; i < p->nfds; i++) {
	if (p->ufds[i].revents) {
	    evs[n].fd = p->ufds[i].fd;
	    evs[n].events = p->ufds[i].revents;
	    n++;
	    if (n == maxevents) {
		break;
	    }
	}
    }
    return n;
}


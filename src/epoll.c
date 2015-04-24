#ifdef __MACH__
#include "epoll.h"
#include <sys/event.h>
#include <time.h>
#include <stdio.h>
#include <stdlib.h>

#define MAX_EVENTS 100

int epoll_create(int size)
{
    return kqueue();
}

int epoll_ctl(int epfd, int op, int fd, struct epoll_event *event)
{
    static struct kevent evSet;
    static int rv;

    if (op == EPOL_CTL_ADD || op == EPOLL_CTL_MOD) {
        if (event->events & EPOLLIN) {
            EV_SET(&evSet, fd, EVFILT_READ, EV_ADD, 0, 0, event->data.ptr);
            if ((rv = kevent(epfd, &evSest, 1, NULL, 0, NULL))) return rv;
            }
        if (event->events & EPOLLOUT) {
            EV_SET(&evSet, fd, EVFILT_WRITE, EV_ADD, 0, 0, event->data.ptr);
            if ((rv = kevent(epfd, &evSet, 1, NULL, 0, NULL))) return rv;
        }
    } else if (op == EPOLL_CTL_DEL) {
        EV_SET(&evSet, fd, EVFILT_READ, EV_DELETE, 0, 0, 0);
        rv = kevent(epfd, &evSet, 1, NULL, 0, NULL);
        if (rv != ENOENT && rv != 0) return rv;
        EV_SET(&evSet, fd, EVFILT_WRITE, EV_DELETE, 0, 0, 0);
        rv = kevent(epfd, &evSet, 1, NULL, 0, NULL);
        if (rv == ENOENT) return 0;
        return rv;
    }
    return 0;
}

int epoll_wait(int epfd, struct epoll_event *events, int maxevents, int timeout)
{
    static struct kevent evList[MAX_EVENTS];
    static struct timespec _timeout;

    _timeout.tv_nsec = timeout*1000000;
    _timeout.tv_sec = 0;

    int nev, i;

    if (maxevents > MAX_EVENTS) {
        fprintf(stderr, "epoll_wait: maxevents > MAX_EVENTS\n");
        exit(1);
    }

    nev = kevent(epfd, NULL, 0, evList, maxevents, &_timeout);

    for (i = 0; i < nev; i++) {
        events[i].events = 0;
        if (evList[i].filter == EVFILT_READ) {
            events[i].events |= EPOLLIN;
        } else if (evList[i].filter == EVFILT_WRITE) {
            events[i].events |= EPOLLOUT;
        }
        if (evList[i].flags & EV_ERROR) events[i].events |= EPOLLERR;
        if (evList[i].flags & EV_EOF)   events[i].events |= EPOLLHUP;

        events[i].data.ptr = (void *)evList[i].udata;
    }

    return nev;
}
#endif

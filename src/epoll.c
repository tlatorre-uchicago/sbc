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

    short filter = 0;
    if (event->events & EPOLLIN)  filter |= EVFILT_READ;
    if (event->events & EPOLLOUT) filter |= EVFILT_WRITE;

    unsigned short flags = 0;
    if (op == EPOLL_CTL_ADD) flags = EV_ADD;
    if (op == EPOLL_CTL_MOD) flags = EV_ADD;
    if (op == EPOLL_CTL_DEL) flags = EV_DELETE;

    EV_SET(&evSet, fd, filter, flags, 0, 0, (void *)event->data);
    return kevent(epfd, &evSet, 1, NULL, 0, NULL);
}

int epoll_wait(int epfd, struct epoll_event *events, int maxevents, int timeout)
{
    static struct kevent evList[MAX_EVENTS];
    static struct timespec _timeout;

    _timeout.tv_nsec = 0;
    _timeout.tv_sec = timeout;

    int nev, i;

    if (maxevents > MAX_EVENTS) {
        fprintf(stderr, "epoll_wait: maxevents > MAX_EVENTS\n");
        exit(1);
    }

    nev = kevent(epfd, NULL, 0, evList, maxevents, &_timeout);

    for (i = 0; i < nev, i++) {
        events[i].events = 0;
        if (evList[i].flags & EVFILT_READ)  events[i].events |= EPOLLIN;
        if (evList[i].flags & EVFILT_WRITE) events[i].events |= EPOLLOUT;
        if (evList[i].flags & EV_ERROR)     events[i].events |= EPOLLERR;
        /* EPOLLHUP seems like the closest thing to EV_EOF */
        if (evList[i].flags & EV_EOF)       events[i].events |= EPOLLHUP;

        events[i].data = evList[i].udata;
    }

    return nev;
}

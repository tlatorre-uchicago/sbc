#ifndef TPOLL_H
#define TPOLL_H
#include <sys/poll.h>
#include <stdint.h>

#define MAX_FDS 100

struct tpoll_event {
    int fd;
    uint32_t events;
};

struct tpoll {
    int nfds;
    struct pollfd ufds[MAX_FDS];
};

struct tpoll *tpoll_create();
void tpoll_free(struct tpoll *p);
int tpoll_modify(struct tpoll *p, int fd, uint32_t eventmask);
int tpoll_modify_or(struct tpoll *p, int fd, uint32_t eventmask);
int tpoll_modify_and(struct tpoll *p, int fd, uint32_t eventmask);
int tpoll_add(struct tpoll *p, int fd, uint32_t eventmask);
int tpoll_del(struct tpoll *p, int fd);
int tpoll_poll(struct tpoll *p, struct tpoll_event *evs, int maxevents, int timeout);
#endif

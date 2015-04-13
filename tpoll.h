#ifndef TPOLL_H
#define TPOLL_H
#include <sys/poll.h>
#include <stdint.h>

struct tpoll_event {
    int fd;
    uint32_t events;
};

struct tpoll {
    unsigned int entries;
    unsigned int size;
    struct pollfd *ufds;
};

struct tpoll *tpoll_init();
void tpoll_free(struct tpoll *p);
int tpoll_modify(struct tpoll *p, int fd, uint32_t eventmask);
int tpoll_modify_or(struct tpoll *p, int fd, uint32_t eventmask);
int tpoll_modify_and(struct tpoll *p, int fd, uint32_t eventmask);
int tpoll_add(struct tpoll *p, int fd, uint32_t eventmask);
int tpoll_del(struct tpoll *p, int fd);
int tpoll_poll(struct tpoll *p, struct tpoll_event *evs, int maxevents, int timeout);
#endif

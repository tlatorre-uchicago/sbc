#ifndef TPOLL_H
#define TPOLL_H

/* An epoll like interface to sys/poll.h.
 *
 * Example:
 *
 * #define MAX_EVENTS 100
 *
 * int i, nfds, sockfd, new_fd;
 * struct tpoll_event ev, events[MAX_EVENTS];
 *
 * struct tpoll *p = tpoll_init();
 *
 * # set up sockfd (socket(), bind(), listen())
 *
 * tpoll_add(fd, POLLIN);
 *
 * nfds = tpoll_poll(p, events, MAX_EVENTS, 1000);
 *
 * for (i = 0, i < nfds; i++) {
 * 	ev = events[i];
 *
 * 	if (ev.fd == sockfd) {
 *          socklen_t sin_size = sizeof their_addr;
 *
 *          new_fd = accept(ev.fd, (struct sockaddr *)&their_addr,
 *                          &sin_size);
 *
 *          if (new_fd == -1) {
 *              perror("accept");
 *              continue;
 *          }
 *
 *          printf("got connection from %s", get_ip(new_fd));
 * 	}
 * }
 *
 */

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
int tpoll_in(struct tpoll *p, int fd);
void tpoll_free(struct tpoll *p);
int tpoll_modify(struct tpoll *p, int fd, uint32_t eventmask);
int tpoll_modify_or(struct tpoll *p, int fd, uint32_t eventmask);
int tpoll_modify_and(struct tpoll *p, int fd, uint32_t eventmask);
int tpoll_add(struct tpoll *p, int fd, uint32_t eventmask);
int tpoll_del(struct tpoll *p, int fd);
int tpoll_poll(struct tpoll *p, struct tpoll_event *evs, int maxevents, int timeout);
#endif

#include <stdio.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>
#include <string.h>
#include <time.h>
#ifdef __MACH__
#include <mach/clock.h>
#include <mach/mach.h>
#endif
#include <unistd.h>
#include <signal.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <fcntl.h>
#include "utils.h"
#include "buf.h"
#include "ptrset.h"
#include "XL3PacketTypes.h"
#include "sock.h"

#ifdef __MACH__
#include "epoll.h"
#else
#include <sys/epoll.h>
#endif

static volatile int go = 1;

void ctrlc_handler(int _) {
    go = 0;
}

/* XL3's connect on port XL3_PORT + crate */
#define XL3_PORT 44601
#define XL3_ORCA_PORT 54601
#define XL3_REQUEST_TIMEOUT 10

void get_monotonic_time(struct timespec *ts)
{
    /* Returns the time from a monotonic clock. Used for scheduling tasks. */
#ifdef __MACH__
    /* OSX does not have clock_gettime.
     * from stackoverflow.com/q/5167269 */
    clock_serv_t cclock;
    mach_timespec_t mts;
    /* SYSTEM_CLOCK is advertised as being monotonic
     * see
     * opensource.apple.com/source/xnu/xnu-2422.1.72/osfmk/mach/clock_types.h */
    host_get_clock_service(mach_host_self(), SYSTEM_CLOCK, &cclock);
    clock_get_time(cclock, &mts);
    mach_port_deallocate(mach_task_self(), cclock);
    ts->tv_sec = mts.tv_sec;
    ts->tv_nsec = mts.tv_nsec;
#else
    clock_gettime(CLOCK_MONOTONIC, ts);
#endif
}

struct sock *find_xl3_socket(int id)
{
    struct sock *s;
    int i;
    for (i = 0; i < sockset->entries; i++) {
        s = (struct sock *)sockset->values[i];
        if (s->type == XL3 && s->id == id) return s;
    }
    return NULL;
}

void check_req_times(struct timespec now)
{
    struct sock *s;
    int i;
    for (i = 0; i < sockset->entries; i++) {
        s = (struct sock *)sockset->values[i];

        if (s->type != XL3) continue;

        if (s->req && (now.tv_sec > s->req->t.tv_sec + XL3_REQUEST_TIMEOUT)) {
            fprintf(stderr, "WARNING: XL3 request timeout, "
                            "sending new request!\n");
            free(s->req);
            /* pop the next request off the queue */
            s->req = (struct XL3_request *)ptrset_popleft(s->req_queue);
            /* if there are no more requests, pop() returns NULL */
            if (s->req) {
                get_monotonic_time(&s->req->t);
                sock_write(s, (char *)&(s->req->packet), XL3_PACKET_SIZE);
            }
        }
    }
}

void process_xl3_orca_socket(struct sock *s)
{
    static char tmp[XL3_PACKET_SIZE];

    int n;
    while (BUF_LEN(s->rbuf) >= XL3_PACKET_SIZE) {
        n = buf_read(s->rbuf, tmp, XL3_PACKET_SIZE);
        if (n < 0) {
            fprintf(stderr, "ERROR: process_xl3_orca_socket(), failed to read "
                            "recv buffer.\n");
            return;
        }

        struct XL3_request *r = malloc(sizeof (struct XL3_request));
        memcpy((char *)&r->packet, tmp, XL3_PACKET_SIZE);
        r->sender = s;

        struct sock *xl3 = find_xl3_socket(s->id);

        if (!xl3) {
            fprintf(stderr, "ERROR: request -> XL3 %d dropped.\n", s->id);
            return;
        }

        if (xl3->req) {
            ptrset_add(xl3->req_queue, r);
        } else {
            xl3->req = r;
            get_monotonic_time(&r->t);
            sock_write(xl3, (char *)&(r->packet), XL3_PACKET_SIZE);
        }
    }
}

void process_xl3_data(struct sock *s)
{
    /* temporary buffer to hold XL3 packets */
    static char tmp[XL3_PACKET_SIZE];

    /* process all of the data in the read buffer */
    int n;
    while (BUF_LEN(s->rbuf) >= XL3_PACKET_SIZE) {
        n = buf_read(s->rbuf,tmp,XL3_PACKET_SIZE);
        if (n < 0) {
            fprintf(stderr, "buf_read failed\n");
            return;
        }
        XL3Packet *p = (XL3Packet *)tmp;

        /* packet type is a single byte so no need to swap
         * bytes */
        switch (p->header.packetType) {
            case PING_ID:
                /* ping -> pong */
                p->header.packetType = PONG_ID;
                sock_write(s, tmp, XL3_PACKET_SIZE);
                break;
            case MESSAGE_ID:
                /* print message */
                printf("XL3 message: %s", p->payload);
                break;
            case MEGA_BUNDLE_ID:
                /* dispatch */
                relay_to_dispatchers(tmp, XL3_PACKET_SIZE, MEGA_BUNDLE);
                break;
            default:
                /* forward -> sender */
                if (!s->req) {
                    fprintf(stderr, "WARNING: received reply packet from XL3, "
                                    "but no request active.\n");
                    continue;
                }
                /* we are waiting for a reply */
                if (p->header.packetType != s->req->packet.header.packetType) {
                    fprintf(stderr, "WARNING: received reply packet from XL3, "
                                    "but header doesn't match request.\n");
                    continue;
                }
                if (s->req->sender) {
                    sock_write(s->req->sender, tmp, XL3_PACKET_SIZE);
                } else {
                    fprintf(stderr, "WARNING: reply from XL3, but sender "
                                    "disconnected.\n");
                }
                /* free the current request */
                free(s->req);
                /* pop the next request off the queue */
                s->req = (struct XL3_request *)ptrset_popleft(s->req_queue);
                /* if there are no more requests, pop() returns NULL */
                if (s->req) {
                    get_monotonic_time(&s->req->t);
                    sock_write(s, (char *)&(s->req->packet), XL3_PACKET_SIZE);
                }
        } /* switch */
    } /* while */
}

int main(void)
{
    signal(SIGINT, ctrlc_handler);

    /* prevent SIGPIPE from crashing the program */
    signal(SIGPIPE, SIG_IGN);

    /* polling object */
    int nfds;
    struct epoll_event events[MAX_EVENTS];
    /* temporary buffer */
    char *tmpbuf = malloc(BUFSIZE*2);
    char *printbuf = malloc(BUFSIZE*2);
    /* time pointer for status */
    time_t t;
    struct tm *timeinfo;
    char timestr[256];
    /* timespec for printing info every 10 seconds */
    struct timespec time_last_info, time_last_check, time_now;
    struct sock *s;

    int i, n;

    get_monotonic_time(&time_now);
    time_last_info = time_last_check = time_now;

    if (global_setup() == -1) {
        exit(1);
    }

    if (sock_listen(3490, 10, CLIENT_LISTEN, 0) == -1) {
        fprintf(stderr, "failed to setup listening socket on port 3490\n");
        exit(1);
    }
    if (sock_listen(3491, 10, DISPATCH_LISTEN, 0) == -1) {
        fprintf(stderr, "failed to setup listening socket on port 3491\n");
        exit(1);
    }

    for (i = 0; i < 20; i++) {
        if (sock_listen(XL3_PORT + i, 1, XL3_LISTEN, i) == -1) {
            fprintf(stderr, "failed to setup listening socket on port %d\n", XL3_PORT+i);
            exit(1);
        }
    }

    for (i = 0; i < 20; i++) {
        if (sock_listen(XL3_ORCA_PORT + i, 1, XL3_ORCA_LISTEN, i) == -1) {
            fprintf(stderr, "failed to setup listening socket on port %d\n", XL3_PORT+i);
            exit(1);
        }
    }

    while (go) {
        /* timeout after 1 second */
        nfds = epoll_wait(epollfd, events, MAX_EVENTS, 100);

        get_monotonic_time(&time_now);

        if (time_now.tv_sec - time_last_check.tv_sec > 1) {
            time_last_check = time_now;
            check_req_times(time_now);
        }

        if (time_now.tv_sec - time_last_info.tv_sec > 10) {
            time_last_info = time_now;
            t = time(NULL);
            timeinfo = localtime(&t);
            /* print out status */
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

        if (nfds == -1) {
            perror("poll");
            continue;
        } 
        /* poll() timeout */
        if (nfds == 0) continue;

        for (i = 0; i < nfds; i++) {
            s = (struct sock *)events[i].data.ptr;

            if (sock_io(s, events[i].events) == -1) continue;

            /* basic I/O done, now check for messages.
             * note: you have to consume as much of the recv buffer here as
             * possible because otherwise if there are no POLL events for this
             * fd, the control flow will never come back here. */
            if (events[i].events & EPOLLIN) {
                switch (s->type) {
                    case XL3:
                        process_xl3_data(s);
                        break;
                    case XL3_ORCA:
                        process_xl3_orca_socket(s);
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
                        relay_to_dispatchers(printbuf, strlen(printbuf), 0);
                        break;
                } /* switch statement */
            } /* if events[i].events & POLLIN */
        } /* loop over nfds */
    } /* while(go) */

    /* free remaining buffers */
    for (i = 0; i < sockset->entries; i++) {
        sock_free(sockset->values[i]);
    }

    global_free();

    free(tmpbuf);
    free(printbuf);

    return 0;
}

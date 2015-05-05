#include <sys/types.h>
#include <sys/socket.h>
#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <unistd.h>
#include <netdb.h>
#include <assert.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include "../src/XL3PacketTypes.h"
#ifdef __MACH__
#include <mach/clock.h>
#include <mach/mach.h>
#endif

#define RATE 10000
#define N 1000
#define XL3_PORT 44601

float randexp()
{
    /* returns a random exponential variable */
    return -log((float)rand()/(float)RAND_MAX);
}

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

int sendall(int s, char *buf, int len)
{
    int n, sent = 0;

    while (sent < len) {
        n = send(s, buf+sent, len-sent, 0);
        if (n == -1) return -1;
        sent += n;
    }

    return 0;
}

int recvall(int s, char *buf, int len)
{
    int n, recvd = 0;

    while (recvd < len) {
        n = recv(s, buf+recvd, len-recvd, 0);
        if (n == -1) return -1;
        recvd += n;
    }

    return 0;
}

int main(int argc, char *argv[])
{
    int sockfd, rv, crate;
    struct addrinfo hints, *servinfo, *p;
    char buf[XL3_PACKET_SIZE];
    XL3Packet *packet = (XL3Packet *)buf;

    if (argc != 2) {
        fprintf(stderr, "usage: test_xl3 port\n");
        exit(1);
    }

    crate = atoi(argv[1]);
    int port = crate + XL3_PORT;
    char service[256];
    sprintf(service,"%d",port);

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    if ((rv = getaddrinfo("127.0.0.1", service, &hints, &servinfo)) != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
    }

    for (p = servinfo; p != NULL; p = p->ai_next) {
        if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
            perror("socket");
            continue;
        }

        if (connect(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
            close(sockfd);
            perror("connect");
            continue;
        }

        break;
    }

    if (p == NULL) {
        fprintf(stderr, "failed to connect.\n");
        return 2;
    }

    freeaddrinfo(servinfo);

    packet->header.packetType = MESSAGE_ID;
    sprintf(packet->payload, "XL3 message: XL3 - crate %d\n"
        "Code v4.02 using scatter gather dma, "
        "last git: b23c2ae20bdba68f1c6b1cbedac8b53a696b8551\n", crate);
    sendall(sockfd, buf, XL3_PACKET_SIZE);

    struct timespec now, then;

    get_monotonic_time(&now);
    then = now;

    int i;
    for (i = 0; i < N; i++) {
        usleep(randexp()*1e6/RATE);
        packet->header.packetType = MEGA_BUNDLE_ID;
        packet->header.packetNum = htons(i);
        packet->header.numBundles = 1; // ?
        memset(packet->payload, crate, sizeof packet->payload);
        sendall(sockfd, buf, XL3_PACKET_SIZE);

        get_monotonic_time(&now);

        if (now.tv_sec > then.tv_sec + 1) {
            then = now;
            packet->header.packetType = PING_ID;
            sendall(sockfd, buf, XL3_PACKET_SIZE);

            recvall(sockfd, buf, XL3_PACKET_SIZE);
            assert(packet->header.packetType == PONG_ID);
        }
    }

    close(sockfd);

    return 0;
}


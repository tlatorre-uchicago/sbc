#include <sys/types.h>
#include <sys/socket.h>
#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <unistd.h>
#include <netdb.h>
#include <assert.h>
#include <string.h>
#include "../src/XL3PacketTypes.h"

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
    int sockfd;
    struct addrinfo hints, *servinfo, *p;
    int rv;
    char buf[XL3_PACKET_SIZE];
    XL3Packet *packet = (XL3Packet *)buf;

    if (argc != 2) {
        fprintf(stderr, "usage: test_xl3 port\n");
        exit(1);
    }

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    if ((rv = getaddrinfo("127.0.0.1", argv[1], &hints, &servinfo)) != 0) {
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
    sprintf(packet->payload, "test");
    sendall(sockfd, buf, XL3_PACKET_SIZE);

    for (;;) {
        sleep(1);
        packet->header.packetType = PING_ID;
        sendall(sockfd, buf, XL3_PACKET_SIZE);

        recvall(sockfd, buf, XL3_PACKET_SIZE);
        assert(packet->header.packetType == PONG_ID);
        printf("ping -> pong\n");
    }

    close(sockfd);

    return 0;
}


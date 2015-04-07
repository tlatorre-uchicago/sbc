#include <stdio.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <sys/poll.h>
#include <errno.h>
#include <string.h>
#include "utils.h"

#define MAXFDS 10
#define BUFSIZE 256

void escape_string(char *dest, char *src)
{
    /* escape special characters in src -> dest.
    *  note: dest should be malloc'd to atleast 2x the size of src.
    *  from http://stackoverflow.com/q/3535023 */
    char c;

    while (c = *(src++)) {
        switch (c) {
            case '\a':
                *(dest++) = '\\';
                *(dest++) = 'a';
                break;
            case '\b':
                *(dest++) = '\\';
                *(dest++) = 'b';
                break;
            case '\t':
                *(dest++) = '\\';
                *(dest++) = 't';
                break;
            case '\n':
                *(dest++) = '\\';
                *(dest++) = 'n';
                break;
            case '\v':
                *(dest++) = '\\';
                *(dest++) = 'v';
                break;
            case '\f':
                *(dest++) = '\\';
                *(dest++) = 'f';
                break;
            case '\r':
                *(dest++) = '\\';
                *(dest++) = 'r';
                break;
            case '\\':
                *(dest++) = '\\';
                *(dest++) = '\\';
                break;
            case '\"':
                *(dest++) = '\\';
                *(dest++) = '\"';
                break;
            default:
                *(dest++) = c;
        }
    }

    *dest = '\0';
}

int main(void)
{
    int sockfd;
    int dispatchfd;

    if ((sockfd = setup_listen_socket("3490",10)) < 0) {
        fprintf(stderr, "failed to setup listening socket\n");
        exit(1);
    }

    /* connector's address info */
    struct sockaddr_storage their_addr;
    /* file descriptor to new connection*/
    int new_fd;
    /* connector's ip address */
    char s[INET6_ADDRSTRLEN];
    char buf[BUFSIZE];
    char pbuf[BUFSIZE*2];
    int size;
    struct pollfd ufds[MAXFDS];
    int i;
    int rv;

    ufds[0].fd = sockfd;
    ufds[0].events = POLLIN;
    ufds[0].revents = 0;
    int nfds = 1;

    printf("waiting for connections...\n");

    while (1) {

        /* timeout after 60 seconds */
        rv = poll(ufds, nfds, 60000);

        if (rv == -1) {
            perror("poll");
            continue;
        } 
            
        if (rv == 0) {
            printf("poll() timeout\n");
            continue;
        }

        if (ufds[0].revents & POLLIN) {
            /* client connected */

            int sin_size = sizeof their_addr;

            new_fd = accept(sockfd, (struct sockaddr *)&their_addr, &sin_size);
            if (new_fd == -1) {
                perror("accept");
                continue;
            }

            inet_ntop(their_addr.ss_family,
                get_in_addr((struct sockaddr *)&their_addr),
                s, sizeof s);
            printf("server: got connection from %s\n", s);

            if (nfds >= MAXFDS) {
                fprintf(stderr, "too many clients\n");
                close(new_fd);
            } else {
                ufds[nfds].fd = new_fd;
                ufds[nfds].events = POLLIN;
                ufds[nfds].revents = 0;
                nfds++;
            }
        }

        for (i=1; i < nfds; i++) {
            int revents = ufds[i].revents;

            if (revents & POLLERR) {
                fprintf(stderr,"received POLLERR, closing socket\n");
                /* close socket */
                close(ufds[i].fd);

                /* delete pollfd from pollfds array */
                int j;
                for (j=i+1; j < nfds; j++) {
                    ufds[j-1] = ufds[j];
                }
                /* one less socket */
                nfds -= 1;
                /* since the other pollfds were moved down by one
                * we need to recheck ufds[i]; */
                i -= 1;
                continue;
            } else if (revents & POLLHUP) {
                fprintf(stderr, "received POLLHUP, closing socket\n");
                /* close socket */
                close(ufds[i].fd);

                /* delete pollfd from pollfds array */
                int j;
                for (j=i+1; j < nfds; j++) {
                    ufds[j-1] = ufds[j];
                }
                /* one less socket */
                nfds -= 1;
                /* since the other pollfds were moved down by one
                * we need to recheck ufds[i]; */
                i -= 1;
                continue;
            } else if (revents & POLLNVAL) {
                fprintf(stderr, "received POLLNVAL, deleting socket\n");
                /* shouldn't close socket, but need to remove it from
                * pollfds.
                * see stackoverflow.com/q/24791625 */
                int j;
                for (j=i+1; j < nfds; j++) {
                    ufds[j-1] = ufds[j];
                }
                nfds -= 1;
                /* since the other pollfds were moved down by one
                * we need to recheck ufds[i]; */
                i -= 1;
                continue;
            } else if (revents & POLLIN) {
                /* data ready from client */

                printf("recv from %i\n",i);
                size = recv(ufds[i].fd, buf, sizeof buf, 0);

                if (size == -1) {
                    if ((errno == EAGAIN) || (errno == EWOULDBLOCK)) {
                        /* no data */
                        continue;
                    } else {
                        perror("recv");
                        continue;
                    }
                }

                if (size == 0) {
                    /* client disconnected */
                    printf("client disconnected\n");
                    close(ufds[i].fd);

                    int j;
                    for (j=i+1; j < nfds; j++) {
                        ufds[j-1] = ufds[j];
                    }
                    nfds -= 1;
                    /* since the other pollfds were moved down by one
                    * we need to recheck ufds[i]; */
                    i -= 1;
                    continue;
                }

                buf[size] = '\0';
                escape_string(pbuf,buf);
                int len = strlen(pbuf);
                pbuf[len++] = '\n';
                pbuf[len] = '\0';
                printf("received: %s", pbuf);
                /* just send, don't check return value */
                send(ufds[i].fd,pbuf,strlen(pbuf),0);
            }
        }
    }

    return 0;
}

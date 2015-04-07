#include <stdio.h>
#include <stdlib.h>
#include <netinet/in.h>
#include "utils.h"

int main(void)
{
    int sockfd;

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

    printf("waiting for connections...\n");

    while (1) {
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

        close(new_fd);
    }

    return 0;
}

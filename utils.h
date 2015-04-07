#include <netinet/in.h>

void *get_in_addr(struct sockaddr *sa);
int setup_listen_socket(char *port, int backlog);

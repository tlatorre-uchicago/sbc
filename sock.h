/* sock.h
 *
 */

#include "buf.h"
#include <sys/epoll.h>
#include "XL3PacketTypes.h"

/* buffer size for the send/recv buffers */
#define BUFSIZE 1000000
/* maximum number of file descriptor events for epoll_wait() */
#define MAX_EVENTS 100

typedef enum sock_type {
    CLIENT,
    CLIENT_LISTEN,
    DISPATCH,
    DISPATCH_LISTEN,
    XL3_LISTEN,
    XL3,
    XL3_ORCA,
    XL3_ORCA_LISTEN,
} sock_type_t;

typedef enum packet_type {
    MEGA_BUNDLE = 1,
    MTCD_BUNDLE = 2,
} packet_type_t;

struct xl3_cmd {
    XL3Packet msg;
    struct sock *sender;
    /* time? */
};

struct sock {
    int fd;
    sock_type_t type;
    int id;
    struct buffer *rbuf;
    struct buffer *sbuf;
    /* queue for commands -> XL3 */
    struct ptrset *cmdqueue;
    /* last sent command */
    struct xl3_cmd *cmd;
};

int epollfd;
struct ptrset *sockset;

int global_setup();
void global_free();
struct sock *sock_init(int fd, sock_type_t type, int id);
void sock_close(struct sock *s);
int sock_listen(int port, int backlog, int type, int id);
void sock_accept(struct sock *s);
void sock_io(struct sock *s, uint32_t event);
void sock_write(struct sock *s, char *buf, int size);
void sock_free(struct sock *s);
void relay_to_dispatchers(char *msg, int size, int type);

all: server

CC=cc
CFLAGS=-O0 -g -Wall -DDEBUG
LDLIBS=

UNAME_S := $(shell uname -s)
ifeq ($(UNAME_S), Linux)
	LDLIBS += -lrt
endif

sock.o: sock.c
	$(CC) $(CFLAGS) -c sock.c

buf.o: buf.c
	$(CC) $(CFLAGS) -c buf.c

ptrset.o: ptrset.c
	$(CC) $(CFLAGS) -c ptrset.c

utils.o: utils.c
	$(CC) $(CFLAGS) -c utils.c

server: utils.o server.c buf.o ptrset.o sock.o epoll.o
	$(CC) $(CFLAGS) server.c utils.o buf.o ptrset.o sock.o epoll.o -o server $(LDLIBS)

clean:
	rm *.o server

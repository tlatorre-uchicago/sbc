all: server

CC=cc
CFLAGS=-O0 -g -Wall -DDEBUG

sock.o: sock.c
	$(CC) $(CFLAGS) -c sock.c

buf.o: buf.c
	$(CC) $(CFLAGS) -c buf.c

intset.o: intset.c
	$(CC) $(CFLAGS) -c intset.c

ptrset.o: ptrset.c
	$(CC) $(CFLAGS) -c ptrset.c

tpoll.o: tpoll.c
	$(CC) $(CFLAGS) -c tpoll.c

utils.o: utils.c
	$(CC) $(CFLAGS) -c utils.c

server: utils.o server.c tpoll.o buf.o ptrset.o pack.o sock.o
	$(CC) $(CFLAGS) server.c utils.o tpoll.o buf.o ptrset.o pack.o sock.o -o server -lrt

pack.o: pack.c
	$(CC) $(CFLAGS) -c pack.c

clean:
	rm *.o server

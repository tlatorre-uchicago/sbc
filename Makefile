all: server pack

CC=cc
CFLAGS=-O0 -g -Wall -DDEBUG

buf.o: buf.c
	$(CC) $(CFLAGS) -c buf.c

tpoll.o: tpoll.c
	$(CC) $(CFLAGS) -c tpoll.c

utils.o: utils.c
	$(CC) $(CFLAGS) -c utils.c

server: utils.o server.c tpoll.o buf.o
	$(CC) $(CFLAGS) server.c utils.o tpoll.o buf.o -o server -lrt

pack.o: pack.c
	$(CC) $(CFLAGS) -c pack.c

pack: pack.o
	$(CC) $(CFLAGS) pack.o -o pack

clean:
	rm *.o server pack

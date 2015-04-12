all: server pack

CC=cc
CFLAGS=-O0 -Wall

tpoll.o: tpoll.c
	$(CC) $(CFLAGS) -c tpoll.c

utils.o: utils.c
	$(CC) $(CFLAGS) -c utils.c

server: utils.o server.c tpoll.o
	$(CC) $(CFLAGS) server.c utils.o tpoll.o -o server

pack.o: pack.c
	$(CC) $(CFLAGS) -c pack.c

pack: pack.o
	$(CC) $(CFLAGS) pack.o -DDEBUG -o pack

clean:
	rm *.o server pack

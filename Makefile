all: server pack

CFLAGS=-O4

tpoll.o: tpoll.c
	cc -c tpoll.c $(CFLAGS)

utils.o: utils.c
	cc -c utils.c $(CFLAGS)

server: utils.o server.c tpoll.o
	cc server.c utils.o tpoll.o -o server $(CFLAGS)

pack.o: pack.c
	cc -c pack.c $(CFLAGS)

pack: pack.o
	cc pack.o -DDEBUG -o pack $(CFLAGS)

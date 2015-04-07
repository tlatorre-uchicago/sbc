all: server pack


utils.o: utils.c
	cc -c utils.c

server: utils.o server.c
	cc server.c utils.o -o server

pack.o: pack.c
	cc -c pack.c

pack: pack.o
	cc pack.o -DDEBUG -o pack

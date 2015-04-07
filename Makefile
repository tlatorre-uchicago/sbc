all: server


utils.o: utils.c
	cc -c utils.c

server: utils.o server.c
	cc server.c utils.o -o server

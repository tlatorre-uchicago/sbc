all: test_ptrset test_buf

CC=cc
CFLAGS=-O0 -g -Wall -DDEBUG

test_ptrset: ../src/ptrset.o test_ptrset.o
	$(CC) $(CFLAGS) ../src/ptrset.o test_ptrset.o -o $@

test_buf: ../src/buf.o test_buf.o
	$(CC) $(CFLAGS) ../src/buf.o test_buf.o -o $@

clean:
	rm test_ptrset test_buf

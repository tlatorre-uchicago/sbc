all: test_ptrset test_buf test_xl3

CC=cc
CFLAGS=-O0 -g -Wall -DDEBUG
TEST=valgrind --leak-check=yes -q

test_ptrset: ../src/ptrset.o test_ptrset.o
	$(CC) $(CFLAGS) ../src/ptrset.o test_ptrset.o -o $@

test_buf: ../src/buf.o test_buf.o
	$(CC) $(CFLAGS) ../src/buf.o test_buf.o -o $@

test_xl3: test_xl3.o
	$(CC) $(CFLAGS) test_xl3.o -o $@

test: test_ptrset test_buf
	$(TEST) ./test_ptrset
	$(TEST) ./test_buf

clean:
	rm test_ptrset test_buf test_xl3

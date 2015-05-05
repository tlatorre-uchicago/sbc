all: test

CC=cc
CFLAGS=-O0 -g -Wall -DDEBUG
TEST=valgrind --leak-check=yes -q

test_ptrset: ../src/ptrset.o test_ptrset.o
	$(CC) $(CFLAGS) ../src/ptrset.o test_ptrset.o -o $@

test_buf: ../src/buf.o test_buf.o
	$(CC) $(CFLAGS) ../src/buf.o test_buf.o -o $@

xl3: xl3.o
	$(CC) $(CFLAGS) xl3.o -o $@ -lm -lrt

test: test_ptrset test_buf xl3
	$(TEST) ./test_ptrset
	$(TEST) ./test_buf
	./test_xl3

clean:
	rm test_ptrset test_buf *.o

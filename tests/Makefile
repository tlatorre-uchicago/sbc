all: test_ptrset

CC=cc
CFLAGS=-O0 -g -Wall -DDEBUG

test_ptrset: ../src/ptrset.o test_ptrset.o
	$(CC) $(CFLAGS) ../src/ptrset.o test_ptrset.o -o $@

clean:
	rm test_ptrset

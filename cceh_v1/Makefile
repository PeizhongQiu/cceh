all: dynamic_hash

CFLAGS=${CFLAG}
CFLAGS+=-g -c

dynamic_hash: test.o memory_management.o hash.o 
	gcc -g  -o dynamic_hash test.o memory_management.o hash.o -lm -lpthread -lpmem

test.o : test.c hash.h
	gcc ${CFLAGS} test.c 

memory_management.o: memory_management.c 
	gcc ${CFLAGS} memory_management.c
 
hash.o :hash.c 
	gcc ${CFLAGS} hash.c

clean:
	rm -rf *.o

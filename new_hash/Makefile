all: dynamic_hash

CFLAGS=${CFLAG}
CFLAGS+=-g -c

dynamic_hash: test.o memory_management.o new_hash.o 
	gcc -g  -o dynamic_hash test.o memory_management.o new_hash.o -lm -lpthread -lpmem

test.o : test.c hash.h
	gcc ${CFLAGS} test.c 

memory_management.o: memory_management.c 
	gcc ${CFLAGS} memory_management.c
 
new_hash.o :new_hash.c 
	gcc ${CFLAGS} new_hash.c

clean:
	rm -rf *.o

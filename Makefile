all: dynamic_hash

dynamic_hash: test.o memory_management.o hash.o 
	gcc -o dynamic_hash test.o memory_management.o hash.o -lm -lpthread -lpmem
test.o : test.c hash.h
	gcc -c test.c

memory_management.o: memory_management.c 
	gcc -c memory_management.c 
hash.o :hash.c 
	gcc -c hash.c
clean:
	rm -rf *.o
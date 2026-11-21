


CC = gcc
CFLAGS = -Wall -Iinclude -Wextra -g -fsanitize=address 


build/test_basic: build/alloc.o build/free.o build/heap.o build/page.o build/segment.o build/test_basic.o
	$(CC) $(CFLAGS) build/alloc.o build/free.o build/heap.o build/page.o build/segment.o build/test_basic.o -o build/test_basic
	
build/test_basic.o: tests/test_basic.c
	$(CC) $(CFLAGS) -c tests/test_basic.c -o build/test_basic.o

build/alloc.o: src/alloc.c
	$(CC) $(CFLAGS) -c src/alloc.c -o build/alloc.o

build/free.o: src/free.c
	$(CC) $(CFLAGS) -c src/free.c -o build/free.o

build/heap.o: src/heap.c
	$(CC) $(CFLAGS) -c src/heap.c -o build/heap.o

build/page.o: src/page.c
	$(CC) $(CFLAGS) -c src/page.c -o build/page.o

build/segment.o: src/segment.c
	$(CC) $(CFLAGS) -c src/segment.c -o build/segment.o


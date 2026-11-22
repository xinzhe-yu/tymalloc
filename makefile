


CC = gcc
CFLAGS = -Wall -Iinclude -Wextra -g -fsanitize=address 
SRC = $(wildcard src/*.c)
OBJS = $(patsubst src/%.c, build/%.o, $(SRC))


build/test_basic: $(OBJS) build/test_basic.o
	$(CC) $(CFLAGS) $^ -o $@
	
build/test_basic.o: tests/test_basic.c
	mkdir -p build
	$(CC) $(CFLAGS) -c $< -o $@

build/%.o: src/%.c
	mkdir -p build
	$(CC) $(CFLAGS) -c $< -o $@



clean: 
	rm -rf build

.PHONY: clean

test: build/test_basic
	./build/test_basic

.PHONY: test
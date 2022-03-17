gcc ?= gcc
CC = gcc
CFLAGS = -Wall -fpic -I../../../libsolv/src

SOURCES = $(shell find -name '*.c')
OBJ = $(patsubst ./%.c, ./%.o, $(SOURCES))

define tips
        $(info )
        $(info *************** $(1) ***************)
        $(info )
endef

%.o: %.c %.h
	$(CC) $(CFLAGS) -c $< -o $@

.PHONY: build

build: $(OBJ)
	$(CC) -shared -o ../lib/libsched.so $(OBJ)

.PHONY: clean

clean:
	rm *.o *.so ../lib/*

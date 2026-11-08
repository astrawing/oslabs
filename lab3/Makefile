CC      = gcc
CFLAGS  = -std=c11 -Wall -Wextra -O2 -D_POSIX_C_SOURCE=200809L
LDLIBS  = -pthread

all: parent child1 child2

parent: parent.c
	$(CC) $(CFLAGS) -o $@ parent.c $(LDLIBS)

child1: child1.c
	$(CC) $(CFLAGS) -o $@ child1.c $(LDLIBS)

child2: child2.c
	$(CC) $(CFLAGS) -o $@ child2.c $(LDLIBS)

clean:
	rm -f parent child1 child2

.PHONY: all clean
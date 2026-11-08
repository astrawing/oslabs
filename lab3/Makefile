CC      = gcc
CFLAGS  = -std=c99
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
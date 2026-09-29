CC = gcc
CFLAGS = -Wall -Wextra -std=c99 

TARGETS = parent child1 child2

all: $(TARGETS)

parent: parent.c
	$(CC) $(CFLAGS) -o $@ $<

child1: child1.c
	$(CC) $(CFLAGS) -o $@ $<

child2: child2.c
	$(CC) $(CFLAGS) -o $@ $<

clean:
	rm -f $(TARGETS)

rebuild: clean all

.PHONY: all clean rebuild
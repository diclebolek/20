CC = gcc
CFLAGS = -Wall -Wextra -std=c99

all: shell

shell: main.o shell.o
	$(CC) $(CFLAGS) -o shell main.o shell.o

main.o: main.c shell.h
	$(CC) $(CFLAGS) -c main.c

shell.o: shell.c shell.h
	$(CC) $(CFLAGS) -c shell.c

clean:
	rm -f *.o shell

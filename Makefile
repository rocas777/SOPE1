CC=gcc
CFLAGS=-I.

main: simpledu.c
	$(CC) -o simpledu simpledu.c

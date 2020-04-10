CC=gcc
CFLAGS=-I.

simpledu: simpledu.c
	$(CC) -o simpledu simpledu.c -Werror

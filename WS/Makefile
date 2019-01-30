CC     = gcc
CFLAGS = -g -Wno-unused-value -Wall -Wstrict-prototypes -std=c99 -pedantic

ws: ws.o sha1.o
	$(CC) ws.o sha1.o -o ws -lm

ws.o: ws.c sha1.h sha1utils.h
	$(CC) $(CFLAGS) -c ws.c

sha1.o: sha1.c sha1.h sha1defines.h
	$(CC) $(CFLAGS) -c sha1.c

clean:
	rm -f *.o ws


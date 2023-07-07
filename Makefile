CC = gcc
CFLAGS = -Wall -Werror -ggdb3

all: subscriber server

subscriber: subscriber.c utils.h utils.c
	$(CC) $(CFLAGS) -o subscriber subscriber.c utils.c

server: server.c utils.h utils.c
	$(CC) $(CFLAGS) -o server server.c utils.c

clean:
	rm -f subscriber server
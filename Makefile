
CC = gcc
CFLAGS = -g -Wall -Werror -std=gnu99 -Iinclude

all: pg2server/myftpd pg2client/myftp

clean:
	@echo "Cleaning..."
	@rm pg2server/myftpd pg2client/myftp src/*.o

pg2client/myftp: src/client.o
	@echo "Compiling Server"
	$(CC) $(CFLAGS) -o $@ $^

pg2server/myftpd: src/server.o
	@echo "Compiling Server"
	$(CC) $(CFLAGS) -o $@ $^

%.o: %.c
	@echo "Compiling C Files"
	$(CC) $(CFLAGS) -c -o $@ $^

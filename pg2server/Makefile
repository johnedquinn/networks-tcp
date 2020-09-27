
CC = gcc
CFLAGS = -g -Wall -Werror -std=gnu99

all: myftpd

clean:
	@echo "Cleaning..."
	@rm myftpd *.o

myftpd: server.o
	@echo "Compiling Server"
	$(CC) $(CFLAGS) -o $@ $^

%.o: %.c
	@echo "Compiling C Files"
	$(CC) $(CFLAGS) -c -o $@ $^

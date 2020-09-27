
CC = gcc
CFLAGS = -g -Wall -Werror -std=gnu99

all: myftp

clean:
	@echo "Cleaning..."
	@rm myftp *.o

myftp: client.o
	@echo "Compiling Server"
	$(CC) $(CFLAGS) -o $@ $^

%.o: %.c
	@echo "Compiling C Files"
	$(CC) $(CFLAGS) -c -o $@ $^

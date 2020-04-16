CC = clang
ARGS = -Wall -g

all: read_stat.o server.o server

read_stat.o: read_stat.c
	$(CC) -c $(ARGS) read_stat.c -pthread
	
server.o: server.c
	$(CC) -c $(ARGS) server.c -pthread
	
server: read_stat.o server.o
	$(CC) -o server $(ARGS) read_stat.o server.o -pthread

clean: 
	rm -rf read_stat.o server.o server
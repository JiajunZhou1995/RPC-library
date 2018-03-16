CC=g++
CFLAGS1 = -O0 -g -Wall -pthread
CFLAGS2 = -w -g -c -std=c++11 -pthread

all: binder

client: client.o rpc.o
	$(CC) $(CFLAGS1) client.o librpc.a -o client

server: server.o rpc.o
	$(CC) $(CFLAGS1) server.o server_functions.o server_function_skels.o librpc.a -o server

binder: binder.o rpc.o
	$(CC) $(CFLAGS1) binder.o librpc.a -o binder

client.o: client.c
	$(CC) $(CFLAGS2) client.c -o client.o

server.o: server.c server_functions.c server_function_skels.c
	$(CC) $(CFLAGS2) server.c server_functions.c server_function_skels.c

binder.o: binder.cc
	$(CC) $(CFLAGS2) binder.cc -o binder.o

rpc.o: rpc.cc
	$(CC) $(CFLAGS2) rpc.cc
	ar crs librpc.a rpc.o

clean:
	rm -f *o client server binder librpc.a

all: server

clean:
	rm server

server: server.c utility.c utility.h
	gcc -o server server.c utility.c 

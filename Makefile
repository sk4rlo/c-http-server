#name of the target, all is the default for the make command
all: server

#will compile only if the binary was changed after the last make
server: server.c http.c http.h
	gcc server.c http.c -Wall -W -O2 -o server.out

clean:
	rm -f server.out
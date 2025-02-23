all: chat-server

chat-server: http-server.c chat-server.c
	gcc -std=c11 -Wall -Wno-unused-variable -g http-server.c chat-server.c -o chat-server

clean:
	rm -f chat-server

client:
	gcc -o client client.c -lpthread -Wall

server:
	gcc -o server server.c -lpthread -Wall

clean:
	rm -f client server *.o

retry: clean client server # for testing purposes
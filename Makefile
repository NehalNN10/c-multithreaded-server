client2:
	gcc -o client client2.c -lpthread -Wall

server2:
	gcc -o server server2.c -lpthread -Wall

clean:
	rm -f client server *.o

retry: clean client2 server2
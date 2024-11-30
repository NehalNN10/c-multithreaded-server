client2:
	gcc -o client client2.c -lpthread -Wall

server2:
	gcc -o server server2.c -lpthread -Wall

clean:
	rm -f client server *.o

retry: clean client2 server2

client1:
	gcc client1.c -o client1 -Wall

server1:
	gcc server1.c -o server1 -Wall
FILE=$(ARGS)

build $(FILE):
	gcc $(FILE).c -o $(FILE) -Wall -lpthread

clean :
	rm -f $(FILE)
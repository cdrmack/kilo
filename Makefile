CFLAGS = -std=c2x -Wall -Wextra -pedantic
BINARY = kilo

kilo: kilo.c
	$(CC) $(CFLAGS) kilo.c -o $(BINARY)

clean:
	rm $(BINARY)

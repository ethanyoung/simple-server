CFLAGS = -Wall -g

OBJ = forking-web-server.o
PROGRAM = forking-web-server

$(PROGRAM): $(OBJ)
	gcc -g -o $@ $(OBJ)

forking-web-server.o: forking-web-server.c
	gcc -g -c -std=gnu89 forking-web-server.c


clean:
	rm -f $(PROGRAM) $(OBJ)

smallsh: smallsh.o
	cc -o smallsh smallsh.o

smallsh.o: smallsh.c
	cc -c smallsh.c

clean:
	rm smallsh smallsh.o

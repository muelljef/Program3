smallsh: main.o
	cc -o smallsh main.o

main.o: main.c
	cc -c main.c

clean:
	rm smallsh main.o

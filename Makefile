tabover: main.c
	gcc -o $@ main.c -lxcb

clean:
	rm -f tabover

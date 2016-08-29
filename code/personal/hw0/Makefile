all: wc main map
main: main.o
	gcc main.o -o main
map: map.o
	gcc map.o -o map	
wc: wc.o
	gcc wc.o -o wc
main.o: main.c
	gcc -c main.c
map.o: map.c
	gcc -c map.c
wc.o: wc.c
	gcc -c wc.c
clean:
	rm -rf *o wc
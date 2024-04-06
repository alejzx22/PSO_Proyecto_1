CC = gcc

all: compress decompress

library: huffman.h huffman.c
	$(CC) -c huffman.c -o huffman.o

compress: compress.c huffman.o
	$(CC) compress.c huffman.o -o compress.out

decompress: decompress.c huffman.o
	$(CC) decompress.c huffman.o -o decompress.out

clean:
	rm -f *.o *.out
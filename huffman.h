// Libs
#include <stdio.h>
#include <locale.h>
#include <wchar.h>
#include <wctype.h>
#include <stdlib.h>
#include <string.h>

// Constants
#define NUM_CHARS 1114112
#define MARKER "\xFF\xFF\xFF\xFF"

// Structs

// Huffman tree
typedef struct Node {
    wint_t character;
    unsigned int frequency;
    struct Node *left;
    struct Node *right;
    char *code;
} Node;

// Function prototypes

/* *** Compression functions *** */

// Count the frequency of each character in the file and store it in the frequencies array
unsigned int get_frequencies(FILE *file, wint_t frequencies[]);

// Create a new node with the given character and frequency
Node *new_node(wint_t character, unsigned int frequency);

// Returns true if the frequencies of the two nodes are equal
int compare_nodes(const void *a, const void *b);

// Build a Huffman tree from the given frequencies
Node *build_tree(wint_t frequencies[], int *unique);

// Print the Huffman tree and its codes
void print_tree(Node *node, int level);

// Recursively asign code to each character
void assign_codes(Node *node, char *code);

// Write the Huffman tree to the output file
void write_tree(FILE *output, Node *node);

// Get the Huffman code for the given character
char *get_code(Node *node, wint_t character);

// Compress the file using the Huffman tree and write the result to the output file in binary
void compress(FILE *input, FILE *output, Node *root);

/* *** Decompression functions *** */

// Read the Huffman tree from the input file
Node *read_tree(FILE *input);

// Decompress the file using the Huffman tree and write the result to the output file
void decompress(FILE *input, FILE *output, Node *root, int total_chars);
void decompress_with_offset(FILE *input, FILE *output, Node *root, int file_size, long file_position);

// Serial implementation of the decompression process
void serial_decompression(FILE *input, Node *root, char **files, int count_of_files, int *file_sizes);
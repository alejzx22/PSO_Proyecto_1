#include "huffman.h"

/* *** Compression functions *** */

unsigned int get_frequencies(FILE *file, wint_t frequencies[]) {
    setlocale(LC_ALL, "en_US.UTF-8");
    unsigned int total = 0;
    wint_t c;
    int line = 0;
    
    while ((c = fgetwc(file)) != WEOF) {
        frequencies[c]++;
        total++;
        if (c == '\n') {
            line++;
        }
    }
    return total;
}

Node *new_node(wint_t character, unsigned int frequency) {
    Node *node = malloc(sizeof(Node));
    node->character = character;
    node->frequency = frequency;
    node->left = NULL;
    node->right = NULL;
    return node;
}

int compare_nodes(const void *a, const void *b) {
    Node *node1 = *(Node **)a;
    Node *node2 = *(Node **)b;
    return node1->frequency - node2->frequency;
}

Node *build_tree(wint_t frequencies[], int *unique) {
    // Create an array of nodes
    Node **nodes = malloc(NUM_CHARS * sizeof(Node *));
    int num_nodes = 0;
    for (int i = 0; i < NUM_CHARS; i++) {
        if (frequencies[i] > 0) {
            (*unique)++; 
            nodes[num_nodes++] = new_node(i, frequencies[i]);
        }
    }
    // Sort the array of nodes
    qsort(nodes, num_nodes, sizeof(Node *), compare_nodes);
    while (num_nodes > 1) {
        // Remove the two nodes with the lowest frequency
        Node *left = nodes[0];
        Node *right = nodes[1];
        // Create a new node with the sum of the frequencies
        Node *node = new_node(0, left->frequency + right->frequency);
        node->left = left;
        node->right = right;
        // Shift the rest of the array down
        for (int i = 2; i < num_nodes; i++) {
            nodes[i - 2] = nodes[i];
        }
        // Insert the new node in the correct position
        int i = num_nodes - 2;
        while (i > 0 && nodes[i - 1]->frequency > node->frequency) {
            nodes[i] = nodes[i - 1];
            i--;
        }
        nodes[i] = node;
        num_nodes--;
    }
    Node *root = nodes[0];
    free(nodes);
    return root;
}

void print_tree(Node *node, int level) {
    if (node == NULL) {
        return;
    }
    for (int i = 0; i < level; i++) {
        printf("  ");
    }
    if (node->character == 0) {
        printf("Node (%u)\n", node->frequency);
    } else {
        printf("'%lc' (%u): %s\n", node->character, node->frequency, node->code);
    }
    print_tree(node->left, level + 1);
    print_tree(node->right, level + 1);
}

void assign_codes(Node *node, char *code) {
    if (node->left == NULL && node->right == NULL) {
        // This is a leaf node, assign the code
        node->code = strdup(code);
    } else {
        // This is an internal node, traverse its children
        char left_code[strlen(code) + 2];
        strcpy(left_code, code);
        strcat(left_code, "0");
        assign_codes(node->left, left_code);

        char right_code[strlen(code) + 2];
        strcpy(right_code, code);
        strcat(right_code, "1");
        assign_codes(node->right, right_code);
    }
}

void write_tree(FILE *output, Node *node) {
    if (node->left == NULL && node->right == NULL) {
        // This is a leaf node, write a '1' followed by the character
        fwrite("1", sizeof(char), 1, output);
        fwrite(&(node)->character, sizeof(wint_t), 1, output);
    } else {
        // This is an internal node, write a '0' and traverse its children
        fwrite("0", sizeof(char), 1, output);
        write_tree(output, node->left);
        write_tree(output, node->right);
    }
}

char *get_code(Node *node, wint_t character) {
    if (node->left == NULL && node->right == NULL) {
        // This is a leaf node, check if it is the character we are looking for
        if (node->character == character) {
            return node->code;
        } else {
            return "";
        }
    } else {
        // This is an internal node, traverse its children
        char *left_code = get_code(node->left, character);
        if (strcmp(left_code, "") != 0) {
            return left_code;
        }
        char *right_code = get_code(node->right, character);
        if (strcmp(right_code, "") != 0) {
            return right_code;
        }
        return "";
    }
}

// void compress(FILE *input, FILE *output, Node *root, int unique, int total) {
//     // Write the number of unique characters
//     fwrite(&unique, sizeof(int), 1, output);
//     // Write the total number of characters
//     fwrite(&total, sizeof(int), 1, output);
//     // Write the Huffman tree
//     write_tree(output, root);
//     // Write the compressed data
//     char buffer = 0;
//     int bits = 0;
//     wint_t c;
//     while ((c = fgetwc(input)) != WEOF) {
//         char *code = get_code(root, c);
//         for (int i = 0; i < strlen(code); i++) {
//             buffer = buffer << 1;
//             if (code[i] == '1') {
//                 buffer = buffer | 1;
//             }
//             bits++;
//             if (bits == 8) {
//                 fwrite(&buffer, sizeof(char), 1, output);
//                 buffer = 0;
//                 bits = 0;
//             }
//         }
//     }
//     if (bits > 0) {
//         buffer = buffer << (8 - bits);
//         fwrite(&buffer, sizeof(char), 1, output);
//     }
// }

void compress(FILE *input, FILE *output, Node *root) {
    // Initialize buffer and bits count
    unsigned char buffer = 0;
    int bits_in_buffer = 0;

    wint_t c;
    while ((c = fgetwc(input)) != WEOF) {
        char *code = get_code(root, c);
        for (int i = 0; i < strlen(code); i++) {
            // Pack the bit into the buffer
            buffer = (buffer << 1) | (code[i] - '0');
            bits_in_buffer++;

            // If buffer is full, write it to the output file
            if (bits_in_buffer == 8) {
                fwrite(&buffer, sizeof(unsigned char), 1, output);
                buffer = 0;
                bits_in_buffer = 0;
            }
        }
    }

    // Write any remaining bits in the buffer
    if (bits_in_buffer > 0) {
        buffer <<= (8 - bits_in_buffer);
        fwrite(&buffer, sizeof(unsigned char), 1, output);
    }
}

/* *** Decompression functions *** */
void decompress(FILE *input, FILE *output, Node *root, int file_size) {
    // Initialize a buffer to hold the bits read from the input file
    unsigned char buffer = 0;
    int bits_in_buffer = 0;

    Node *node = root;
    unsigned char c;
    int decompressed_size = 0;
    while (decompressed_size < file_size && fread(&c, sizeof(unsigned char), 1, input) == 1) {
        for (int i = 0; i < 8; i++) {
            // Get the bit most to the left to go through the Huffman tree
            unsigned char bit = (c >> (7 - i)) & 1;

            // Traverse the Huffman tree
            if (bit == 0) {
                node = node->left;
            } else {
                node = node->right;
            }

            // If this is a leaf node, write the character to the output file
            if (node->left == NULL && node->right == NULL) {
                fputwc(node->character, output);
                node = root;
                decompressed_size++;
                if (decompressed_size == file_size) {
                    break; 
                }
            }
        }
    }
}

void decompress_with_offset(FILE *input, FILE *output, Node *root, int file_size, long file_position) {
    fseek(input, file_position, SEEK_SET);
    // Initialize a buffer to hold the bits read from the input file
    unsigned char buffer = 0;
    int bits_in_buffer = 0;

    Node *node = root;
    unsigned char c;
    int decompressed_size = 0;
    while (decompressed_size < file_size && fread(&c, sizeof(unsigned char), 1, input) == 1) {
        for (int i = 0; i < 8; i++) {
            // Get the bit most to the left to go through the Huffman tree
            unsigned char bit = (c >> (7 - i)) & 1;

            // Traverse the Huffman tree
            if (bit == 0) {
                node = node->left;
            } else {
                node = node->right;
            }

            // If this is a leaf node, write the character to the output file
            if (node->left == NULL && node->right == NULL) {
                fputwc(node->character, output);
                node = root;
                decompressed_size++;
                if (decompressed_size == file_size) {
                    break; 
                }
            }
        }
    }
}


Node *read_tree(FILE *input) {
    // Read a bit from the input file
    unsigned char bit;
    fread(&bit, 1, 1, input);

    // If the bit is '1', this is a leaf node
    if (bit == '1') {
        // Read the character from the input file
        wint_t character;
        fread(&character, sizeof(wint_t), 1, input);

        // Create a new leaf node
        Node *node = (Node *)malloc(sizeof(Node));
        node->character = character;
        node->left = NULL;
        node->right = NULL;
        return node;
    } else {
        // Create a new internal node
        Node *node = (Node *)malloc(sizeof(Node));
        node->left = read_tree(input);
        node->right = read_tree(input);
        return node;
    }
}

void serial_decompression(FILE *input, Node *root, char **files, int count_of_files, int *file_sizes) {
    // Decompress each file in the output directory
    for (int i = 0; i < count_of_files; i++) {
        // Read the filename
        char *filename = files[i];

        // Create the output file path
        char output_filepath[256];
        snprintf(output_filepath, sizeof(output_filepath), "output/%s", filename);

        // Open the output file
        FILE *output_file = fopen(output_filepath, "w");
        if (output_file == NULL) {
            perror("fopen");
            return;
        }

        // Decompress the file
        decompress(input, output_file, root, file_sizes[i]);
        fclose(output_file);
    }
}
#include "huffman.h"
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/time.h>
#include <pthread.h>
#include <sys/wait.h>
#include <unistd.h>

#define MAX_THREADS 8
#define MAX_PROCESSES 16

// Structs
typedef struct {
    FILE *input;
    FILE *output;
    Node *root;
    int size;
    pthread_mutex_t *mutex;
} ThreadData;

extern int optind;

int serial_decompression_procedure(FILE *input) {
    int unique_chars;
    fread(&unique_chars, sizeof(int), 1, input);
    int total_chars;
    fread(&total_chars, sizeof(int), 1, input);
    Node *root = read_tree(input);

    // Read the filenames
    int count_of_files;
    fread(&count_of_files, sizeof(int), 1, input);
    char **files = (char **)malloc(count_of_files * sizeof(char *));
    if (files == NULL) {
        fprintf(stderr, "Memory allocation failed.\n");
        fclose(input);
        return 1;
    }

    for (int i = 0; i < count_of_files; i++){
        // Read the length of the filename
        int filename_length;
        fread(&filename_length, sizeof(int), 1, input);
        files[i] = (char *)malloc((filename_length + 1) * sizeof(char));
        if (files[i] == NULL) {
            fprintf(stderr, "Memory allocation failed.\n");
            // Free memory allocated so far
            for (int j = 0; j < i; j++) {
                free(files[j]);
            }
            free(files);
            fclose(input);
            return 1;
        }
        // Read the filename
        fread(files[i], sizeof(char), filename_length, input);
        // Null-terminate the string
        files[i][filename_length] = '\0';
    }

    // Read the size of each file
    int *file_sizes = (int *)malloc(count_of_files * sizeof(int));
    if (file_sizes == NULL) {
        fprintf(stderr, "Memory allocation failed.\n");
        // Free memory allocated so far
        for (int i = 0; i < count_of_files; i++) {
            free(files[i]);
        }
        free(files);
        fclose(input);
        return 1;
    }
    fread(file_sizes, sizeof(int), count_of_files, input);

    // Create the output directory if it doesn't exist
    mkdir("output", 0777);

    serial_decompression(input, root, files, count_of_files, file_sizes);

    for (int i = 0; i < count_of_files; i++){
        free(files[i]);
    }
    free(files);
    return 0;
}


void* concurrent_decompression_thread(void *arg){
    ThreadData *data = (ThreadData *)arg;
    pthread_mutex_lock(data->mutex);
    decompress(data->input, data->output, data->root, data->size);
    pthread_mutex_unlock(data->mutex);
    return NULL;
}

int concurrent_decompression_procedure(FILE *input){
    int unique_chars;
    fread(&unique_chars, sizeof(int), 1, input);
    int total_chars;
    fread(&total_chars, sizeof(int), 1, input);
    Node *root = read_tree(input);

    // Read the filenames
    int count_of_files;
    fread(&count_of_files, sizeof(int), 1, input);
    char **files = (char **)malloc(count_of_files * sizeof(char *));
    if (files == NULL) {
        fprintf(stderr, "Memory allocation failed.\n");
        fclose(input);
        return 1;
    }

    for (int i = 0; i < count_of_files; i++){
        // Read the length of the filename
        int filename_length;
        fread(&filename_length, sizeof(int), 1, input);
        files[i] = (char *)malloc((filename_length + 1) * sizeof(char));
        if (files[i] == NULL) {
            fprintf(stderr, "Memory allocation failed.\n");
            // Free memory allocated so far
            for (int j = 0; j < i; j++) {
                free(files[j]);
            }
            free(files);
            fclose(input);
            return 1;
        }
        // Read the filename
        fread(files[i], sizeof(char), filename_length, input);
        // Null-terminate the string
        files[i][filename_length] = '\0';
    }

    // Read the size of each file
    int *file_sizes = (int *)malloc(count_of_files * sizeof(int));
    if (file_sizes == NULL) {
        fprintf(stderr, "Memory allocation failed.\n");
        // Free memory allocated so far
        for (int i = 0; i < count_of_files; i++) {
            free(files[i]);
        }
        free(files);
        fclose(input);
        return 1;
    }
    fread(file_sizes, sizeof(int), count_of_files, input);

    // Create the output directory if it doesn't exist
    mkdir("output", 0777);

    /* START OF CONCURRENT DECOMPRESSION*/

    pthread_t threads[MAX_THREADS];
    ThreadData thread_data[MAX_THREADS];

    pthread_mutex_t mutex;
    pthread_mutex_init(&mutex, NULL);

    for (int i = 0; i < count_of_files; i+=MAX_THREADS){
        int active_threads = MAX_THREADS;
        if ( i + MAX_THREADS > count_of_files){
            active_threads = count_of_files - i;
        }

        for (int j = 0; j < active_threads; j++){
            char output_filepath[256];
            snprintf(output_filepath, sizeof(output_filepath), "output/%s", files[i+j]);
            FILE *output_file = fopen(output_filepath, "w");
            if (output_file == NULL) {
                perror("fopen");
                return 1;
            }

            thread_data[j].input = input;
            thread_data[j].output = output_file;
            thread_data[j].root = root;
            thread_data[j].size = file_sizes[i+j];
            thread_data[j].mutex = &mutex;
            pthread_create(&threads[j], NULL, concurrent_decompression_thread, &thread_data[j]);
        }

        for (int j = 0; j < active_threads; j++){
            pthread_join(threads[j], NULL);
        }
    }   

    pthread_mutex_destroy(&mutex);
    /* END OF CONCURRENT DECOMPRESSION*/
    

    for (int i = 0; i < count_of_files; i++){
        free(files[i]);
    }
    free(files);
    return 0;
}

int get_bytes(FILE *input, int file_size, Node *root) {
    // Get the current position in the file
    long current_position = ftell(input);

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
                node = root;
                decompressed_size++;
                if (decompressed_size == file_size) {
                    break; 
                }
            }
        }
    }
    // Get the current position in the file
    long end_position = ftell(input);
    long bytes_read = end_position - current_position;
    return bytes_read;
}

int parallel_decompression_procedure(FILE *input, char *input_filename){
    int unique_chars;
    fread(&unique_chars, sizeof(int), 1, input);
    int total_chars;
    fread(&total_chars, sizeof(int), 1, input);
    Node *root = read_tree(input);

    // Read the filenames
    int count_of_files;
    fread(&count_of_files, sizeof(int), 1, input);
    char **files = (char **)malloc(count_of_files * sizeof(char *));
    if (files == NULL) {
        fprintf(stderr, "Memory allocation failed.\n");
        fclose(input);
        return 1;
    }

    for (int i = 0; i < count_of_files; i++){
        // Read the length of the filename
        int filename_length;
        fread(&filename_length, sizeof(int), 1, input);
        files[i] = (char *)malloc((filename_length + 1) * sizeof(char));
        if (files[i] == NULL) {
            fprintf(stderr, "Memory allocation failed.\n");
            // Free memory allocated so far
            for (int j = 0; j < i; j++) {
                free(files[j]);
            }
            free(files);
            fclose(input);
            return 1;
        }
        // Read the filename
        fread(files[i], sizeof(char), filename_length, input);
        // Null-terminate the string
        files[i][filename_length] = '\0';
    }

    // Read the size of each file
    int *file_sizes = (int *)malloc(count_of_files * sizeof(int));
    if (file_sizes == NULL) {
        fprintf(stderr, "Memory allocation failed.\n");
        // Free memory allocated so far
        for (int i = 0; i < count_of_files; i++) {
            free(files[i]);
        }
        free(files);
        fclose(input);
        return 1;
    }
    fread(file_sizes, sizeof(int), count_of_files, input);

    // Set the offsets for each file based on the byte position in the file
    int *offsets = (int *)malloc(count_of_files * sizeof(int));
    if (offsets == NULL) {
        fprintf(stderr, "Memory allocation failed.\n");
        // Free memory allocated so far
        for (int i = 0; i < count_of_files; i++) {
            free(files[i]);
        }
        free(files);
        fclose(input);
        return 1;
    }
    long current_position = ftell(input);
    for (int i = 0; i < count_of_files; i++){
        if (i == 0){
            offsets[i] = current_position; 
        }
        else {
            offsets[i] = offsets[i-1] + get_bytes(input, file_sizes[i - 1], root);
        }
    }

    // Create the output directory if it doesn't exist
    mkdir("output", 0777);

    /* START OF PARALLEL DECOMPRESSION*/

    int active_processes = 0;
    for (int i = 0; i < count_of_files; i++){
        if (active_processes == MAX_PROCESSES){
            wait(NULL);
            active_processes--;
        }

        pid_t pid = fork();
        if (pid == 0){
            long offset = offsets[i];
            char output_filepath[256];
            snprintf(output_filepath, sizeof(output_filepath), "output/%s", files[i]);
            FILE *output_file = fopen(output_filepath, "w");
            if (output_file == NULL) {
                perror("fopen");
                return 1;
            }
            FILE *child_input = fopen(input_filename, "rb");
            if (child_input == NULL) {
                perror("fopen");
                return 1;
            }
            fseek(child_input, offset, SEEK_SET);
            long input_offset = ftell(child_input);
            decompress(child_input, output_file, root, file_sizes[i]);
            long output_offset = ftell(output_file);
            fclose(child_input);
            fclose(output_file);
            exit(0);
        } else {
            active_processes++;
        }
    }

    while (active_processes > 0){
        wait(NULL);
        active_processes--;
    }

    /* END OF PARALLEL DECOMPRESSION*/

    for (int i = 0; i < count_of_files; i++){
        free(files[i]);
    }
    free(files);
    return 0;
}

int main(int argc, char *argv[]) {
    setlocale(LC_ALL, "");

    struct timeval start, end;
    double time_taken;

    int serial = 1;
    int concurrent = 1;
    int parallel = 1;
    int opt;

    // Parse command line arguments to determine which decompression method to use
    // If no method is specified, use all of them
    while ((opt = getopt(argc, argv, "scp")) != -1) {
        switch (opt) {
            case 's':
                concurrent = 0;
                parallel = 0;
                break;
            case 'c':
                serial = 0;
                parallel = 0;
                break;
            case 'p':
                serial = 0;
                concurrent = 0;
                break;
            default:
                fprintf(stderr, "Usage: %s [-scp] input_file\n", argv[0]);
                return 1;
        }
    }

    if (optind >= argc) {
        fprintf(stderr, "Expected input file\n");
        return 1;
    }

    FILE *input = fopen(argv[optind], "rb");
    if (input == NULL) {
        perror("fopen");
        return 1;
    }

    if (serial){
        printf("Serial decompression\n");
        gettimeofday(&start, NULL);
        serial_decompression_procedure(input);
        gettimeofday(&end, NULL);
        time_taken = ((end.tv_sec - start.tv_sec) * 1e9) + ((end.tv_usec - start.tv_usec) * 1e3);
        printf("Time taken: %f nanoseconds\n", time_taken);
    }

    if (concurrent){
        input = fopen(argv[optind], "rb");
        if (input == NULL) {
            perror("fopen");
            return 1;
        }

        printf("Concurrent decompression\n");
        gettimeofday(&start, NULL);
        concurrent_decompression_procedure(input);
        gettimeofday(&end, NULL);
        time_taken = ((end.tv_sec - start.tv_sec) * 1e9) + ((end.tv_usec - start.tv_usec) * 1e3);
        printf("Time taken: %f nanoseconds\n", time_taken);
    }

    if (parallel){
        input = fopen(argv[optind], "rb");
        if (input == NULL) {
            perror("fopen");
            return 1;
        }

        printf("Parallel decompression\n");
        gettimeofday(&start, NULL);
        parallel_decompression_procedure(input, argv[1]);
        gettimeofday(&end, NULL);
        time_taken = ((end.tv_sec - start.tv_sec) * 1e9) + ((end.tv_usec - start.tv_usec) * 1e3);
        printf("Time taken: %f nanoseconds\n", time_taken);
    }

    fclose(input);

    return 0;
}

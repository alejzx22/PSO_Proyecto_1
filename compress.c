#include "huffman.h"
#include <dirent.h>
#include <pthread.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

// Constants
#define MAX_BOOKS 98
#define NUM_THREADS 8
#define BUFFER_SIZE 8192
#define MAX_PROCESSES 16

// Structs

typedef struct {
    char path[262];
    wint_t *frequencies;
    unsigned int total;
    pthread_mutex_t *mutex;
} ThreadArgs;

// Struct to pass arguments to the compression thread
typedef struct {
    char input[262];
    char output[262];
    Node *root;
} CompressionArgs;

// Function prototypes

int count_files(char *path);
char** load_file_names(char *path);
void delete_files(char **files, int count, char *directory);
void copy_file(FILE *input, FILE *output);
void print_files(char **files, int count);
FILE *open_file(char *path, char *mode);
void serial_compression();

void concurrent_compression();
void* concurrent_frequencies(void *args);
void* compression_thread(void *args);

void parallel_compression();

// Function definitions

int count_files(char *path){
    DIR *dir;
    struct dirent *entry;
    int count = 0;

    if (path == NULL){
        return -1;
    }

    if((dir = opendir(path)) == NULL){
        perror("Could not open: directory./n");
        return -1;
    }

    while((entry = readdir(dir)) != NULL){
        if(strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
        continue;
        count++;
    }

    closedir(dir);
    return count;
}

char** load_file_names(char *path){
    DIR *dir;
    struct dirent *entry;
    char **files;
    int count = 0;

    if (path == NULL){
        return NULL;
    }

    count = count_files(path);
    files = (char**)malloc(count * sizeof(char*));

    if((dir = opendir(path)) == NULL){
        perror("opendir");
        return NULL;
    }

    int i = 0;
    while((entry = readdir(dir)) != NULL){
        if(strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
        continue;
        files[i] = strdup(entry->d_name);
        i++;
    }

    closedir(dir);
    return files;
}

void delete_files(char **files, int count, char *directory){
    for (int i = 0; i < count; i++){
        char path[262];
        snprintf(path, sizeof(path), "%s/%s", directory, files[i]);
        if (remove(path) == -1){
            perror("Error deleting file\n");            
        }
    }
    if (rmdir(directory) == -1){
        perror("Error deleting directory\n");
    }
}

void copy_file(FILE *input, FILE *output){
    char buffer[BUFFER_SIZE];
    size_t bytes;
    while ((bytes = fread(buffer, 1, BUFFER_SIZE, input)) > 0){
        fwrite(buffer, 1, bytes, output);
    }
}

void print_files(char **files, int count){
    printf("Loaded %d files.\n", count);
    for (int i = 0; i < count; i++){
        printf("%s\n", files[i]);
    }
}

FILE *open_file(char *path, char *mode){
    FILE *file = fopen(path, mode);
    if (file == NULL){
        perror("Cannot open file.\n");
        return NULL;
    }
    return file;
}

int main2(int argc, char *argv[]) {
    FILE *file = fopen("books/A Christmas Carol in Prose; Being a Ghost Story of Christmas by Charles Dickens (12062).txt", "r");
    if (file == NULL) {
        printf("Error: Could not open file\n");
        return 1;
    }
    FILE *frequency_file = fopen("frequencies.txt", "w");
    if (frequency_file == NULL) {
        printf("Error: Could not open file\n");
        return 1;
    }

    wint_t frequencies[NUM_CHARS] = {0};
    unsigned int total = get_frequencies(file, frequencies);
    fclose(file);
    for (int i = 0; i < NUM_CHARS; i++) {
        if (frequencies[i] > 0) {
            fprintf(frequency_file, "%lc %u\n", i, frequencies[i]);
        }
    }
    fclose(frequency_file);
    return 0;
}


// Write header to the output file
void write_metadata(FILE *output, Node *root, int unique, int total, char **files, int count_of_files, int total_chars[]) {
    // Write the number of unique characters
    fwrite(&unique, sizeof(int), 1, output);
    // Write the total number of characters
    fwrite(&total, sizeof(int), 1, output);
    // Write the Huffman tree
    write_tree(output, root);
    // Write the names of the files
    fwrite(&count_of_files, sizeof(int), 1, output);
    for (int i = 0; i < count_of_files; i++){
        int len = strlen(files[i]);        fwrite(&len, sizeof(int), 1, output);
        fwrite(files[i], sizeof(char), len, output);
    }
    // Write the size of each file    
    fwrite(total_chars, sizeof(int), count_of_files, output);    
}

void serial_compression(){
    FILE *output = open_file("compressed.bin", "wb");
    if (output == NULL) return;

    int count_of_files = count_files("books");
    char **files = load_file_names("books");
    unsigned int total_chars[count_of_files];

    wint_t frequencies[NUM_CHARS] = {0};
    unsigned int total = 0;
    
    // Count the frequency of each character in the file
    for (int i = 0; i < count_of_files; i++){
        char path[262];
        snprintf(path, sizeof(path), "books/%s", files[i]);
        FILE *file = open_file(path, "r");
        if (file == NULL){
            return;
        }
        // Use a pointer to pass the get_frequencies function to store the size of the file
        unsigned int total_file = get_frequencies(file, frequencies);
        total_chars[i] = total_file;        
        total += total_file;
        fclose(file);
    }


    // Build the Huffman tree
    int unique = 0;
    Node *root = NULL;
    root = build_tree(frequencies, &unique);
    assign_codes(root, "");
     

    // Write the metadata to the output file
    write_metadata(output, root, unique, total, files, count_of_files, total_chars);

    // Write the compressed data to the output file
    for (int i = 0; i < count_of_files; i++){
        char path[263];
        snprintf(path, sizeof(path), "books/%s", files[i]);
        FILE *file = open_file(path, "r");
        if (file == NULL){
            return;
        }

        compress(file, output, root);
        fclose(file);
    }
    fclose(output);
    free(files);
}


// function to get the frequency of each character in the file using pthreads
void* concurrent_frequencies(void *args){    
    ThreadArgs *thread_args = (ThreadArgs *)args;
    FILE* file = open_file(thread_args->path, "r");
    if (file != NULL){
        pthread_mutex_lock(thread_args->mutex);
        thread_args->total = get_frequencies(file, thread_args->frequencies);
        pthread_mutex_unlock(thread_args->mutex);
        fclose(file);
    }
    return NULL;
}

void* compression_thread(void *args){
    CompressionArgs *compression_args = (CompressionArgs *)args;
    FILE *file = open_file((char *)compression_args->input, "r");
    FILE *output = open_file((char *)compression_args->output, "wb");
    if (file != NULL && output != NULL){
        compress(file, output, compression_args->root);
        fclose(file);
        fclose(output);
    }
    return NULL;
}

// function to compress files in concurrently using pthreads
void concurrent_compression(){
    FILE *output = open_file("compressed.bin", "wb");
    if (output == NULL) return;

    int count_of_files = count_files("books");
    char **files = load_file_names("books");

    wint_t frequencies[NUM_CHARS] = {0};
    unsigned int files_total[count_of_files];
    unsigned int total = 0;
    
    // Count the frequency of each character in the file in parallel
    pthread_t threads[NUM_THREADS];
    ThreadArgs thread_args[NUM_THREADS];
    pthread_mutex_t mutex;
    pthread_mutex_init(&mutex, NULL);

    for (int i = 0; i < count_of_files; i+=NUM_THREADS){
        int active_threads = NUM_THREADS;
        if (i + NUM_THREADS > count_of_files){
            active_threads = count_of_files - i;
        }

        for (int j = 0; j < active_threads; j++){            
            snprintf(thread_args[j].path, 262, "books/%s", files[i + j]);           
            thread_args[j].frequencies = frequencies;
            thread_args[j].total = 0;
            thread_args[j].mutex = &mutex;
            pthread_create(&threads[j], NULL, concurrent_frequencies, &thread_args[j]);
        }

        for (int k = 0; k < active_threads; k++){
            pthread_join(threads[k], NULL);
            files_total[i + k] = thread_args[k].total;
            total += thread_args[k].total;
        }
    }

    pthread_mutex_destroy(&mutex);
    
    // Build the Huffman tree
    Node *root = NULL;
    int unique = 0;
    root = build_tree(frequencies, &unique);
    assign_codes(root, "");

    // Write the metadata to the output file
    write_metadata(output, root, unique, total, files, count_of_files, files_total);
    fclose(output);


    /* COMPRESSION*/
    // create a temporary directory to store the compressed files
    if (mkdir("temps", 0777) == -1){
        perror("Error creating temporary directory\n");
        exit(1);
    }

    CompressionArgs compression_args[NUM_THREADS];    

    // Write the compressed data to temporal files
    for (int i = 0; i < count_of_files; i+=NUM_THREADS){
        int active_threads = NUM_THREADS;
        if (i + NUM_THREADS > count_of_files){
            active_threads = count_of_files - i;
        }

        for (int j = 0; j < active_threads; j++){            
            snprintf(compression_args[j].input, 262, "books/%s", files[i + j]);
            snprintf(compression_args[j].output, 262, "temps/%s", files[i + j]);
            compression_args[j].root = root;
            pthread_create(&threads[j], NULL, compression_thread, &compression_args[j]);
        }

        for (int k = 0; k < active_threads; k++){
            pthread_join(threads[k], NULL);            
        }
    }

    int temp_count = count_files("temps");
    char **temp_files = load_file_names("temps");
    
    // Write the compressed data from the temps to the output file
    // open the output file in append binary mode
    output = open_file("compressed.bin", "ab");
    for (int i = 0; i < temp_count; i++){
        char path[263];
        snprintf(path, sizeof(path), "temps/%s", temp_files[i]);
        FILE *file = open_file(path, "r");
        if (file == NULL){
            return;
        }
        copy_file(file, output);
        fclose(file);
    }

    // Delete the temporary directory and files
    delete_files(files, count_of_files, "temps");
    fclose(output);
    free(files);
    free(temp_files);
}

void parallel_frequencies(char *filename){
    wint_t *frequencies = calloc(NUM_CHARS, sizeof(wint_t));
    unsigned int total = 0;
    char path[262];
    snprintf(path, sizeof(path), "books/%s", filename);    
    FILE *file = open_file(path, "r");
    if (file == NULL){
        return;
    }
    total = get_frequencies(file, frequencies);
    char parallel_path[262];
    snprintf(parallel_path, 262, "parallel/%s", filename);    
    FILE *temp = open_file(parallel_path, "wb");
    if (temp == NULL){
        perror("Error opening temporary file\n");
        return;
    }

    fwrite(frequencies, sizeof(wint_t), NUM_CHARS, temp);
    fwrite(&total, sizeof(unsigned int), 1, temp);
    fclose(file);
    fclose(temp);
    free(frequencies);
}

unsigned int add_frequencies(const char *filename, wint_t *total_frequencies, unsigned int *total_total) {
    wint_t *frequencies = calloc(NUM_CHARS, sizeof(wint_t));
    unsigned int total;
    char path[262];
    snprintf(path, sizeof(path), "parallel/%s", filename);
    FILE *temp = fopen(path, "rb");
    if (temp == NULL){
        perror("Error opening temporary file\n");
        return 0;
    }

    fread(frequencies, sizeof(wint_t), NUM_CHARS, temp);
    fread(&total, sizeof(unsigned int), 1, temp);
    fclose(temp);

    for (int i = 0; i < NUM_CHARS; i++) {
        total_frequencies[i] += frequencies[i];
    }
    *total_total += total;
    free(frequencies);
    return total;
}

void parallel_compression_worker(char *input_name, char *output_name, Node *root){            
    FILE *input = open_file(input_name, "r");
    FILE *output = open_file(output_name, "wb");
    if (input != NULL && output != NULL){
        compress(input, output, root);
        fclose(input);
        fclose(output);
    }
}


// function to compress files parallelly using processes
void parallel_compression(){
    FILE *output = open_file("compressed.bin", "wb");
    if (output == NULL) return;

    int count_of_files = count_files("books");
    char **files = load_file_names("books");
    unsigned int total_chars[count_of_files];

    wint_t frequencies[NUM_CHARS] = {0};
    unsigned int total = 0;
    
    // Count the frequency of each character in the file
    for (int i = 0; i < count_of_files; i++){
        char path[262];
        snprintf(path, sizeof(path), "books/%s", files[i]);
        FILE *file = open_file(path, "r");
        if (file == NULL){
            return;
        }
        // Use a pointer to pass the get_frequencies function to store the size of the file
        unsigned int total_file = get_frequencies(file, frequencies);
        total_chars[i] = total_file;        
        total += total_file;
        fclose(file);
    }

    // Build the Huffman tree
    Node *root = NULL;
    int unique = 0;
    root = build_tree(frequencies, &unique);
    assign_codes(root, "");

    // Write the metadata to the output file
    write_metadata(output, root, unique, total, files, count_of_files, total_chars);
    fclose(output);

    /* COMPRESSION*/
    // create a temporary directory to store the compressed files
    if (mkdir("temporals", 0777) == -1){
        perror("Error creating temporary directory\n");
        exit(1);
    }
    
    // Write the compressed data to temporal files
    int active_processes = 0;
    for (int i = 0; i < count_of_files; i++){
        if (active_processes == MAX_PROCESSES){
            wait(NULL);
            active_processes--;
        }
        pid_t pid = fork();
        if (pid == 0){
            char input_path[262];
            char output_path[262];
            snprintf(input_path, sizeof(input_path), "books/%s", files[i]);
            snprintf(output_path, sizeof(output_path), "temporals/%s", files[i]);
            parallel_compression_worker(input_path, output_path, root);
            exit(0);
        } else if (pid > 0){
            active_processes++;
        } else {
            perror("Error creating child process\n");
            exit(1);
        }
    }

    while (active_processes > 0){
        wait(NULL);
        active_processes--;
    }

    

    char **temp_files = load_file_names("temporals");
    int temp_count = count_files("temporals");

    // Write the compressed data from the temps to the output file
    // open the output file in append binary mode
    output = open_file("compressed.bin", "ab");
    for (int i = 0; i < temp_count; i++){
        char path[263];
        snprintf(path, sizeof(path), "temporals/%s", temp_files[i]);
        FILE *file = open_file(path, "r");
        if (file == NULL){
            return;
        }
        copy_file(file, output);
        fclose(file);
    }

    
    fclose(output);
    delete_files(files, count_of_files, "temporals");
    free(temp_files);
    free(files);
}


int main(int argc, char *argv[]) {
    int parallel = 1;
    int concurrent = 1;
    int serial = 1;
    int opt;
    while ((opt = getopt(argc, argv, "pcs")) != -1){
        switch (opt){
            case 'p':
                serial = 0;
                concurrent = 0;
                break;
            case 'c':
                serial = 0;
                parallel = 0;
                break;
            case 's':
                parallel = 0;
                concurrent = 0;
                break;
            default:
                fprintf(stderr, "Usage: %s [-pcs]\n", argv[0]);
                exit(EXIT_FAILURE);
        }
    }
    

    struct timeval start, end;
    double time_taken;

    if (serial) {
        printf("Serial compression\n");
        gettimeofday(&start, NULL);
        serial_compression();
        gettimeofday(&end, NULL);
        time_taken = ((end.tv_sec - start.tv_sec) * 1e9) + ((end.tv_usec - start.tv_usec) * 1e3);
        printf("Time taken: %f nanoseconds\n", time_taken);
    }

    if (concurrent) {
        printf("Concurrent compression\n");
        gettimeofday(&start, NULL);
        concurrent_compression();
        gettimeofday(&end, NULL);
        time_taken = ((end.tv_sec - start.tv_sec) * 1e9) + ((end.tv_usec - start.tv_usec) * 1e3);
        printf("Time taken: %f nanoseconds\n", time_taken);
    }

    if (parallel) {
        printf("Parallel compression\n");
        gettimeofday(&start, NULL);
        parallel_compression();
        gettimeofday(&end, NULL);
        time_taken = ((end.tv_sec - start.tv_sec) * 1e9) + ((end.tv_usec - start.tv_usec) * 1e3);
        printf("Time taken: %f nanoseconds\n", time_taken);
    }

    return 0;
}

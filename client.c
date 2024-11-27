// Client side C program to demonstrate Socket
// programming
#include <arpa/inet.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
#include <pthread.h>
#include <stdlib.h>
#define PORT 8080

static pthread_mutex_t file_mutex = PTHREAD_MUTEX_INITIALIZER;

int round_up_division(int a, int b)
{
    return (a + b - 1) / b;
}

unsigned char checksum(unsigned char *ptr, size_t sz)
{
    unsigned char chk = 0;
    while (sz-- != 0)
        chk -= *ptr++;
    return chk;
}
// simple checksum function borrowed from StackOverflow
// https://stackoverflow.com/a/3464036

int create_file_with_size(const char *filename, int file_size)
{
    FILE *rec_file = fopen(filename, "wb"); // used for writing in binary files too
    printf("Creating file to receive data rn...\n");
    if (rec_file == NULL)
    {
        perror("Client: Error creating file");
        return -1;
    }
    fclose(rec_file);

    // Preallocate file size
    if (truncate(filename, file_size) != 0)
    {
        perror("Client: Error preallocating file size");
        return -1;
    }
    return 0;
}

// TODO: include checksum in this struct
struct chunk_data
{
    int client_socket; // needed for TCP connection
    int start;
    int size;
    char *file_name;
};

static int t_count = 0;

// function which receives chunks from server
void* receive_chunks(void* args)
{
    // pthread_mutex_lock(&file_mutex);
    struct chunk_data* data = (struct chunk_data*)args;
    int client_fd = data->client_socket;
    int size = data->size;
    unsigned char* buffer = (unsigned char*)(malloc(size));
    int start = 0;
    int valread = read(client_fd, &start, sizeof(int));
    valread = read(client_fd, buffer, size);
    printf("Receiving chunk of size %d from position %d\n", size, start);
    if (valread < 0)
    {
        printf("Client: Error reading chunk from server\n");
        return NULL;
    }
    if (valread != size)
    {
        printf("Warning: Read mismatched chunk size: %d\n", valread);
        fflush(stdout);
    }
    printf("Client: Thread %d {\n%s\n}, position %d\n", ++t_count, buffer, data->start);

    // appending chunk to file
    // using some random file for now
    // char* file_name = (char*)(malloc(256));
    char file_name[256];
    snprintf(file_name, 256, "received_files/%s", data->file_name); // Construct the file path

    printf("File path: %s\n", file_name);
    FILE* file = fopen(file_name, "rb+");

    printf("Writing from position %d\n", start);

    if (file == NULL)
    {
        printf("Client: Error opening file %s\n", file_name);
        return NULL;
    }
    fseek(file, start, SEEK_SET);
    fwrite(buffer, 1, valread, file);
    // rewind(file);

    // t_count += 1;

    // TODO: check checksum here

    // close file
    fclose(file);
    pthread_mutex_unlock(&file_mutex);

    // free buffer
    free(buffer);
    // free(file_name);
    // printf("Chunk received and written to file from thread %ld\n", pthread_self());
    return NULL;
}

int main(int argc, char const *argv[])
{
    // proper usage check
    if (argc < 3)
    {
        printf("Usage: %s <file_name> <number_of_concurrent_sessions>\n", argv[0]);
        return -1;
    }

    // store and assign file name
    // char* file_name = (char*)(malloc(strlen(argv[1]) + 1));
    char file_name[256];
    strcpy(file_name, argv[1]);

    int no_of_threads = atoi(argv[2]);

    // int status, valread, client_fd;
    int status, valread, client_fd;
    struct sockaddr_in serv_addr;
    // char *hello = "Hello from client";
    // char buffer[1024] = {0};
    if ((client_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    {
        printf("\n Socket creation error \n");
        return -1;
    }

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(PORT);

    // Convert IPv4 and IPv6 addresses from text to binary
    // form
    if (inet_pton(AF_INET, "127.0.0.1", &serv_addr.sin_addr) <= 0)
    {
        printf(
            "\nInvalid address/ Address not supported \n");
        return -1;
    }

    if ((status = connect(client_fd, (struct sockaddr *)&serv_addr,
                          sizeof(serv_addr))) < 0)
    {
        printf("\nConnection Failed \n");
        printf("\nPerhaps the server is not running\n");
        return -1;
    }
    // send(client_fd, hello, strlen(hello), 0);

    // sending arguments to background server
    // printf("Size of file name: %ld\n", sizeof(file_name));
    send(client_fd, file_name, sizeof(file_name), 0);
    // Send number of threads
    if (send(client_fd, &no_of_threads, sizeof(int), 0) <= 0)
    {
        perror("Client: Error sending number of threads");
        close(client_fd);
        return -1;
    }
    printf("File transfer request sent!\n");
    // printf("Client wants to send %s with %d threads\n", file_name, no_of_threads);

    char *rec_file_name = (char *)(malloc(256));
    int _ = snprintf(rec_file_name, 256, "received_files/%s", file_name); // Construct the file path
    _ += 0;

    // receiving response from server
    int file_size;
    valread = read(client_fd, &file_size, sizeof(int));
    printf("Size of file: %d\n", file_size);
    valread += 0; // to avoid unused variable warning
    // printf("The size of the file is %d\n", file_size);
    // printf("Trying to clear %s\n", rec_file_name);

    FILE *temp = fopen(rec_file_name, "wb");
    if (temp == NULL)
    {
        printf("Client: Error opening file %s\n", rec_file_name);
        perror("Client: Error opening file");
        close(client_fd);
        return -1;
    }
    fclose(temp);

    if (create_file_with_size(rec_file_name, file_size) == 0)
    {
        printf("File created successfully: %s\n", rec_file_name);
    }
    else
    {
        printf("Failed to create file\n");
    }

    // create threads
    pthread_t* threads = (pthread_t*)(malloc(no_of_threads * sizeof(pthread_t)));
    // printf("%d threads created\n", no_of_threads);
    for (int i = 0; i < no_of_threads; i++)
    {
        struct chunk_data* data = (struct chunk_data*)(malloc(sizeof(struct chunk_data)));
        data->client_socket = client_fd;
        data->start = i * round_up_division(file_size, no_of_threads);
        if (i == no_of_threads - 1)
        {
            data->size = file_size - data->start;
        }
        else
        {
            data->size = round_up_division(file_size, no_of_threads);
        }
        data->file_name = file_name;
        pthread_create(&threads[i], NULL, receive_chunks, data);
        // printf("Thread %d created\n", i);
    }

    // join threads
    for (int i = 0; i < no_of_threads; i++)
    {
        pthread_join(threads[i], NULL);
    }

    // receiving checksum
    // calculating checksum

    // closing the connected socket
    close(client_fd);

    // free data here
    free(threads);
    free(rec_file_name);
    
    return 0;
}

// TODO: perform comparison to check if file is received correctly

// Borrowed from Geeksforgeeks
// https://www.geeksforgeeks.org/socket-programming-cc/
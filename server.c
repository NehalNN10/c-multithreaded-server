// Server side C program to demonstrate Socket
// programming
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
#include <pthread.h>

#define PORT 8080

// * file reading is thread safe

static int t_count = 0;

struct chunk_data
{
    int client_socket; // needed for TCP connection
    int start;
    int size;
    char *file_name;
};

void* send_chunk(void *args)
{
    struct chunk_data *data = (struct chunk_data *)args;
    int client_fd = data->client_socket;
    int start = data->start;
    int size = data->size;
    char *file_name = data->file_name;

    // open file
    FILE *file = fopen(file_name, "rb");
    if (file == NULL)
    {
        printf("Server: Error opening file %s\n", file_name);
        return NULL;
    }

    // seek to start
    fseek(file, start, SEEK_SET);

    // read chunk
    unsigned char buffer[size]; // ? might need to change based on whether file is image or text
    int read_bytes = fread(buffer, 1, size, file);
    if (read_bytes < 0)
    {
        printf("Server: Error reading file %s\n", file_name);
        return NULL;
    }

    // printf("Sending chunk of size %d\n", read_bytes);
    // printf("Sending chunk of size %d\n", size);
    // fflush(stdout);

    // send chunk
    send(client_fd, buffer, read_bytes, 0);
    printf("Server: Thread %d {\n%s\n}, position %d\n", ++t_count, buffer, start);
    fflush(stdout); // Ensure immediate printing

    // close file
    fclose(file);

    free(data);

    return NULL;
}

void *handle_client(void *socket_ptr)
{
    int client_socket = *(int *)socket_ptr;
    free(socket_ptr);

    char file_name[256];
    int no_of_threads;

    if (recv(client_socket, file_name, sizeof(file_name), 0) <= 0)
    {
        perror("Server: Error receiving file name");
        close(client_socket);
        return NULL;
    }
    // printf("File name received: %s\n", file_name);

    if (recv(client_socket, &no_of_threads, sizeof(int), 0) <= 0)
    {
        perror("Server: Error receiving number of threads");
        close(client_socket);
        return NULL;
    }
    // printf("Number of threads received: %d\n", no_of_threads);

    char f_name[256];
    int _ = snprintf(f_name, 256, "test_files/%s", file_name); // constructing the file path
    // printf("Server file path: %s\n", f_name);
    _ += 0;

    FILE *file = fopen(f_name, "rb");
    // printf("Opening file %s\n", f_name);
    if (file == NULL)
    {
        perror("Server: Error opening file");
        close(client_socket);
        return NULL;
    }
    fseek(file, 0, SEEK_END);
    int file_size = ftell(file);
    rewind(file);

    send(client_socket, &file_size, sizeof(int), 0);

    pthread_t *threads = malloc(no_of_threads * sizeof(pthread_t));
    for (int i = 0; i < no_of_threads; i++)
    {
        struct chunk_data *data = malloc(sizeof(struct chunk_data));
        data->client_socket = client_socket;
        data->file_name = f_name;
        data->start = i * (file_size / no_of_threads);
        data->size = (i == no_of_threads - 1) ? (file_size - data->start) : (file_size / no_of_threads);

        pthread_create(&threads[i], NULL, send_chunk, data);
    }

    for (int i = 0; i < no_of_threads; i++)
    {
        pthread_join(threads[i], NULL);
    }

    free(threads);
    fclose(file);
    close(client_socket);
    printf("Client handled and socket closed\n");
    return NULL;
}

int main()
{
    int server_fd;
    struct sockaddr_in address;
    int opt = 1;
    socklen_t addrlen = sizeof(address);

    // creating socket file descriptor
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    {
        perror("Server: Socket failed");
        exit(EXIT_FAILURE);
    }

    // Forcefully attaching socket to the port 8080
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt)))
    {
        perror("setsockopt");
        exit(EXIT_FAILURE);
    }
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);

    // Forcefully attaching socket to the port 8080
    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0)
    {
        perror("bind failed");
        exit(EXIT_FAILURE);
    }

    if (listen(server_fd, 3) < 0)
    {
        perror("listen");
        exit(EXIT_FAILURE);
    }

    printf("Server is running and waiting for connections...\n");

    while (1)
    {
        int *socket_ptr = malloc(sizeof(int));
        *socket_ptr = accept(server_fd, (struct sockaddr *)&address, &addrlen);
        if (*socket_ptr < 0)
        {
            perror("accept");
            free(socket_ptr);
            continue;
        }

        pthread_t client_thread;
        pthread_create(&client_thread, NULL, handle_client, socket_ptr);
        pthread_detach(client_thread);
    }

    close(server_fd);
    return 0;
}

// Borrowed from Geeksforgeeks
// https://www.geeksforgeeks.org/socket-programming-cc/
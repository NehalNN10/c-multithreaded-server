#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <semaphore.h>
#include <sys/socket.h>

#define PORT 8080
#define MAX_CLIENTS 100
#define FILENAME_SIZE 256

sem_t file_semaphore; // Semaphore to manage file access

struct chunk_data
{
    int client_socket;
    int start;
    int size;
    char *file_name;
};

int round_up_division(int a, int b)
{
    return (a + b - 1) / b;
}

void *send_chunk(void *args)
{
    struct chunk_data *data = (struct chunk_data *)args;

    // Lock file access
    sem_wait(&file_semaphore);
    FILE *file = fopen(data->file_name, "rb");
    if (!file)
    {
        perror("[SERVER] Error opening file");
        sem_post(&file_semaphore);
        free(data);
        return NULL;
    }

    fseek(file, data->start, SEEK_SET);
    unsigned char *buffer = malloc(data->size);
    if (!buffer)
    {
        perror("[SERVER] Memory allocation failed");
        fclose(file);
        sem_post(&file_semaphore);
        free(data);
        return NULL;
    }

    int read_bytes = fread(buffer, 1, data->size, file);
    fclose(file);
    sem_post(&file_semaphore);

    if (read_bytes < 0)
    {
        perror("[SERVER] Error reading file");
        free(buffer);
        free(data);
        return NULL;
    }

    // Send metadata (start and size)
    int header[2] = {data->start, read_bytes};
    send(data->client_socket, header, sizeof(header), 0);

    // Send the chunk data
    send(data->client_socket, buffer, read_bytes, 0);

    // Cleanup
    free(buffer);
    close(data->client_socket);
    free(data);

    return NULL;
}

void *handle_client(void *socket_ptr)
{
    int client_socket = *(int *)socket_ptr;
    free(socket_ptr);

    char file_name[FILENAME_SIZE];
    int thread_id;
    int no_of_threads;

    // Receive file name and thread ID
    recv(client_socket, file_name, sizeof(file_name), 0);
    recv(client_socket, &thread_id, sizeof(int), 0);
    recv(client_socket, &no_of_threads, sizeof(int), 0);

    printf("[SERVER] Received request for thread %d\n", thread_id);

    // Calculate chunk metadata
    FILE *file = fopen(file_name, "rb");
    if (!file)
    {
        perror("[SERVER] Error opening file");
        close(client_socket);
        return NULL;
    }

    fseek(file, 0, SEEK_END);
    int file_size = ftell(file);
    fclose(file);

    int chunk_size = round_up_division(file_size, no_of_threads);
    int start = thread_id * chunk_size;
    int size = (thread_id == no_of_threads - 1) ? (file_size - start) : chunk_size;

    // Prepare and launch a thread to handle the chunk
    struct chunk_data *data = malloc(sizeof(struct chunk_data));
    data->client_socket = client_socket;
    data->start = start;
    data->size = size;
    data->file_name = strdup(file_name);

    pthread_t chunk_thread;
    pthread_create(&chunk_thread, NULL, send_chunk, data);
    pthread_detach(chunk_thread);

    return NULL;
}

int main()
{
    int server_fd;
    struct sockaddr_in address;
    int opt = 1;
    socklen_t addrlen = sizeof(address);

    sem_init(&file_semaphore, 0, 1); // Initialize semaphore

    // Set up server socket
    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt));
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);

    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0)
    {
        perror("[SERVER] Error binding socket");
        return -1;
    }

    listen(server_fd, MAX_CLIENTS);

    printf("Server is running and waiting for connections...\n");

    while (1)
    {
        int *client_socket = malloc(sizeof(int));
        *client_socket = accept(server_fd, (struct sockaddr *)&address, &addrlen);
        if (*client_socket < 0)
        {
            perror("[SERVER] Accept failed");
            free(client_socket);
            continue;
        }

        printf("[SERVER] New connection accepted\n");

        pthread_t client_thread;
        pthread_create(&client_thread, NULL, handle_client, client_socket);
        pthread_detach(client_thread);
    }

    sem_destroy(&file_semaphore);
    close(server_fd);
    return 0;
}
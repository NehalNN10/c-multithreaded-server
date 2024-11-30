#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>

#define PORT 8080
#define FILENAME_SIZE 256

struct thread_data
{
    int thread_id;
    int client_socket;
    char *file_name;
    int no_of_threads;
};

void *receive_chunk(void *args)
{
    struct thread_data *data = (struct thread_data *)args;

    // Send file name and thread ID
    send(data->client_socket, data->file_name, FILENAME_SIZE, 0);
    send(data->client_socket, &data->thread_id, sizeof(int), 0);
    send(data->client_socket, &data->no_of_threads, sizeof(int), 0);

    // Receive metadata
    int header[2];
    recv(data->client_socket, header, sizeof(header), 0);
    int start = header[0];
    int size = header[1];

    // Receive the chunk data
    unsigned char *buffer = malloc(size);
    recv(data->client_socket, buffer, size, 0);

    // Save the chunk to a file
    FILE *file = fopen(data->file_name, "r+b");
    if (file)
    {
        fseek(file, start, SEEK_SET);
        fwrite(buffer, 1, size, file);
        fclose(file);
    }
    else
    {
        perror("[CLIENT] Error opening file for writing");
    }

    free(buffer);
    free(data);
    return NULL;
}

int main(int argc, char *argv[])
{
    if (argc < 3)
    {
        fprintf(stderr, "Usage: %s <server_ip> <file_name> <no_of_threads>\n", argv[0]);
        return 1;
    }

    const char *server_ip = argv[1];
    const char *file_name = argv[2];
    int no_of_threads = atoi(argv[3]);

    int client_socket;
    struct sockaddr_in server_address;

    client_socket = socket(AF_INET, SOCK_STREAM, 0);
    server_address.sin_family = AF_INET;
    server_address.sin_port = htons(PORT);
    inet_pton(AF_INET, server_ip, &server_address.sin_addr);

    if (connect(client_socket, (struct sockaddr *)&server_address, sizeof(server_address)) < 0)
    {
        perror("[CLIENT] Connection failed");
        return -1;
    }

    pthread_t threads[no_of_threads];
    for (int i = 0; i < no_of_threads; i++)
    {
        struct thread_data *data = malloc(sizeof(struct thread_data));
        data->thread_id = i;
        data->client_socket = client_socket;
        data->file_name = strdup(file_name);
        data->no_of_threads = no_of_threads;

        pthread_create(&threads[i], NULL, receive_chunk, data);
    }

    for (int i = 0; i < no_of_threads; i++)
    {
        pthread_join(threads[i], NULL);
    }

    close(client_socket);
    return 0;
}
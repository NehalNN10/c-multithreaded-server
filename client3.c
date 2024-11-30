#include <arpa/inet.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
#include <pthread.h>
#include <stdlib.h>
#include <semaphore.h>

#define PORT 8080

sem_t write_semaphore;

struct foo
{
    int client_fd;
    char *file_name;
    int no_of_threads;
};

void *receive_chunks(void *args)
{
    struct foo *data = (struct foo *)args;

    // Step 1: Create a new socket for this thread
    int thread_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (thread_socket < 0)
    {
        perror("Client: Socket creation failed");
        return NULL;
    }

    // Step 2: Connect the thread's socket to the server
    struct sockaddr_in serv_addr;
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(PORT);
    // inet_pton(AF_INET, "127.0.0.1", &serv_addr.sin_addr);

    // Convert IPv4 and IPv6 addresses from text to binary
    // form
    if (inet_pton(AF_INET, "127.0.0.1", &serv_addr.sin_addr) <= 0)
    {
        printf(
            "\nInvalid address/ Address not supported \n");
        return NULL;
    }

    // Connecting thread socket to server
    printf("[CLIENT] Attempting to connect to server\n");
    sleep(2);
    if (connect(thread_socket, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)
    {
        perror("Client: Connection failed");
        close(thread_socket);
        return NULL;
    }

    printf("Client Thread %d: Connected to server\n", data->client_fd);

    // Step 3: Send the file name and thread ID to the server
    send(thread_socket, data->file_name, strlen(data->file_name) + 1, 0);
    int thread_id = data->client_fd; // Use this as the "chunk ID"
    send(thread_socket, &thread_id, sizeof(int), 0);
    send(thread_socket, &data->no_of_threads, sizeof(int), 0);

    printf("Client Thread %d: Connected and requested chunk\n", thread_id);

    // Step 4: Receive metadata (start and size)
    int header[2];
    int valread = read(thread_socket, header, sizeof(header));
    if (valread != sizeof(header))
    {
        perror("Client: Error reading chunk metadata");
        close(thread_socket);
        return NULL;
    }

    int start = header[0];
    int size = header[1];

    // Step 5: Receive the chunk data
    unsigned char *buffer = malloc(size);
    if (!buffer)
    {
        perror("Client: Memory allocation failed");
        close(thread_socket);
        return NULL;
    }

    valread = read(thread_socket, buffer, size);
    if (valread != size)
    {
        printf("Client: Error reading chunk data for thread %d", thread_id);
        perror("Client: Error reading chunk data for thread");
        free(buffer);
        close(thread_socket);
        return NULL;
    }
    printf("Client Thread %d: Received chunk of size %d from position %d\n", thread_id, size, start);
    printf("Now beginning to write data to file %s\n", data->file_name);
    fflush(stdout);

    // Step 6: Write the chunk to the file
    sem_wait(&write_semaphore);
    FILE *file = fopen(data->file_name, "rb+");
    if (!file)
    {
        perror("Client: Error opening file for writing");
        sem_post(&write_semaphore);
        free(buffer);
        close(thread_socket);
        return NULL;
    }

    fseek(file, start, SEEK_SET);
    fwrite(buffer, 1, size, file);
    fclose(file);
    sem_post(&write_semaphore);

    // Cleanup
    free(buffer);
    close(thread_socket);

    printf("Client Thread %d: Chunk written and socket closed\n", thread_id);
    return NULL;
}

int main(int argc, char *argv[])
{
    if (argc != 3)
    {
        printf("Usage: %s <file_name> <number_of_threads>\n", argv[0]);
        return -1;
    }

    char file_name[256];
    strcpy(file_name, argv[1]);
    int no_of_threads = atoi(argv[2]);

    sem_init(&write_semaphore, 0, 1);

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
    printf("[CLIENT] Successfully connected to server\n");
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
    sleep(2);
    // printf("Client wants to send %s with %d threads\n", file_name, no_of_threads);

    char *rec_file_name = (char *)(malloc(256));
    int _ = snprintf(rec_file_name, 256, "received_files/%s", file_name); // Construct the file path
    _ += 0;

    // receiving response from server
    // int file_size;
    // valread = read(client_fd, &file_size, sizeof(int));
    // printf("Size of file: %d\n", file_size);
    // valread += 0; // to avoid unused variable warning
    // printf("The size of the file is %d\n", file_size);
    // printf("Trying to clear %s\n", rec_file_name);

    // FILE *temp = fopen(rec_file_name, "wb");
    // if (temp == NULL)
    // {
    //     printf("Client: Error opening file %s\n", rec_file_name);
    //     perror("Client: Error opening file");
    //     close(client_fd);
    //     return -1;
    // }
    // fclose(temp);

    // Create threads
    pthread_t threads[no_of_threads];
    struct foo thread_data[no_of_threads];
    for (int i = 0; i < no_of_threads; i++)
    {
        thread_data[i].client_fd = i;                 // Use as thread/chunk ID
        thread_data[i].file_name = strdup(file_name); // Duplicate file name
        thread_data[i].no_of_threads = no_of_threads;
        pthread_create(&threads[i], NULL, receive_chunks, &thread_data[i]);
    }

    // Join threads
    for (int i = 0; i < no_of_threads; i++)
    {
        pthread_join(threads[i], NULL);
        free(thread_data[i].file_name);
    }

    sem_destroy(&write_semaphore);
    return 0;
}

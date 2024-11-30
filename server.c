// Server side C program to demonstrate Socket
// programming
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <pthread.h>

#define PORT 8080

// * file reading is thread safe

static int server_fd = 0;

struct chunk_data
{
    int client_socket; // needed for TCP connection
    int start;
    int size;
    char *file_name;
    int thread_id;
};

int round_up_division(int a, int b)
{
    return (a + b - 1) / b;
}

char *sha_256_checksum(char *file_name)
{
    FILE *file = popen("sha256sum file_name", "r");
    char buffer[64];
    if (file == NULL)
    {
        perror("Error opening file");
        return NULL;
    }
    if (fscanf(file, "%64s", buffer) == 1)
    {
        printf("Checksum: %s\n", buffer);
    }
    pclose(file);
    return buffer;
}

// function to send chunks
void *send_chunk(void *args)
{
    struct chunk_data *data = (struct chunk_data *)args;
    // int client_fd = data->client_socket;
    printf("[SERVER] Thread %d: Sending chunk of size %d from position %d\n", data->thread_id, data->size, data->start);
    // int start = data->start;
    // int size = data->size;
    // char *file_name = data->file_name;

    // pthread_mutex_lock(&file_mutex);

    // open file
    printf("[SERVER] Opening file %s\n", data->file_name);
    FILE *file = fopen(data->file_name, "rb");
    if (file == NULL)
    {
        printf("[SERVER] Error opening file %s\n", data->file_name);
        return NULL;
    }
    printf("[SERVER] File %s opened\n", data->file_name);

    // seek to start
    fseek(file, data->start, SEEK_SET);

    // read chunk
    unsigned char buffer[data->size]; // ? might need to change based on whether file is image or text
    int read_bytes = fread(buffer, 1, data->size, file);
    printf("[SERVER] Read %d bytes from file %s\n", read_bytes, data->file_name);
    // printf("Bytes read: %d\n", read_bytes);
    // fflush(stdout);
    // printf("Size: %d\n", size);
    // fflush(stdout);
    if (read_bytes < 0)
    {
        printf("[SERVER] Error reading file %s\n", data->file_name);
        fflush(stdout);
        return NULL;
    }

    fclose(file);

    // send chunk
    printf("[SERVER] Thread has read chunk %d\n", data->thread_id);

    // establish separate socket for thread
    printf("[SERVER] Attempting to create new socket for thread %d\n", data->thread_id);
    struct sockaddr_in client_addr;
    socklen_t addr_len = sizeof(client_addr);

    printf("[SERVER] Waiting for a thread-specific connection...\n");

    // Accept a new connection for this thread
    int thread_socket = accept(server_fd, (struct sockaddr *)&client_addr, &addr_len);
    if (thread_socket < 0)
    {
        perror("[SERVER] Error accepting thread-specific connection");
        return NULL;
    }

    printf("[SERVER] Connection accepted\n");

    char client_ip[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &client_addr.sin_addr, client_ip, INET_ADDRSTRLEN);
    printf("[SERVER] Thread socket created. FD: %d, Client IP: %s, Port: %d\n",
           thread_socket, client_ip, ntohs(client_addr.sin_port));

    printf("[SERVER] Thread %d socket created\n", data->thread_id);

    // send thread number
    send(thread_socket, &data->thread_id, sizeof(int), 0);
    printf("[SERVER] Sent thread number: %d\n", data->thread_id);

    // send metadata
    int header[2] = {data->start, read_bytes};
    send(thread_socket, header, sizeof(header), 0);
    printf("[SERVER] Sent chunk metadata: start=%d, size=%d\n", data->start, data->size);

    // implement check whether chunk > 64kB
    if (read_bytes > 65536)
    {
        printf("[SERVER] Chunk too large for thread %d - sending in chunks\n", data->thread_id);
        int remaining_bytes = read_bytes;
        int i = 0;
        while (remaining_bytes > 0)
        {
            int chunk_size = (remaining_bytes > 65536) ? 65536 : remaining_bytes;
            send(thread_socket, &chunk_size, sizeof(int), 0);
            printf("[SERVER] Sent chunk size: %d for thread %d\n", chunk_size, data->thread_id);
            sleep(0.5);
            send(thread_socket, buffer + i * 65536, chunk_size, 0);
            printf("[SERVER] Sent chunk data for thread %d from offset %d\n", data->thread_id , i * 65536);
            remaining_bytes -= chunk_size;
            printf("[SERVER] Remaining bytes: %d for thread %d\n", remaining_bytes, data->thread_id);
            sleep(0.5);
            send(thread_socket, (remaining_bytes > 0) ? (char *)0 : (char *)1, sizeof(char), 0);
            printf("[SERVER] Sent flag for thread %d: %s\n", data->thread_id, (remaining_bytes > 0) ? "0" : "1");
            printf("[SERVER] Remaining bytes: %d for thread %d\n", remaining_bytes, data->thread_id);
            i++;
            if (remaining_bytes == 0)
            {
                break;
            }
        }
    }
    else
    {
        // send actual chunk
        send(thread_socket, buffer, read_bytes, 0);
    }
    printf("[SERVER] Sent chunk data\n");

    close(thread_socket);

    printf("[SERVER] Thread %d socket closed\n", data->thread_id);

    return NULL;
}

void *handle_client(void *socket_ptr)
{
    printf("[SERVER] Handling client\n");
    int client_socket = *(int *)socket_ptr;
    free(socket_ptr);

    char file_name[256];
    int no_of_threads;

    if (recv(client_socket, file_name, sizeof(file_name), 0) <= 0)
    {
        printf("[SERVER] Error receiving file name from thread\n");
        perror("Server: Error receiving file name");
        close(client_socket);
        return NULL;
    }

    if (recv(client_socket, &no_of_threads, sizeof(int), 0) <= 0)
    {
        perror("Server: Error receiving number of threads");
        close(client_socket);
        return NULL;
    }
    // printf("Number of threads received: %d\n", no_of_threads);

    printf("[SERVER] Received request for file %s with %d threads\n", file_name, no_of_threads);
    fflush(stdout);
    // exit(0);

    char f_name[256];
    int _ = snprintf(f_name, 256, "test_files/%s", file_name); // constructing the file path
    _ += 0;

    FILE *file = fopen(f_name, "rb");
    if (file == NULL)
    {
        printf("[SERVER] Error opening file %s\n", f_name);
        perror("Server: Error opening file");
        close(client_socket);
        return NULL;
    }
    fseek(file, 0, SEEK_END);
    int file_size = ftell(file);
    rewind(file);

    fclose(file);

    // send client the file size
    send(client_socket, &file_size, sizeof(int), 0);
    printf("[SERVER] Sent file size: %d\n", file_size);

    pthread_t *threads = malloc(no_of_threads * sizeof(pthread_t));
    for (int i = 0; i < no_of_threads; i++)
    {
        struct chunk_data *data = malloc(sizeof(struct chunk_data));
        data->client_socket = client_socket;
        data->file_name = malloc(strlen(f_name) + 1);
        if (data->file_name == NULL)
        {
            perror("Server: Error allocating memory for file_name");
            free(data);
            close(client_socket);
            return NULL;
        }
        strcpy(data->file_name, f_name);
        data->start = i * round_up_division(file_size, no_of_threads);
        data->size = (i == no_of_threads - 1) ? (file_size - data->start) : round_up_division(file_size, no_of_threads);
        data->thread_id = i+1;

        pthread_create(&threads[i], NULL, send_chunk, data);
        printf("[SERVER] Thread %d created\n", data->thread_id);
    }

    printf("[SERVER] Job done?\n");
    for (int i = 0; i < no_of_threads; i++)
    {
        pthread_join(threads[i], NULL);
    }

    // free(threads);
    close(client_socket);
    printf("[SERVER] Client handled and socket closed\n");
    return NULL;
}

int main()
{
    // int server_fd;
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

    if (listen(server_fd, 100) < 0)
    {
        perror("listen");
        exit(EXIT_FAILURE);
    }

    printf("[SERVER] Running and waiting for connections\n");

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
        printf("[SERVER] Connection accepted\n");

        pthread_t client_thread;
        // pthread_create(&client_thread, NULL, handle_client, socket_ptr);
        pthread_create(&client_thread, NULL, handle_client, socket_ptr);
        // pthread_detach(client_thread); // no need to join threads after this point
        printf("[SERVER] Client thread created\n");
        pthread_join(client_thread, NULL);
    }

    // TODO: send file checksum before closing socket
    close(server_fd);
    printf("**************************[SERVER] Server closed**************************\n");
    return 0;
}

// Borrowed from Geeksforgeeks
// https://www.geeksforgeeks.org/socket-programming-cc/
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

struct chunk_data
{
    int client_socket; // needed for TCP connection
    int start;
    int size;
    char *file_name;
};

char *sha_256_checksum(char *file_name)
{
    char command[256];
    snprintf(command, sizeof(command), "sha256sum %s", file_name);
    FILE *file = popen(command, "r");
    char *buffer = (char *)(malloc(65));
    if (file == NULL)
    {
        perror("Error opening file");
        return NULL;
    }
    if (fscanf(file, "%64s", buffer) == 1)
    {
        printf("[CLIENT] Checksum: %s\n", buffer);
    }
    pclose(file);
    return buffer;
}
// Inspired by https://medium.com/@sandipbhuyan/how-to-calculate-sha256-in-c-using-system-call-and-pipeline-4fd7e0064c22

// function which receives chunks from server
void *receive_chunks(void *args)
{
    int client_fd = *(int *)args;
    client_fd += 0;

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

    // Convert IPv4 and IPv6 addresses from text to binary
    // form
    if (inet_pton(AF_INET, "127.0.0.1", &serv_addr.sin_addr) <= 0)
    {
        printf(
            "\nInvalid address/ Address not supported \n");
        return NULL;
    }

    // Connecting thread socket to server
    sleep(1);
    if (connect(thread_socket, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)
    {
        perror("Client: Connection failed");
        close(thread_socket);
        return NULL;
    }

    int thread_no = 0;
    // receive thread number
    int valread = read(thread_socket, &thread_no, sizeof(int));

    // receive metadata
    int header[2];
    valread = read(thread_socket, header, sizeof(header));
    if (valread != sizeof(header))
    {
        close(thread_socket);
        return NULL;
    }

    // receive actual chunk
    unsigned char *buffer = (unsigned char *)(malloc(header[1] == 65536 ? 65536 : header[1]));

    // write data to file
    char temp_file_name[256];
    // pthread_mutex_lock(&file_mutex);
    snprintf(temp_file_name, 256, "received_files/temp%d.txt", thread_no); // Construct the file path
    FILE *file = fopen(temp_file_name, "wb+");
    if (file == NULL)
    {
        printf("[CLIENT] Error opening file %s\n", temp_file_name);
        perror("[CLIENT] Error opening file");
        free(buffer);
        return NULL;
    }

    if (header[1] > 65536)
    {
        pthread_mutex_lock(&file_mutex);
        int i = 0;
        char* flag = "0";
        int chunk_size = 0;
        // while(flag == 0)
        // while(strcmp(flag, "1") != 0)
        while(i*65536 < header[1])
        {
            valread = read(thread_socket, &chunk_size, sizeof(int));
            if (valread < 0)
            {
                free(buffer);
                close(thread_socket);
                return NULL;
            }
            valread = read(thread_socket, buffer, chunk_size);
            if (valread < 0)
            {
                free(buffer);
                close(thread_socket);
                return NULL;
            }
            fwrite(buffer, 1, valread, file);
            valread = read(thread_socket, flag, sizeof(char));
            i++;
        }
    }
    else
    {
        valread = read(thread_socket, buffer, header[1]);
        if (valread < 0)
        {
            free(buffer);
            close(thread_socket);
            return NULL;
        }
        fwrite(buffer, 1, valread, file);
    }

    fclose(file);
    pthread_mutex_unlock(&file_mutex);
    close(thread_socket);

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
    char file_name[256];
    strcpy(file_name, argv[1]);

    int no_of_threads = atoi(argv[2]);

    // need to communicate these to the server initially
    int status, valread, client_fd;
    struct sockaddr_in serv_addr;
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

    // sending arguments to background server
    send(client_fd, file_name, sizeof(file_name), 0);
    // Send number of threads
    if (send(client_fd, &no_of_threads, sizeof(int), 0) <= 0)
    {
        perror("Client: Error sending number of threads");
        close(client_fd);
        return -1;
    }
    printf("[CLIENT] File transfer request sent!\n");

    // receiving response from server
    int file_size;
    valread = read(client_fd, &file_size, sizeof(int));
    valread += 0; // to avoid unused variable warning

    // create threads
    pthread_t *threads = (pthread_t *)(malloc(no_of_threads * sizeof(pthread_t)));
    for (int i = 0; i < no_of_threads; i++)
    {
        struct chunk_data *data = (struct chunk_data *)(malloc(sizeof(struct chunk_data)));
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
        pthread_create(&threads[i], NULL, receive_chunks, &client_fd);
    }

    // join threads
    for (int i = 0; i < no_of_threads; i++)
    {
        pthread_join(threads[i], NULL);
    }

    // put all temp files together into one file
    char *rec_file_name = (char *)(malloc(256));
    int _ = snprintf(rec_file_name, 256, "received_files/%s", file_name); // Construct the file path
    _ += 0; // to avoid unused variable warning

    FILE *rec_file = fopen(rec_file_name, "wb");
    if (rec_file == NULL)
    {
        perror("[CLIENT] Error creating file");
        return -1;
    }

    for (int i = 1; i <= no_of_threads; i++)
    {
        char temp_filename[256];
        snprintf(temp_filename, sizeof(temp_filename), "received_files/temp%d.txt", i);
        FILE *temp_file = fopen(temp_filename, "rb");
        if (temp_file == NULL)
        {
            perror("[CLIENT] Error opening temp file");
            fclose(rec_file);
            return -1;
        }

        fseek(temp_file, 0, SEEK_END);
        long temp_file_size = ftell(temp_file);
        rewind(temp_file);

        unsigned char *buffer = (unsigned char *)malloc(temp_file_size);
        if (buffer == NULL)
        {
            perror("[CLIENT] Error allocating memory");
            fclose(temp_file);
            fclose(rec_file);
            return -1;
        }

        fread(buffer, 1, temp_file_size, temp_file);
        fwrite(buffer, 1, temp_file_size, rec_file);

        free(buffer);
        fclose(temp_file);
    }

    fclose(rec_file);

    // cleanup
    system("rm received_files/temp*.txt");

    // receiving checksum
    char* rec_checksum = (char*)(malloc(64));
    valread = read(client_fd, rec_checksum, 64);
    if (valread < 0)
    {
        printf("[CLIENT] Error receiving checksum\n");
        close(client_fd);
        return -1;
    }
    printf("[CLIENT] Checksum received: %s\n", rec_checksum);
    
    // calculating checksum
    char *checksum = sha_256_checksum(rec_file_name);

    // closing the connected socket
    close(client_fd);
    printf("[CLIENT] Connection closed\n\n\n");

    if (strcmp(checksum, rec_checksum) == 0)
    {
        printf("[CLIENT] Checksums match\n");
    }
    else
    {
        printf("[CLIENT] Checksums do not match\n");
    }

    // free data here
    free(threads);
    free(rec_file_name);

    return 0;
}

// Borrowed from Geeksforgeeks
// https://www.geeksforgeeks.org/socket-programming-cc/
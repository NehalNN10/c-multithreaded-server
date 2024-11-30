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
struct chunk_data
{
    int client_socket; // needed for TCP connection
    int start;
    int size;
    char *file_name;
};

char* sha_256_checksum(char* file_name)
{
    FILE* file = popen("sha256sum file_name", "r");
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

// function which receives chunks from server
void *receive_chunks(void *args)
{
    // pthread_mutex_lock(&file_mutex);
    int client_fd = *(int *)args;

    printf("[CLIENT] Creating new socket for thread %d\n", client_fd);
    int thread_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (thread_socket < 0)
    {
        perror("Client: Socket creation failed");
        return NULL;
    }
    printf("[CLIENT] Thread %d: Socket created\n", client_fd);

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
    printf("[CLIENT] Attempting to connect thread socket %d to server\n", client_fd);
    sleep(1);
    if (connect(thread_socket, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)
    {
        perror("Client: Connection failed");
        close(thread_socket);
        return NULL;
    }

    printf("[CLIENT] Thread %d: Connected to server\n", client_fd);

    int thread_no = 0;
    // receive thread number
    int valread = read(thread_socket, &thread_no, sizeof(int));
    printf("[CLIENT] Thread %d: Received thread number %d\n", client_fd, thread_no);

    // receive metadata
    int header[2];
    valread = read(thread_socket, header, sizeof(header));
    if (valread != sizeof(header))
    {
        printf("[CLIENT] Error reading chunk metadata\n");
        close(thread_socket);
        return NULL;
    }
    printf("[CLIENT] Thread %d: Received chunk metadata: start=%d, size=%d\n", thread_no, header[0], header[1]);

    // receive actual chunk
    unsigned char *buffer = (unsigned char *)(malloc(header[1] == 65536 ? 65536 : header[1]));

    // write data to file
    char temp_file_name[256];
    // pthread_mutex_lock(&file_mutex);
    snprintf(temp_file_name, 256, "received_files/temp%d.txt", thread_no); // Construct the file path
    printf("[CLIENT] Thread %d: Writing data to file %s\n", thread_no, temp_file_name);
    FILE *file = fopen(temp_file_name, "wb+");
    if (file == NULL)
    {
        printf("[CLIENT] Error opening file %s\n", temp_file_name);
        perror("[CLIENT] Error opening file");
        free(buffer);
        return NULL;
    }
    // pthread_mutex_unlock(&file_mutex);
    printf("[CLIENT] Thread %d: File opened\n", thread_no);
    // printf("[CLIENT] Thread %d: Data written to file\n", thread_no);

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
                printf("[CLIENT] Error reading chunk size for thread %d\n", thread_no);
                free(buffer);
                close(thread_socket);
                return NULL;
            }
            printf("[CLIENT] Thread %d: Received chunk size: %d\n", thread_no, chunk_size);
            valread = read(thread_socket, buffer, chunk_size);
            if (valread < 0)
            {
                printf("[CLIENT] Error reading chunk data\n");
                free(buffer);
                close(thread_socket);
                return NULL;
            }
            printf("[CLIENT] Thread %d: Received chunk data from offset %d\n", thread_no, i*65536);
            // sleep(1);
            fwrite(buffer, 1, valread, file);
            printf("[CLIENT] Thread %d: Data written to file %s\n", thread_no, temp_file_name);
            valread = read(thread_socket, flag, sizeof(char));
            printf("[CLIENT] Flag value: %s for thread %d\n", flag, thread_no);
            i++;
        }
        // pthread_mutex_unlock(&file_mutex);
    }
    else
    {
        valread = read(thread_socket, buffer, header[1]);
        if (valread < 0)
        {
            printf("[CLIENT] Error reading chunk data\n");
            free(buffer);
            close(thread_socket);
            return NULL;
        }
        printf("[CLIENT] Thread %d: Received chunk data\n", thread_no);
        fwrite(buffer, 1, valread, file);
        printf("[CLIENT] Thread %d: Data written to file %s\n", thread_no, temp_file_name);
    }

    fclose(file);
    pthread_mutex_unlock(&file_mutex);
    close(thread_socket);

    printf("[CLIENT] Thread %d: Socket closed\n", thread_no);

    // // write data to file
    // char temp_file_name[256];
    // // pthread_mutex_lock(&file_mutex);
    // snprintf(temp_file_name, 256, "received_files/temp%d.txt", thread_no); // Construct the file path
    // printf("[CLIENT] Thread %d: Writing data to file %s\n", thread_no, temp_file_name);
    // FILE *file = fopen(temp_file_name, "wb+");
    // if (file == NULL)
    // {
    //     printf("[CLIENT] Error opening file %s\n", temp_file_name);
    //     perror("[CLIENT] Error opening file");
    //     free(buffer);
    //     return NULL;
    // }
    // fwrite(buffer, 1, valread, file);
    // fclose(file);
    // pthread_mutex_unlock(&file_mutex);
    printf("[CLIENT] Thread %d: Data written to file\n", thread_no);

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
    // sleep(3);

    // receiving response from server
    int file_size;
    valread = read(client_fd, &file_size, sizeof(int));
    printf("[CLIENT] Size of file: %d\n", file_size);
    valread += 0; // to avoid unused variable warning
    // // printf("The size of the file is %d\n", file_size);
    // // printf("Trying to clear %s\n", rec_file_name);


    // FILE *temp = fopen(rec_file_name, "wb");
    // if (temp == NULL)
    // {
    //     printf("[CLIENT] Error opening file %s\n", rec_file_name);
    //     perror("[CLIENT] Error opening file");
    //     close(client_fd);
    //     return -1;
    // }
    // fclose(temp);

    // if (create_file_with_size(rec_file_name, file_size) == 0)
    // {
    //     printf("[CLIENT] File created successfully: %s\n", rec_file_name);
    // }
    // else
    // {
    //     printf("[CLIENT] Failed to create file\n");
    // }

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
        // pthread_create(&threads[i], NULL, receive_chunks, data);
        pthread_create(&threads[i], NULL, receive_chunks, &client_fd);
        // printf("Thread %d created\n", i);
    }

    // join threads
    printf("[CLIENT] Waiting for threads to finish\n");
    for (int i = 0; i < no_of_threads; i++)
    {
        pthread_join(threads[i], NULL);
    }

    // put all temp files together into one file
    char *rec_file_name = (char *)(malloc(256));
    int _ = snprintf(rec_file_name, 256, "received_files/%s", file_name); // Construct the file path
    _ += 0; // to avoid unused variable warning
    printf("[CLIENT] File path: %s\n", rec_file_name);

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
        printf("[CLIENT] Temp file %s size: %ld\n", temp_filename, temp_file_size);
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
        printf("[CLIENT] Temp file %s written to final output\n", temp_filename);
    }

    fclose(rec_file);
    printf("[CLIENT] Final output file created: %s\n", rec_file_name);
    free(rec_file_name);

    // cleanup
    system("rm received_files/temp*.txt");

    // // receiving checksum
    // // calculating checksum

    // closing the connected socket
    close(client_fd);
    printf("[CLIENT] Connection closed\n\n\n");

    // // free data here
    // free(threads);
    // free(rec_file_name);

    return 0;
}

// TODO: perform comparison to check if file is received correctly

// Borrowed from Geeksforgeeks
// https://www.geeksforgeeks.org/socket-programming-cc/
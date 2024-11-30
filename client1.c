// Client side C program to demonstrate Socket
// programming
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netinet/tcp.h> // Add this line to define SO_REUSEPORT
#include <netinet/ip.h>  // Add this line to define struct iphdr
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <openssl/md5.h>
#include <unistd.h>
#include <time.h>  
#include <pthread.h>
#define PORT 8080
#define _POSIX_C_SOURCE 199309L

int CheckSumCalc(char * filename){
    FILE *fp = fopen(filename,"rb");
    if(fp == NULL)
    {
        //handle error here
        return -1;
    }
    unsigned char checksum = 0;
    while (!feof(fp) && !ferror(fp)) {
        checksum ^= fgetc(fp);
    }
    fclose(fp);
    return checksum;
}

int requests(char* file_name, int no_of_sessions) {
    int status, valread, client_fd;
    struct sockaddr_in serv_addr;
    char buffer[1024] = { 0 };
    int checksum, sentchecksum, file_size;

    if ((client_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        printf("\n Socket creation error \n");
        return -1;
    }

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(PORT);

    // Convert IPv4 and IPv6 addresses from text to binary
    // form
    if (inet_pton(AF_INET, "127.0.0.1", &serv_addr.sin_addr) <= 0) {
        printf("\nInvalid address/ Address not supported \n");
        return -1;
    }

    if ((status = connect(client_fd, (struct sockaddr*)&serv_addr, sizeof(serv_addr))) < 0) {
        printf("\nConnection Failed \n");
        return -1;
    }
    // send(client_fd, hello, strlen(hello), 0);
    send(client_fd, file_name, strlen(file_name), 0);
    // printf("file name sent\n");
    sleep(0.5);
    send(client_fd, &no_of_sessions, sizeof(no_of_sessions), 0);
    // printf("no of sessions sent\n");
    
    //CONVERT THIS SO THAT YOU CAN OPEN ANY FILE IN ANY FOLDER
    char custom_file_name[1024];
    snprintf(custom_file_name, sizeof(custom_file_name), "req%s", file_name);
    FILE* file = fopen(custom_file_name, "w");
    if (file == NULL) {
        printf("Error opening file %s\n", custom_file_name);
        return -1;
    }

    memset(buffer, 0, sizeof(buffer));

    read(client_fd, &sentchecksum, sizeof(sentchecksum));

    read(client_fd, &file_size, sizeof(file_size));
    printf("file size: %d\n", file_size);

    if (setsockopt(client_fd, SOL_SOCKET, SO_RCVBUF, &file_size, sizeof(file_size)) < 0) {
            perror("setsockopt");
            exit(EXIT_FAILURE);
            
        }

    char buffer1[file_size];

    valread = read(client_fd, buffer1, file_size); 
    printf("valread: %d\n", valread);
    fwrite(buffer1, 1, valread, file);

    // fprintf(file, "%s", custom_file_name);
    printf("%s\n", custom_file_name);
    // printf("buffer: %s\n", buffer);


    // while (valread != 0) {
    //     printf("valread: %d\n", valread);
    //     fwrite(buffer, 1, valread, file);
    //     // printf("%s\n", custom_file_name);
    //     valread = read(client_fd, buffer, 1024 - 1);
    //     // memset(buffer, 0, sizeof(buffer));
    // }

    fclose(file);
    printf("Recieved checksum: %d\n", sentchecksum);

    checksum = CheckSumCalc(custom_file_name);
    printf("Checksum: %d\n", checksum);

    close(client_fd);

    if (checksum == sentchecksum) {
        printf("file received correctly\n");
    } else {
        printf("corrupted file received\n");
    }

    
    return 0;
}


int main(int argc, char const* argv[])
{
    if (argc != 3){
        printf("Usage: %s  <file_name>  <no of sessions needed>\n", argv[0]);
        return 1;
    }
    char* file_name = argv[1];
    int no_of_sessions = atoi(argv[2]);

    for (int i = 0; i < 1; i++)
        requests(file_name, no_of_sessions);

    

    return 0;
}
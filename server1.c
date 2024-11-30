// Server side C program to demonstrate Socket
// programming
#include <netinet/in.h>
#include <netinet/tcp.h> // Add this line to define SO_REUSEPORT
#include <netinet/ip.h>  // Add this line to define struct iphdr
#include <arpa/inet.h>
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
        return -1;
    unsigned char checksum = 0;
    while (!feof(fp) && !ferror(fp))
        checksum ^= fgetc(fp);
    
    fclose(fp);
    return checksum;
}

int main(int argc, char const* argv[])
{
    int server_fd, new_socket;
    ssize_t valread;
    struct sockaddr_in address;
    int opt = 1;
    socklen_t addrlen = sizeof(address);
    char buffer[1024] = { 0 };
    char* buff;
    


    // Creating socket file descriptor
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("socket failed");
        exit(EXIT_FAILURE);
    }

    // Forcefully attaching socket to the port 8080
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt))) {
        perror("setsockopt");
        exit(EXIT_FAILURE);
    }
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);

    // Forcefully attaching socket to the port 8080
    if (bind(server_fd, (struct sockaddr*)&address, sizeof(address)) < 0) {
        perror("bind failed");
        exit(EXIT_FAILURE);
    }

    while(1){
        if (listen(server_fd, 10) < 0) {
            perror("listen");
            exit(EXIT_FAILURE);
        }
        if ((new_socket = accept(server_fd, (struct sockaddr*)&address, &addrlen)) < 0) {
            perror("accept");
            exit(EXIT_FAILURE);
        }

        memset(buffer, 0, sizeof(buffer));
        int checksum;

        valread = read(new_socket, buffer, 1024 - 1); // subtract 1 for the null terminator at the end
        if (valread > 0) {
            printf("requested file: %s\n", buffer);
            checksum = CheckSumCalc(buffer);
            printf("Checksum: %d\n", checksum);
        } else {
            perror("read");
            continue;
        }

        send(new_socket, &checksum, sizeof(checksum), 0);
        sleep(0.5);

        FILE* file = fopen(buffer, "r");
        if (file == NULL) {
            perror("fopen");
            continue;
        }

        fseek(file, 0, SEEK_END);
        int file_size = ftell(file);    
        fseek(file, 0, SEEK_SET);

        send(new_socket, &file_size, sizeof(file_size), 0);
        printf("file size: %d\n", file_size);

        if (setsockopt(server_fd, SOL_SOCKET, SO_SNDBUF, &file_size, sizeof(file_size)) < 0) {
            perror("setsockopt");
            exit(EXIT_FAILURE);
            
        }

        buff = (char*)malloc(file_size);
        int no_of_sessions;
        valread = read(new_socket, &no_of_sessions, sizeof(no_of_sessions));
        if (valread > 0) {
            printf("sessions: %d\n", no_of_sessions);
        } else {
            perror("read");
        }

        memset(buff, 0, file_size);
        // pthread_t t1;
        // pthread_create(&t1, NULL, func1, NULL);
        // pthread_join(t1, NULL);
        // while ((valread = fread(buffer, 1, sizeof(buffer), file)) > 0) {
        //     send(new_socket, buffer, valread, 0);
        // }

        valread = fread(buff, 1, file_size, file);
        printf("valread: %d\n", valread);
        send(new_socket, buff, valread, 0);

        fclose(file);

        // send(new_socket, file, strlen(file), 0);
        printf("file sent\n");
        close(new_socket);
    }

        close(server_fd);
    return 0;
}

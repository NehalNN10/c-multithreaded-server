# Operating Systems Homework 4
### Nehal Naeem Haji, nh07884

## Overview

The deliverable of this assignment was a multi-threaded server in C, which takes the number of threads and the filename from the client (via the terminal) and sends a request to the server, through a TCP socket.

## Principle of Action

1. **Client Initialization**:
   - The client program is started with the filename and the number of threads as command-line arguments.
   - The client establishes a connection to the server using a TCP socket.

2. **Request Transmission**:
   - The client sends the filename and the number of threads to the server.
   - The server receives the request and prepares to process the file.

3. **File Splitting**:
   - The server opens the requested file and calculates its size.
   - The server splits the file into chunks based on the number of threads specified by the client.

4. **Thread Creation on Server**:
   - The server creates multiple threads, each responsible for sending a chunk of the file to the client.
   - Each thread sends its assigned chunk to the client over a new socket connection.

5. **Thread Creation on Client**:
   - The client creates multiple threads, each responsible for receiving a chunk of the file from the server.
   - Each thread establishes a new socket connection to the server to receive its assigned chunk.

6. **Data Transfer**:
   - Each server thread sends its chunk of the file to the corresponding client thread.
   - Each client thread receives its chunk and writes it to a temporary file.

7. **File Assembly**:
   - After all chunks have been received, the client combines the temporary files into the final output file.
   - The client ensures that the chunks are combined in the correct order to reconstruct the original file.

8. **Completion**:
   - The client and server close their respective socket connections.
   - The client cleans up any temporary files created during the process.

## How to run
Simply type `make client` to compile the client, and `make server` to compile the server. In order to get rid of the compiled files, including all object files, type `make clean` into the terminal.

To run all test cases, type `./tests.sh` into the terminal. Note, you must give the script permission to execute by typing `chmod +x tests.sh` into the terminal.

## Assumptions

The program assumes the files to be tested are already in the folder named `test_files`. If not, place them there before running the test script.

## Limitations

- The server can only handle one client at a time.
- The maximum chunk size is 64KiB, meaning the server has to read the file in chunks of 64KiB.

## Future work

A database server, such as MySQL or Microsoft SSMS, can be a future extension of this work.

Additionally, this server can be implemented across different machines on the same network.
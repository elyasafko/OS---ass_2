#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <stdbool.h>
#include <signal.h>
#include <sys/wait.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>

// Global variables to hold socket file descriptors
int input_fd = STDIN_FILENO;
int output_fd = STDOUT_FILENO;

int child = 0;

void close_socket_if_open()
{
    if (input_fd != STDIN_FILENO)
    {
        close(input_fd);
    }

    if (output_fd != STDOUT_FILENO)
    {
        close(output_fd);
    }
}

void close_resources_and_exit(int)
{
    close_socket_if_open();

    if (child) {
        printf("going to kill %d\n", child);
        kill(child, SIGKILL);
    }

    exit(EXIT_SUCCESS);
}

void run_command(const char *exec_command)
{
    printf("Running command: %s\n", exec_command);
    // Fork a new process to run the command.
    // If the fork() function returns 0, this is the child process.
    if ((child = fork()) == 0)
    {
        // Execute the command.
        // If exec_command is not NULL, execute the command using the shell.
        // If exec_command is NULL, copy stdin to stdout.
        if (exec_command)
        {
            execl("/bin/sh", "sh", "-c", exec_command, (char *)NULL);
            perror("execl failed");                 // Print an error message if execl fails.
            close_resources_and_exit(EXIT_FAILURE); // Exit with a failure status.
        }
        else
        {
            printf("Copying stdin to stdout\n");
            // If no command is given, copy stdin to stdout.
            // Read data from stdin, write it to stdout, and continue until there is no more data.
            while (1)
            {
                char buffer[1024]; // Buffer to hold the data read from stdin.
                int n = read(STDIN_FILENO, buffer, sizeof(buffer));
                if (n <= 0)
                    break;                       // If there is no more data, exit the loop.
                write(STDOUT_FILENO, buffer, n); // Write the data to stdout.
            }
            close_resources_and_exit(EXIT_SUCCESS); // Exit with a success status.
        }
    }

    // Wait for the child process to finish.
    // The parent process waits for the child process to exit.
    wait(NULL);
}

/**
 * start_tcp_server: Creates a TCP server socket, sets socket options, binds the socket to a port,
 *                  and starts listening for incoming client connections.
 * @param port: The port number to listen on.
 */
int start_tcp_server(int port)
{
    // Create a server socket.
    int server_fd = socket(AF_INET, SOCK_STREAM, 0); // AF_INET specifies IPv4, SOCK_STREAM specifies TCP.
    if (server_fd == -1)                             // Check if the socket creation was successful.
    {
        perror("Failed to create server socket"); // Print an error message if the socket creation failed.
        exit(EXIT_FAILURE);                       // Exit with a failure status.
    }
    printf("Server socket created\n"); // Print a message indicating that the server socket was created.

    // Set the socket options to reuse the address.
    int opt = 1;
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) == -1)
    {
        perror("Failed to set socket options"); // Print an error message if setting the socket options failed.
        close(server_fd);                       // Close the server socket.
        exit(EXIT_FAILURE);                     // Exit with a failure status.
    }
    printf("Server socket options set\n"); // Print a message indicating that the server socket options were set.

    // Set up the server address structure.
    struct sockaddr_in serverAddress;
    serverAddress.sin_family = AF_INET;         // Set the address family to AF_INET (IPv4).
    serverAddress.sin_addr.s_addr = INADDR_ANY; // Set the server address to INADDR_ANY (listen on all available network interfaces).
    serverAddress.sin_port = htons(port);       // Set the server port.

    // Bind the server socket to the server address.
    if (bind(server_fd, (struct sockaddr *)&serverAddress, sizeof(serverAddress)) == -1)
    {
        perror("Failed to bind server socket"); // Print an error message if binding the server socket failed.
        close(server_fd);                       // Close the server socket.
        exit(EXIT_FAILURE);                     // Exit with a failure status.
    }
    printf("Server socket bound\n"); // Print a message indicating that the server socket was bound.

    // Start listening for incoming client connections.
    if (listen(server_fd, 1) == -1)
    {
        perror("Failed to listen on server socket"); // Print an error message if listening on the server socket failed.
        close(server_fd);                            // Close the server socket.
        exit(EXIT_FAILURE);                          // Exit with a failure status.
    }
    printf("Server socket listening\n"); // Print a message indicating that the server socket is listening.

    // Accept an incoming client connection.
    struct sockaddr_in clientAddress;
    socklen_t clientAddressLength = sizeof(clientAddress);
    int clientSocket = accept(server_fd, (struct sockaddr *)&clientAddress, &clientAddressLength);
    if (clientSocket == -1)
    {
        perror("Failed to accept client connection"); // Print an error message if accepting the client connection failed.
        close(server_fd);                             // Close the server socket.
        exit(EXIT_FAILURE);                           // Exit with a failure status.
    }
    printf("Client connection accepted\n"); // Print a message indicating that the client connection was accepted.

    return clientSocket;
}

/**
 * start_tcp_client: Creates a TCP client socket and connects to a server.
 * @param hostname: The hostname or IP address of the server.
 * @param port: The port number of the server.
 */
int start_tcp_client(const char *hostname, int port)
{
    int client_fd;
    // Create a TCP client socket.
    if ((client_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    {
        perror("Socket creation error"); // Print an error message if socket creation fails.
        exit(EXIT_FAILURE);              // Exit with a failure status.
    }
    printf("Socket created\n"); // Print a message indicating that the socket was created.

    // Set up the server address structure.
    struct sockaddr_in serv_addr;
    serv_addr.sin_family = AF_INET;   // Set the address family to AF_INET (IPv4).
    serv_addr.sin_port = htons(port); // Set the server port.

    // Check if the hostname is NULL or "localhost".
    if (hostname == NULL || (strcmp(hostname, "localhost") == 0))
    {
        serv_addr.sin_addr.s_addr = inet_addr("127.0.0.1"); // Set the server address to "127.0.0.1" (localhost).
        printf("Server address set to localhost\n");
    }
    else
    {
        // Check if the hostname is a valid IP address.
        if (inet_pton(AF_INET, hostname, &serv_addr.sin_addr) <= 0)
        {
            perror("Invalid address/ Address not supported"); // Print an error message if the hostname is invalid.
            close(client_fd);                                 // Close the client socket.
            exit(EXIT_FAILURE);                               // Exit with a failure status.
        }
        printf("Server address set to %s\n", hostname);
    }

    // Print a message indicating that the client is connecting to the server.
    printf("Connecting to %s:%d\n", hostname ?: "localhost", port);

    // Connect the client socket to the server.
    if (connect(client_fd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)
    {
        perror("Connection failed"); // Print an error message if the connection fails.
        close(client_fd);            // Close the client socket.
        exit(EXIT_FAILURE);          // Exit with a failure status.
    }
    printf("Connected to %s:%d\n", hostname ?: "localhost", port);

    return client_fd;
}

int start_udp_server(int port)
{
    int server_fd;
    if ((server_fd = socket(AF_INET, SOCK_DGRAM, 0)) == 0)
    {
        perror("socket failed");
        exit(EXIT_FAILURE);
    }
    printf("UDP server socket created\n");

    // Set the socket options to reuse the address
    int opt = 1;
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)))
    {
        perror("setsockopt");
        close(server_fd);
        exit(EXIT_FAILURE);
    }
    printf("UDP server socket options set\n");

    struct sockaddr_in address;
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(port);

    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0)
    {
        perror("bind failed");
        close(server_fd);
        exit(EXIT_FAILURE);
    }
    printf("UDP server socket bound to port %d\n", port);

    // char buffer[1024];
    // struct sockaddr_in client_addr;
    // socklen_t client_addr_len = sizeof(client_addr);

    // int bytes_received = recvfrom(server_fd, buffer, sizeof(buffer), 0, (struct sockaddr *)&client_addr, &client_addr_len);
    // if (bytes_received == -1)
    //{
    //     perror("error receiving data");
    //     close_resources_and_exit(EXIT_FAILURE);
    // }
    // printf("Received data from client: %s\n", buffer);

    // Call connect to save the client address
    // if (connect(server_fd, (struct sockaddr *)&client_addr, client_addr_len) == -1)
    //{
    //    perror("error connecting to client");
    //    close_resources_and_exit(EXIT_FAILURE);
    //}
    // printf("Connected to client\n");
    return server_fd;
}

int start_udp_client(const char *hostname, int port)
{
    struct sockaddr_in serv_addr;
    char buffer[1024] = "Hello from UDP client";
    int client_fd;
    if ((client_fd = socket(AF_INET, SOCK_DGRAM, 0)) < 0)
    {
        perror("Socket creation error");
        exit(EXIT_FAILURE);
    }

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(port);

    if (hostname == NULL || strcmp(hostname, "localhost") == 0)
    {
        serv_addr.sin_addr.s_addr = inet_addr("127.0.0.1");
    }
    else
    {
        if (inet_pton(AF_INET, hostname, &serv_addr.sin_addr) <= 0)
        {
            perror("Invalid address/ Address not supported");
            close(client_fd);
            exit(EXIT_FAILURE);
        }
    }

    sendto(client_fd, buffer, strlen(buffer), MSG_CONFIRM, (const struct sockaddr *)&serv_addr, sizeof(serv_addr));
    connect(client_fd, (const struct sockaddr *)&serv_addr, sizeof(serv_addr));
    printf("Message sent\n");

    return client_fd;
}

int start_uds_server_datagram(char *path)
{
    printf("Starting UDS server\n");
    // create a socket
    int sockfd = socket(AF_UNIX, SOCK_DGRAM, 0);
    if (sockfd == -1)
    {
        perror("error creating socket");
        close_resources_and_exit(EXIT_FAILURE);
    }
    printf("Socket created\n");
    // bind the socket to the address
    struct sockaddr_un addr;
    addr.sun_family = AF_UNIX;
    strcpy(addr.sun_path, path);
    unlink(addr.sun_path);
    if (bind(sockfd, (struct sockaddr *)&addr, sizeof(addr)) == -1)
    {
        printf("%s\n", addr.sun_path);
        perror("error binding socket");
        close_resources_and_exit(EXIT_FAILURE);
    }
    printf("Socket bound\n");
    // receive dummy data to get the client address
    char buffer[1024];
    struct sockaddr_un client_addr;
    socklen_t client_addr_len = sizeof(client_addr);

    int bytes_received = recvfrom(sockfd, buffer, sizeof(buffer), 0, (struct sockaddr *)&client_addr, &client_addr_len);
    if (bytes_received == -1)
    {
        perror("error receiving data");
        close_resources_and_exit(EXIT_FAILURE);
    }
    printf("Received data from client: %s\n", buffer);
    return sockfd;
}

int start_uds_server_stream(char *path)
{
    printf("Starting UDS server\n");
    int sockfd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (sockfd == -1)
    {
        perror("error creating socket");
        close_resources_and_exit(EXIT_FAILURE);
    }
    printf("Socket created\n");
    struct sockaddr_un addr;
    addr.sun_family = AF_UNIX;
    strcpy(addr.sun_path, path);
    unlink(addr.sun_path);
    if (bind(sockfd, (struct sockaddr *)&addr, sizeof(addr)) == -1)
    {
        perror("error binding socket");
        close_resources_and_exit(EXIT_FAILURE);
    }
    printf("Socket bound\n");
    if (listen(sockfd, 5) == -1)
    {
        perror("error listening");
        close_resources_and_exit(EXIT_FAILURE);
    }
    printf("Listening for connections\n");
    struct sockaddr_un client_addr;
    socklen_t client_addr_len = sizeof(client_addr);
    int client_fd = accept(sockfd, (struct sockaddr *)&client_addr, &client_addr_len);
    if (client_fd == -1)
    {
        perror("error accepting connection");
        close_resources_and_exit(EXIT_FAILURE);
    }
    printf("Connection accepted\n");
    return client_fd;
}

int start_uds_client_datagram(char *path)
{
    printf("Starting UDS client\n");
    int sockfd = socket(AF_UNIX, SOCK_DGRAM, 0);
    if (sockfd == -1)
    {
        perror("error creating socket");
        close_resources_and_exit(EXIT_FAILURE);
    }

    struct sockaddr_un addr;
    addr.sun_family = AF_UNIX;
    strcpy(addr.sun_path, path);
    printf("Connecting to server\n");
    if (connect(sockfd, (struct sockaddr *)&addr, sizeof(addr)) == -1)
    {
        perror("error connecting to server");
        close_resources_and_exit(EXIT_FAILURE);
    }

    printf("Connected to server\n");

    return sockfd;
}

int start_uds_client_stream(char *path)
{
    printf("Starting UDS client\n");
    int sockfd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (sockfd == -1)
    {
        perror("error creating socket");
        close_resources_and_exit(EXIT_FAILURE);
    }

    struct sockaddr_un addr;
    addr.sun_family = AF_UNIX;
    strcpy(addr.sun_path, path);
    printf("Connecting to server\n");
    if (connect(sockfd, (struct sockaddr *)&addr, sizeof(addr)) == -1)
    {
        perror("error connecting to server");
        close_resources_and_exit(EXIT_FAILURE);
    }
    printf("Connected to server\n");
    return sockfd;
}

/**
 * Update the input and output file descriptors
 * @param value the value to update the file descriptors
 * @param input_need_change 1 if the input file descriptor needs to be changed, 0 otherwise
 * @param output_need_change 1 if the output file descriptor needs to be changed, 0 otherwise
 */
void configureInputOutput(bool tcp, bool udp, bool uds, bool server, bool client, int port, char *hostname, char *path, int change_in, int change_out)
{
    printf("configureInputOutput called with tcp: %d, udp: %d, uds: %d, server: %d, client: %d, port: %d, hostname: %s, path: %s, change_in: %d, change_out: %d\n", tcp, udp, uds, server, client, port, hostname, path, change_in, change_out);
    int new_fd;
    if (!uds && tcp && server)
    {
        new_fd = start_tcp_server(port);
    }
    else if (!uds && tcp && client)
    {
        new_fd = start_tcp_client(hostname, port);
    }
    else if (!uds && udp && server)
    {
        new_fd = start_udp_server(port);
    }
    else if (!uds && udp && client)
    {
        new_fd = start_udp_client(hostname, port);
    }
    else if (uds && tcp && server)
    {
        new_fd = start_uds_server_datagram(path);
    }
    else if (uds && tcp && client)
    {
        new_fd = start_uds_client_datagram(path);
    }
    else if (uds && udp && server)
    {
        new_fd = start_uds_server_stream(path);
    }
    else if (uds && udp && client)
    {
        new_fd = start_uds_client_stream(path);
    }
    else
    {
        fprintf(stderr, "Invalid input");
        close_resources_and_exit(EXIT_FAILURE);
    }

    if (change_in)
    {
        printf("Changing input_fd from %d to %d\n", input_fd, new_fd);
        input_fd = new_fd;
    }
    if (change_out)
    {
        printf("Changing output_fd from %d to %d\n", output_fd, new_fd);
        output_fd = new_fd;
    }
}

char *extract_path(const char *arg)
{
    // Find the "-i " option
    const char *start = strstr(arg, "UDS");
    if (start == NULL || (strlen(start) <= 5))
    {
        return NULL;
    }
    // Move past the "-i " option to the actual path
    start += 5;

    // Allocate memory for the path (assuming the path will not exceed 256 characters)
    static char path[256];
    int i = 0;

    // Extract the path until you find a space or end of string
    while (*start != ' ' && *start != '\0' && i < 255)
    {
        path[i++] = *start++;
    }
    path[i] = '\0'; // Null-terminate the string
    return path;
}

int main(int argc, char *argv[])
{
    if (argc < 2)
        return EXIT_FAILURE;

    char *command = NULL;
    char *server = NULL;
    char *client = NULL;
    static char filepath[256];
    char flag_server = 0;
    char flag_client = 0;
    bool e_flag = false;
    bool t_flag = false;
    unsigned int time = 0;
    bool udsss = false;
    bool udscs = false;
    bool udssd = false;
    bool udscd = false;

    for (int i = 1; i < argc; i++)
    {
        if (strcmp(argv[i], "-e") == 0)
        {
            e_flag = true;
            if (i + 1 < argc)
            {
                command = argv[++i];
                printf("Command: %s\n", command);
            }
            else
                return EXIT_FAILURE;
        }
        else if (strcmp(argv[i], "-t") == 0)
        {
            t_flag = true;
            if (i + 1 < argc)
            {
                time = atoi(argv[++i]);
                if (time == 0) {
                    printf("Error: timeout param error\n");
                    return EXIT_FAILURE;
                }

                printf("Time: %d\n", time);
            }
            else
                return EXIT_FAILURE;
        }
        else if (strcmp(argv[i], "-i") == 0 || strcmp(argv[i], "-o") == 0 || strcmp(argv[i], "-b") == 0)
        {
            printf("Flag: %c\n", argv[i][1]);
            char flag = argv[i][1];
            if (i + 1 < argc)
            {
                if (strncmp(argv[i + 1], "TCPS", 4) == 0)
                {
                    printf("Argument: %s\n", argv[i + 1]);
                    server = argv[++i];
                    flag_server = flag;
                    printf("Flag server: %c\n", flag_server);
                }
                else if (strncmp(argv[i + 1], "TCPC", 4) == 0)
                {
                    printf("Argument: %s\n", argv[i + 1]);
                    client = argv[++i];
                    flag_client = flag;
                    printf("Flag client: %c\n", flag_client);
                }
                else if (strncmp(argv[i + 1], "UDPS", 4) == 0)
                {
                    printf("Argument: %s\n", argv[i + 1]);
                    server = argv[++i];
                    flag_server = flag;
                    printf("Flag server: %c\n", flag_server);
                }
                else if (strncmp(argv[i + 1], "UDPC", 4) == 0)
                {
                    printf("Argument: %s\n", argv[i + 1]);
                    client = argv[++i];
                    flag_client = flag;
                    printf("Flag client: %c\n", flag_client);
                }
                else if (strncmp(argv[i + 1], "UDS", 3) == 0)
                {
                    // Extract path from UDS argument
                    char *file_location = extract_path(argv[i + 1]);
                    if (file_location != NULL && (strlen(file_location) > 0))
                    {
                        strncpy(filepath, file_location, sizeof(filepath) - 1); // Copy extracted path to filepath
                        filepath[sizeof(filepath) - 1] = '\0';                  // Ensure null-termination
                        printf("Extracted path: %s\n", filepath);
                    }
                    else
                    {
                        printf("Invalid UDS argument\n");
                        return EXIT_FAILURE;
                    }                 

                    if (strncmp(argv[i + 1], "UDSSS", 5) == 0)
                    {
                        printf("UDS server using stream\n");
                        printf("file_location Path: %s\n", file_location);
                        flag_server = flag;
                        printf("Flag server: %c\n", flag_server);
                        udsss = true;
                    }
                    if (strncmp(argv[i + 1], "UDSSD", 5) == 0)
                    {
                        printf("UDS server using datagram\n");
                        printf("file_location Path: %s\n", file_location);
                        flag_server = flag;
                        printf("Flag server: %c\n", flag_server);
                        udssd = true;
                    }
                    if (strncmp(argv[i + 1], "UDSCD", 5) == 0)
                    {

                        printf("UDS client using datagram\n");
                        printf("file_location Path: %s\n", file_location);
                        flag_client = flag;
                        printf("Flag client: %c\n", flag_client);
                        udscd = true;
                    }
                    if (strncmp(argv[i + 1], "UDSCS", 5) == 0)
                    {
                        printf("UDS client using stream\n");
                        printf("file_location Path: %s\n", file_location);
                        flag_client = flag;
                        printf("Flag client: %c\n", flag_client);
                        udscs = true;
                    }
                }
                else
                {
                    fprintf(stderr, "Invalid TCP/UDP argument\n");
                    return EXIT_FAILURE;
                }
            }
            else
            {
                fprintf(stderr, "Expected argument after %s\n", argv[i]);
                return EXIT_FAILURE;
            }
        }
    }

    printf("Server: %s\n", server ?: "localhost");

    if (t_flag)
    {
        signal(SIGALRM, close_resources_and_exit);
        alarm(time);
    }

    if (server == NULL && client == NULL && e_flag == false && !(udssd || udsss || udscs || udscd))
    {
        run_command(command);
    }
    if (server && strncmp(server, "TCPS", 4) == 0)
    {
        int port = atoi(server + 4);
        printf("Port for TCP server: %d\n", port);

        if (flag_server == 'i')
            configureInputOutput(true, false, false, true, false, port, NULL, NULL, 1, 0);
        else if (flag_server == 'o')
            configureInputOutput(true, false, false, true, false, port, NULL, NULL, 0, 1);
        else if (flag_server == 'b')
            configureInputOutput(true, false, false, true, false, port, NULL, NULL, 1, 1);
        else
        {
            fprintf(stderr, "Invalid flag\n");
            return EXIT_FAILURE;
        }
    }
    if (client && strncmp(client, "TCPC", 4) == 0)
    {
        char *hostname = client + 4;
        char *port_str = strchr(hostname, ',');
        if (port_str == NULL)
        {
            // TCPS1234
            port_str = hostname; // *port_str=1234
            hostname = NULL;     // hostname=NULL
        }
        else
        {
            // TCPSmyserver,1234
            *port_str = '\0'; // *hostname="myserver\0"
            port_str++;       // *port_str=1234
        }
        int port = atoi(port_str);
        printf("Port for TCP client: %d\n", port);
        if (flag_client == 'i')
            configureInputOutput(true, false, false, false, true, port, hostname, NULL, 1, 0);
        else if (flag_client == 'o')
            configureInputOutput(true, false, false, false, true, port, hostname, NULL, 0, 1);
        else if (flag_client == 'b')
            configureInputOutput(true, false, false, false, true, port, hostname, NULL, 1, 1);
        else
        {
            fprintf(stderr, "Invalid flag\n");
            return EXIT_FAILURE;
        }
    }
    if (server && strncmp(server, "UDPS", 4) == 0)
    {
        int port = atoi(server + 4);
        printf("Port for UDP server: %d\n", port);
        if (flag_server == 'i')
            configureInputOutput(false, true, false, true, false, port, NULL, NULL, 1, 0);
        else if (flag_server == 'o')
            configureInputOutput(false, true, false, true, false, port, NULL, NULL, 0, 1);
        else if (flag_server == 'b')
            configureInputOutput(false, true, false, true, false, port, NULL, NULL, 1, 1);
        else
        {
            fprintf(stderr, "Invalid flag\n");
            return EXIT_FAILURE;
        }
    }
    if (client && strncmp(client, "UDPC", 4) == 0)
    {
        char *hostname = client + 4;
        char *port_str = strchr(hostname, ',');
        if (port_str == NULL)
        {
            // UDPC1234
            port_str = hostname; // *port_str=1234
            hostname = NULL;     // hostname=NULL
        }
        else
        {
            // UDPCmyserver,1234
            *port_str = '\0'; // *hostname="myserver\0"
            port_str++;       // *port_str=1234
        }
        int port = atoi(port_str);
        printf("Port for UDP client: %d\n", port);
        if (flag_client == 'i')
            configureInputOutput(false, true, false, false, true, port, hostname, NULL, 1, 0);
        else if (flag_client == 'o')
            configureInputOutput(false, true, false, false, true, port, hostname, NULL, 0, 1);
        else if (flag_client == 'b')
            configureInputOutput(false, true, false, false, true, port, hostname, NULL, 1, 1);
        else
        {
            fprintf(stderr, "Invalid flag\n");
            return EXIT_FAILURE;
        }
    }

    if (udssd)
    {
        if (flag_server == 'i')
            configureInputOutput(false, true, true, true, false, 0, NULL, filepath, 1, 0);
        else
        {
            fprintf(stderr, "Invalid flag\n");
            return EXIT_FAILURE;
        }
    }
    if (udscd)
    {
        if (flag_client == 'o')
            configureInputOutput(false, true, true, false, true, 0, NULL, filepath, 0, 1);
        else
        {
            fprintf(stderr, "Invalid flag\n");
            return EXIT_FAILURE;
        }
    }
    if (udsss)
    {
        printf("using UDSSS\n");
        printf("file location: %s\n", filepath);
        if (flag_server == 'i')
            configureInputOutput(false, true, true, true, false, 0, NULL, filepath, 1, 0);
        else if (flag_server == 'o')
            configureInputOutput(false, true, true, true, false, 0, NULL, filepath, 0, 1);
        else if (flag_server == 'b')
            configureInputOutput(false, true, true, true, false, 0, NULL, filepath, 1, 1);
        else
        {
            fprintf(stderr, "Invalid flag\n");
            return EXIT_FAILURE;
        }
    }
    if (udscs)
    {
        if (flag_client == 'i')
            configureInputOutput(false, true, true, false, true, 0, NULL, filepath, 1, 0);
        else if (flag_client == 'o')
            configureInputOutput(false, true, true, false, true, 0, NULL, filepath, 0, 1);
        else if (flag_client == 'b')
            configureInputOutput(false, true, true, false, true, 0, NULL, filepath, 1, 1);
        else
        {
            fprintf(stderr, "Invalid flag\n");
            return EXIT_FAILURE;
        }
    }

    printf("Input file descriptor: %d\n", input_fd);
    printf("Output file descriptor: %d\n", output_fd);
    if (e_flag)
    {
        if (input_fd != STDIN_FILENO)
        {
            if (dup2(input_fd, STDIN_FILENO) == -1)
            {
                perror("dup2 input");
                close_resources_and_exit(EXIT_FAILURE);
            }
            printf("Input file descriptor changed to %d\n", input_fd);
        }

        if (output_fd != STDOUT_FILENO)
        {
            if (dup2(output_fd, STDOUT_FILENO) == -1)
            {
                perror("dup2 output");
                close_resources_and_exit(EXIT_FAILURE);
            }
            printf("Output file descriptor changed to %d\n", output_fd);
        }

        // Run the program with the given arguments
        run_command(command);
    }
    else
    {
        while (true)
        {
            char buf[255];
            ssize_t size;
            if ((size = read(input_fd, buf, 255)) == -1)
            {
                fprintf(stderr, "Exiting.\n");
                break;
            }

            write(output_fd, buf, size);

            if (time)
                alarm(time);
        }
    }

    return EXIT_SUCCESS;
}

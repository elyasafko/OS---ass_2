#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <stdbool.h>
#include <signal.h>
#include <sys/wait.h>

// Global variables to hold socket file descriptors
int server_fd = -1;
int client_fd = -1;
int new_socket = -1;

/**
 * @brief Close a socket file descriptor if it is open.
 *
 * @param socket_fd The socket file descriptor to close.
 */
void close_socket_if_open(int socket_fd)
{
    if (socket_fd != -1)
    {
        close(socket_fd);
    }
}

/**
 * @brief Close all open file descriptors and exit the process successfully.
 */
void close_resources_and_exit(int)
{
    close_socket_if_open(server_fd);
    close_socket_if_open(client_fd);
    close_socket_if_open(new_socket);

    close(STDIN_FILENO);
    close(STDOUT_FILENO);
    close(STDERR_FILENO);

    exit(EXIT_SUCCESS);
}

/**
 * @brief Run a command in a child process.
 *
 * @param flag The redirection flag. 'b' for both, 'i' for input, 'o' for output.
 * @param exec_command The command to execute. If NULL, stdin is copied to stdout.
 */
void run_command(char flag, const char *exec_command)
{
    // Fork a new process to run the command.
    // If the fork() function returns 0, this is the child process.
    if (fork() == 0)
    {
        // Redirect stdin if necessary.
        // If the flag is 'b' or 'i', redirect stdin to the client socket.
        if (flag == 'b' || flag == 'i')
        {
            dup2(client_fd, STDIN_FILENO);
        }

        // Redirect stdout if necessary.
        // If the flag is 'b' or 'o', redirect stdout to the client socket.
        if (flag == 'b' || flag == 'o')
        {
            dup2(client_fd, STDOUT_FILENO);
        }

        // Close the client socket.
        // We don't need the client socket in the child process.
        close(client_fd);

        // Execute the command.
        // If exec_command is not NULL, execute the command using the shell.
        // If exec_command is NULL, copy stdin to stdout.
        if (exec_command)
        {
            execl("/bin/sh", "sh", "-c", exec_command, (char *)NULL);
            perror("execl failed"); // Print an error message if execl fails.
            exit(EXIT_FAILURE);     // Exit with a failure status.
        }
        else
        {
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
            exit(EXIT_SUCCESS); // Exit with a success status.
        }
    }

    // Wait for the child process to finish.
    // The parent process waits for the child process to exit.
    wait(NULL);

    // Close the client socket.
    // The parent process no longer needs the client socket.
    close(client_fd);
}

// This function starts a TCP server and listens for incoming client connections.
// It forks a child process for each client connection.
// The child process duplicates the client socket file descriptor to STDIN_FILENO and STDOUT_FILENO.
// The child process executes the specified command using the shell or copies stdin to stdout.
// The parent process waits for the child process to finish.
// The parent process closes the client socket.
void start_tcp_server(int port, char flag, const char *exec_command)
{
    // Create a server socket.
    int serverSocket = socket(AF_INET, SOCK_STREAM, 0); // AF_INET specifies IPv4, SOCK_STREAM specifies TCP.
    if (serverSocket == -1)                             // Check if the socket creation was successful.
    {
        perror("Failed to create server socket"); // Print an error message if the socket creation failed.
        exit(EXIT_FAILURE);                       // Exit with a failure status.
    }

    // Set the socket options to reuse the address.
    int opt = 1;
    if (setsockopt(serverSocket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) == -1)
    {
        perror("Failed to set socket options"); // Print an error message if setting the socket options failed.
        close(serverSocket);                    // Close the server socket.
        exit(EXIT_FAILURE);                     // Exit with a failure status.
    }

    // Set up the server address structure.
    struct sockaddr_in serverAddress
    {
    };
    serverAddress.sin_family = AF_INET;         // Set the address family to AF_INET (IPv4).
    serverAddress.sin_addr.s_addr = INADDR_ANY; // Set the server address to INADDR_ANY (listen on all available network interfaces).
    serverAddress.sin_port = htons(port);       // Set the server port.

    // Bind the server socket to the server address.
    if (bind(serverSocket, reinterpret_cast<struct sockaddr *>(&serverAddress), sizeof(serverAddress)) == -1)
    {
        perror("Failed to bind server socket"); // Print an error message if binding the server socket failed.
        close(serverSocket);                    // Close the server socket.
        exit(EXIT_FAILURE);                     // Exit with a failure status.
    }

    // Start listening for incoming client connections.
    if (listen(serverSocket, 3) == -1)
    {
        perror("Failed to listen on server socket"); // Print an error message if listening on the server socket failed.
        close(serverSocket);                         // Close the server socket.
        exit(EXIT_FAILURE);                          // Exit with a failure status.
    }

    // Accept an incoming client connection.
    struct sockaddr_in clientAddress
    {
    };
    socklen_t clientAddressLength = sizeof(clientAddress);
    int clientSocket = accept(serverSocket, reinterpret_cast<struct sockaddr *>(&clientAddress), &clientAddressLength);
    if (clientSocket == -1)
    {
        perror("Failed to accept client connection"); // Print an error message if accepting the client connection failed.
        close(serverSocket);                          // Close the server socket.
        exit(EXIT_FAILURE);                           // Exit with a failure status.
    }

    // Fork a child process for each client connection.
    if (fork() == 0)
    {
        // If the flag is 'b', duplicate the client socket file descriptor to STDIN_FILENO and STDOUT_FILENO.
        if (flag == 'b')
        {
            dup2(clientSocket, STDIN_FILENO);  // Duplicate the client socket file descriptor to STDIN_FILENO.
            dup2(clientSocket, STDOUT_FILENO); // Duplicate the client socket file descriptor to STDOUT_FILENO.
        }
        // If the flag is 'i', duplicate the client socket file descriptor to STDIN_FILENO.
        else if (flag == 'i')
        {
            dup2(clientSocket, STDIN_FILENO); // Duplicate the client socket file descriptor to STDIN_FILENO.
        }
        // If the flag is 'o', duplicate the client socket file descriptor to STDOUT_FILENO.
        else if (flag == 'o')
        {
            dup2(clientSocket, STDOUT_FILENO); // Duplicate the client socket file descriptor to STDOUT_FILENO.
        }
        close(clientSocket); // Close the client socket.
        close(serverSocket); // Close the server socket.

        // Execute the specified command using the shell or copy stdin to stdout.
        if (exec_command)
        {
            execl("/bin/sh", "sh", "-c", exec_command, nullptr); // Execute the command using the shell.
            perror("Failed to execute command");                 // Print an error message if executing the command fails.
            exit(EXIT_FAILURE);                                  // Exit with a failure status.
        }
        else
        {
            char buffer[1024]; // Buffer to hold the data read from stdin.
            while (read(STDIN_FILENO, buffer, sizeof(buffer)) > 0)
            {
                write(STDOUT_FILENO, buffer, sizeof(buffer)); // Write the data to stdout.
            }
            exit(EXIT_SUCCESS); // Exit with a success status.
        }
    }

    // Wait for the child process to finish.
    wait(nullptr);       // Wait for the child process to exit.
    close(clientSocket); // Close the client socket.
    close(serverSocket); // Close the server socket.
}

/**
 * start_tcp_client: Creates a TCP client socket and connects to a server.
 * @param hostname: The hostname or IP address of the server.
 * @param port: The port number of the server.
 * @param flag: The flag indicating the type of communication (b=bidirectional,
 *                                                            i=input, o=output).
 * @param exec_command: The command to be executed by the client.
 */
void start_tcp_client(const char *hostname, int port, char flag, const char *exec_command)
{
    // Declare a struct sockaddr_in variable to hold the server address.
    struct sockaddr_in serv_addr;

    // Create a TCP client socket.
    if ((client_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    {
        perror("Socket creation error"); // Print an error message if socket creation fails.
        exit(EXIT_FAILURE);              // Exit with a failure status.
    }
    printf("Socket created\n"); // Print a message indicating that the socket was created.

    // Set up the server address structure.
    serv_addr.sin_family = AF_INET;   // Set the address family to AF_INET (IPv4).
    serv_addr.sin_port = htons(port); // Set the server port.

    // Check if the hostname is NULL or "localhost".
    if (hostname == NULL || (strcmp(hostname, "localhost") == 0))
    {
        serv_addr.sin_addr.s_addr = inet_addr("127.0.0.1"); // Set the server address to "127.0.0.1" (localhost).
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

    // Print a message indicating that the client is connected to the server.
    printf("Connected\n");

    // Execute the specified command using the client socket.
    run_command(flag, exec_command);
}

void start_udp_server(int port, char flag, const char *exec_command)
{
    struct sockaddr_in address;
    socklen_t addrlen = sizeof(address);
    char buffer[1024] = {0};

    if ((server_fd = socket(AF_INET, SOCK_DGRAM, 0)) == 0)
    {
        perror("socket failed");
        exit(EXIT_FAILURE);
    }

    // Set the socket options to reuse the address
    int opt = 1;
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)))
    {
        perror("setsockopt");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(port);

    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0)
    {
        perror("bind failed");
        close(server_fd);
        exit(EXIT_FAILURE);
    }
    printf("Listening on port %d\n", port);

    if (fork() == 0)
    {
        while (1)
        {
            int len = recvfrom(server_fd, buffer, 1024, MSG_WAITALL, (struct sockaddr *)&address, &addrlen);
            buffer[len] = '\0';
            printf("Received: %s\n", buffer);

            if (flag == 'b' || flag == 'i')
            {
                dup2(server_fd, STDIN_FILENO);
            }
            if (flag == 'b' || flag == 'o')
            {
                dup2(server_fd, STDOUT_FILENO);
            }

            if (exec_command)
            {
                execl("/bin/sh", "sh", "-c", exec_command, (char *)NULL);
                perror("execl failed");
                exit(EXIT_FAILURE);
            }
            else
            {
                while (1)
                {
                    char buffer[1024];
                    int n = read(STDIN_FILENO, buffer, sizeof(buffer));
                    if (n <= 0)
                        break;
                    write(STDOUT_FILENO, buffer, n);
                }
                exit(EXIT_SUCCESS);
            }
        }
    }
}

void start_udp_client(const char *hostname, int port, char flag, const char *exec_command)
{
    struct sockaddr_in serv_addr;
    char buffer[1024] = "Hello from UDP client";

    if ((client_fd = socket(AF_INET, SOCK_DGRAM, 0)) < 0)
    {
        perror("Socket creation error");
        exit(EXIT_FAILURE);
    }

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(port);

    if (strcmp(hostname, "localhost") == 0)
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
    printf("Message sent\n");

    run_command(flag, exec_command);
}

int main(int argc, char *argv[])
{
    if (argc < 2)
        return EXIT_FAILURE;

    char *command = NULL;
    char *server = NULL;
    char *client = NULL;
    char flag_server = 0;
    char flag_client = 0;
    bool e_flag = false;
    bool t_flag = false;
    unsigned int time = 0;

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
                if (strncmp(argv[i + 1], "TCPS", 4) == 0 || strncmp(argv[i + 1], "UDPS", 4) == 0)
                {
                    printf("Argument: %s\n", argv[i + 1]);
                    server = argv[++i];
                    flag_server = flag;
                }
                else if (strncmp(argv[i + 1], "TCPC", 4) == 0 || strncmp(argv[i + 1], "UDPC", 4) == 0)
                {
                    printf("Argument: %s\n", argv[i + 1]);
                    client = argv[++i];
                    flag_client = flag;
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

    if (server == NULL && client == NULL)
    {
        run_command('\0', command);
    }
    else if (server && strncmp(server, "TCPS", 4) == 0)
    {
        int port = atoi(server + 4);
        printf("Port for TCP server: %d\n", port);
        if (port > 0 && port < 65536)
            start_tcp_server(port, flag_server, e_flag ? command : NULL);
    }
    else if (client && strncmp(client, "TCPC", 4) == 0)
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
            // TCPSmyservder,1234
            *port_str = '\0'; // *hostname="myserver\0"
            port_str++;       // *port_str=1234
        }
        int port = atoi(port_str);
        printf("Port for TCP client: %d\n", port);
        if (port > 0 && port < 65536)
            start_tcp_client(hostname, port, flag_client, e_flag ? command : NULL);
    }
    else if (server && strncmp(server, "UDPS", 4) == 0)
    {
        int port = atoi(server + 4);
        printf("Port for UDP server: %d\n", port);
        if (port > 0 && port < 65536)
            start_udp_server(port, flag_server, e_flag ? command : NULL);
    }
    else if (client && strncmp(client, "UDPC", 4) == 0)
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
            // TCPSmyservder,1234
            *port_str = '\0'; // *hostname="myserver\0"
            port_str++;       // *port_str=1234
        }
        *port_str = '\0';
        int port = atoi(port_str);
        printf("Port for UDP client: %d\n", port);
        if (port > 0 && port < 65536)
            start_udp_client(hostname, port, flag_client, e_flag ? command : NULL);
    }

    return EXIT_SUCCESS;
}

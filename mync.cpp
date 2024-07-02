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

#define MAX_FILEPATH 256
// Global variables to hold socket file descriptors
int input_fd = STDIN_FILENO;
int output_fd = STDOUT_FILENO;

int child = 0;

/**
 * @brief Close the open socket file descriptors.
 *
 * This function closes the input and output socket file descriptors.
 * If the input file descriptor is not STDIN_FILENO, it closes the input file descriptor.
 * If the output file descriptor is not STDOUT_FILENO, it closes the output file descriptor.
 */
void closeOpenSockets()
{
    // Close the input file descriptor if it is not STDIN_FILENO
    if (input_fd != STDIN_FILENO)
    {
        close(input_fd); // Close the input file descriptor
    }

    // Close the output file descriptor if it is not STDOUT_FILENO
    if (output_fd != STDOUT_FILENO)
    {
        close(output_fd); // Close the output file descriptor
    }
}

/**
 * @brief Close the open socket file descriptors and terminate the program.
 *
 * This function closes the input and output socket file descriptors,
 * and terminates the program with the given exit code.
 *
 * @param exitCode The exit code to terminate the program with.
 */
void closeResourcesAndExit(int exitCode)
{
    // Close the open socket file descriptors.
    closeOpenSockets();

    // If the child process is running, terminate it.
    if (child != 0)
    {
        kill(child, SIGKILL);
    }

    // Terminate the program with the given exit code.
    exit(exitCode);
}

/**
 * @brief Execute the given command using the shell or copy stdin to stdout if no command is given.
 *
 * If a command is given, it executes the command using the shell. If no command is given, it copies
 * the standard input to the standard output.
 *
 * @param command The command to execute or nullptr to copy stdin to stdout.
 */
void executeCommand(const char *command)
{
    // If a command is given, execute it using the shell
    if (command)
    {
        // Execute the command using the shell
        execl("/bin/sh", "sh", "-c", command, nullptr);

        // If the command execution fails, print an error message and exit with failure
        perror("Failed to execute command");
        exit(EXIT_FAILURE);
    }
    // If no command is given, exit with success
    exit(EXIT_SUCCESS);
}

void chat_stdin_to_stdout()
{
    const size_t bufferSize = 1024;
    char buffer[bufferSize];

    while (true)
    {
        ssize_t bytesRead = read(STDIN_FILENO, buffer, bufferSize);
        if (bytesRead <= 0)
        {
            break;
        }

        write(STDOUT_FILENO, buffer, static_cast<size_t>(bytesRead));
    }

    closeResourcesAndExit(EXIT_SUCCESS);
}

/**
 * start_tcp_server: Creates a TCP server socket, sets socket options, binds the socket to a port,
 *                  and starts listening for incoming client connections.
 * @param port: The port number to listen on.
 * @return The client socket file descriptor, or -1 if an error occurred.
 */
int start_tcp_server(int port)
{
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd == -1)
    {
        perror("Failed to create server socket");
        return -1;
    }

    int opt = 1;
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) == -1)
    {
        perror("Failed to set socket options");
        close(server_fd);
        return -1;
    }

    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(port);

    if (bind(server_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) == -1)
    {
        perror("Failed to bind server socket");
        close(server_fd);
        return -1;
    }

    if (listen(server_fd, 1) == -1)
    {
        perror("Failed to listen on server socket");
        close(server_fd);
        return -1;
    }

    struct sockaddr_in client_addr;
    socklen_t client_addr_len = sizeof(client_addr);
    int client_fd = accept(server_fd, (struct sockaddr *)&client_addr, &client_addr_len);
    if (client_fd == -1)
    {
        perror("Failed to accept client connection");
        close(server_fd);
        return -1;
    }

    return client_fd;
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

    if (!(strcmp(hostname, "127.0.0.1")))
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
    return server_fd;
}

int start_udp_client(const char *hostname, int port)
{
    struct sockaddr_in serv_addr;
    // char buffer[1024] = "Hello from UDP client";
    int client_fd;
    if ((client_fd = socket(AF_INET, SOCK_DGRAM, 0)) < 0)
    {
        perror("Socket creation error");
        exit(EXIT_FAILURE);
    }

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(port);

    if (!(strcmp(hostname, "127.0.0.1")))
    {
        if (inet_pton(AF_INET, hostname, &serv_addr.sin_addr) <= 0)
        {
            perror("Invalid address/ Address not supported");
            close(client_fd);
            exit(EXIT_FAILURE);
        }
    }

    // sendto(client_fd, buffer, strlen(buffer), MSG_CONFIRM, (const struct sockaddr *)&serv_addr, sizeof(serv_addr));
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
        closeResourcesAndExit(EXIT_FAILURE);
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
        closeResourcesAndExit(EXIT_FAILURE);
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
        closeResourcesAndExit(EXIT_FAILURE);
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
        closeResourcesAndExit(EXIT_FAILURE);
    }
    printf("Socket created\n");
    struct sockaddr_un addr;
    addr.sun_family = AF_UNIX;
    strcpy(addr.sun_path, path);
    unlink(addr.sun_path);
    if (bind(sockfd, (struct sockaddr *)&addr, sizeof(addr)) == -1)
    {
        perror("error binding socket");
        closeResourcesAndExit(EXIT_FAILURE);
    }
    printf("Socket bound\n");
    if (listen(sockfd, 5) == -1)
    {
        perror("error listening");
        closeResourcesAndExit(EXIT_FAILURE);
    }
    printf("Listening for connections\n");
    struct sockaddr_un client_addr;
    socklen_t client_addr_len = sizeof(client_addr);
    int client_fd = accept(sockfd, (struct sockaddr *)&client_addr, &client_addr_len);
    if (client_fd == -1)
    {
        perror("error accepting connection");
        closeResourcesAndExit(EXIT_FAILURE);
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
        closeResourcesAndExit(EXIT_FAILURE);
    }

    struct sockaddr_un addr;
    addr.sun_family = AF_UNIX;
    strcpy(addr.sun_path, path);
    printf("Connecting to server\n");
    if (connect(sockfd, (struct sockaddr *)&addr, sizeof(addr)) == -1)
    {
        perror("error connecting to server");
        closeResourcesAndExit(EXIT_FAILURE);
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
        closeResourcesAndExit(EXIT_FAILURE);
    }

    struct sockaddr_un addr;
    addr.sun_family = AF_UNIX;
    strcpy(addr.sun_path, path);
    printf("Connecting to server\n");
    if (connect(sockfd, (struct sockaddr *)&addr, sizeof(addr)) == -1)
    {
        perror("error connecting to server");
        closeResourcesAndExit(EXIT_FAILURE);
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

    if (hostname == NULL || (strcmp(hostname, "localhost") == 0))
    {
        char hostname[16]; // Make sure the size is sufficient to hold the string
        strcpy(hostname, "127.0.0.1");

        printf("Server address set to localhost as 127.0.0.1\n");
    }

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
        closeResourcesAndExit(EXIT_FAILURE);
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

    // Allocate memory for the path
    static char path[MAX_FILEPATH];
    int i = 0;

    // Extract the path until you find a space or end of string
    while (*start != ' ' && *start != '\0' && i < 255)
    {
        path[i++] = *start++;
    }
    path[i] = '\0'; // Null-terminate the string
    return path;
}

void print_usage(const char *progname)
{
    printf("Usage: %s [-e command] [-t time] [-i|-o|-b argument]\n", progname);
}

int main(int argc, char *argv[])
{
    if (argc < 2)
        return EXIT_FAILURE;

    int opt;
    char *command = NULL;
    char *server = NULL;
    char *client = NULL;
    static char ifilepath[MAX_FILEPATH] = {0};
    static char ofilepath[MAX_FILEPATH] = {0};
    char flag_server = '\0';
    char flag_client = '\0';
    bool e_flag = false;
    bool t_flag = false;
    unsigned int time = 0;
    bool udsss = false;
    bool udscs = false;
    bool udssd = false;
    bool udscd = false;

    while ((opt = getopt(argc, argv, "e:t:i:o:b:")) != -1)
    {
        switch (opt)
        {
        case 'e':
            e_flag = true;
            command = optarg;
            printf("Command: %s\n", command);
            break;
        case 't':
            t_flag = true;
            time = atoi(optarg);
            if (time == 0)
            {
                printf("Error: timeout param error\n");
                return EXIT_FAILURE;
            }
            printf("Time: %d\n", time);
            break;
        case 'i':
        case 'o':
        case 'b':
            printf("Flag: %c\n", opt);
            if (strncmp(optarg, "TCPS", 4) == 0)
            {
                printf("Argument: %s\n", optarg);
                server = optarg;
                flag_server = opt;
                printf("Flag server: %c\n", flag_server);
            }
            else if (strncmp(optarg, "TCPC", 4) == 0)
            {
                printf("Argument: %s\n", optarg);
                client = optarg;
                flag_client = opt;
                printf("Flag client: %c\n", flag_client);
            }
            else if (strncmp(optarg, "UDPS", 4) == 0)
            {
                printf("Argument: %s\n", optarg);
                server = optarg;
                flag_server = opt;
                printf("Flag server: %c\n", flag_server);
            }
            else if (strncmp(optarg, "UDPC", 4) == 0)
            {
                printf("Argument: %s\n", optarg);
                client = optarg;
                flag_client = opt;
                printf("Flag client: %c\n", flag_client);
            }
            else if (strncmp(optarg, "UDS", 3) == 0)
            {
                // Extract path from UDS argument
                char *file_location = extract_path(optarg);
                char *fp = (opt == 'i') ? ifilepath : ofilepath;
                if (file_location != NULL && (strlen(file_location) > 0))
                {
                    strncpy(fp, file_location, MAX_FILEPATH - 1); // Copy extracted path to filepath
                    fp[MAX_FILEPATH - 1] = '\0';                  // Ensure null-termination
                    printf("Extracted path: %s\n", fp);
                }
                else
                {
                    printf("Invalid UDS argument\n");
                    return EXIT_FAILURE;
                }

                if (strncmp(optarg, "UDSSS", 5) == 0)
                {
                    printf("UDS server using stream\n");
                    printf("file_location Path: %s\n", file_location);
                    flag_server = opt;
                    printf("Flag server: %c\n", flag_server);
                    udsss = true;
                }
                if (strncmp(optarg, "UDSSD", 5) == 0)
                {
                    printf("UDS server using datagram\n");
                    printf("file_location Path: %s\n", file_location);
                    flag_server = opt;
                    printf("Flag server: %c\n", flag_server);
                    udssd = true;
                }
                if (strncmp(optarg, "UDSCD", 5) == 0)
                {
                    printf("UDS client using datagram\n");
                    printf("file_location Path: %s\n", file_location);
                    flag_client = opt;
                    printf("Flag client: %c\n", flag_client);
                    udscd = true;
                }
                if (strncmp(optarg, "UDSCS", 5) == 0)
                {
                    printf("UDS client using stream\n");
                    printf("file_location Path: %s\n", file_location);
                    flag_client = opt;
                    printf("Flag client: %c\n", flag_client);
                    udscs = true;
                }
            }
            else
            {
                fprintf(stderr, "Invalid TCP/UDP argument\n");
                return EXIT_FAILURE;
            }
            break;
        default:
            print_usage(argv[0]);
            return EXIT_FAILURE;
        }
    }

    printf("Server: %s\n", server ?: "localhost");

    if (t_flag)
    {
        printf("Setting timer to : %d seconds\n", time);
        signal(SIGALRM, closeResourcesAndExit);
        alarm(time);
    }

    if (server == NULL && client == NULL && e_flag == false && !(udssd || udsss || udscs || udscd))
    {
        printf("no excute given\n");
        chat_stdin_to_stdout();
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
            configureInputOutput(false, true, true, true, false, 0, NULL, ifilepath, 1, 0);
        else
        {
            fprintf(stderr, "Invalid flag\n");
            return EXIT_FAILURE;
        }
    }
    if (udscd)
    {
        if (flag_client == 'o')
            configureInputOutput(false, true, true, false, true, 0, NULL, ofilepath, 0, 1);
        else
        {
            fprintf(stderr, "Invalid flag\n");
            return EXIT_FAILURE;
        }
    }
    if (udsss)
    {
        printf("using UDSSS\n");
        char *fp = (flag_server == 'i') ? ifilepath : ofilepath;
        printf("file location: %s\n", fp);
        if (flag_server == 'i')
            configureInputOutput(false, true, true, true, false, 0, NULL, fp, 1, 0);
        else if (flag_server == 'o')
            configureInputOutput(false, true, true, true, false, 0, NULL, fp, 0, 1);
        else if (flag_server == 'b')
            configureInputOutput(false, true, true, true, false, 0, NULL, fp, 1, 1);
        else
        {
            fprintf(stderr, "Invalid flag\n");
            return EXIT_FAILURE;
        }
    }
    if (udscs)
    {
        printf("using UDSCS\n");
        char *fp = (flag_client == 'i') ? ifilepath : ofilepath;
        printf("file location: %s\n", fp);
        if (flag_client == 'i')
            configureInputOutput(false, true, true, false, true, 0, NULL, fp, 1, 0);
        else if (flag_client == 'o')
            configureInputOutput(false, true, true, false, true, 0, NULL, fp, 0, 1);
        else if (flag_client == 'b')
            configureInputOutput(false, true, true, false, true, 0, NULL, fp, 1, 1);
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
                closeResourcesAndExit(EXIT_FAILURE);
            }
            printf("Input file descriptor changed to %d\n", input_fd);
        }

        if (output_fd != STDOUT_FILENO)
        {
            if (dup2(output_fd, STDOUT_FILENO) == -1)
            {
                perror("dup2 output");
                closeResourcesAndExit(EXIT_FAILURE);
            }
            printf("Output file descriptor changed to %d\n", output_fd);
        }

        // Run the program with the given arguments
        executeCommand(command);
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

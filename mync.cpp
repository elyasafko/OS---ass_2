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

void close_process(int /*sig*/)
{
    // Close the sockets if they are open
    if (server_fd != -1)
        close(server_fd);
    if (client_fd != -1)
        close(client_fd);
    if (new_socket != -1)
        close(new_socket);

    // Close standard input/output
    close(STDIN_FILENO);
    close(STDOUT_FILENO);
    close(STDERR_FILENO);

    exit(EXIT_SUCCESS);
}

void start_tcp_server(int port, char flag, const char *exec_command)
{
    struct sockaddr_in address;
    int addrlen = sizeof(address);

    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0)
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

    if (listen(server_fd, 3) < 0)
    {
        perror("listen");
        close(server_fd);
        exit(EXIT_FAILURE);
    }
    printf("Waiting for incoming connection\n");

    if ((new_socket = accept(server_fd, (struct sockaddr *)&address, (socklen_t *)&addrlen)) < 0)
    {
        perror("accept");
        close(server_fd);
        exit(EXIT_FAILURE);
    }
    printf("Connection accepted\n");

    // Create a child process to handle the incoming connection
    if (fork() == 0)
    {
        if (flag == 'b')
        {
            dup2(new_socket, STDIN_FILENO);
            dup2(new_socket, STDOUT_FILENO);
        }
        else if (flag == 'i')
        {
            dup2(new_socket, STDIN_FILENO);
        }
        else if (flag == 'o')
        {
            dup2(new_socket, STDOUT_FILENO);
        }
        close(new_socket);
        close(server_fd);

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

    wait(NULL); // Wait for child process
    close(new_socket);
    close(server_fd);
}

void start_tcp_client(const char *hostname, int port, char flag, const char *exec_command)
{
    struct sockaddr_in serv_addr;

    if ((client_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    {
        perror("Socket creation error");
        exit(EXIT_FAILURE);
    }
    printf("Socket created\n");

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
    printf("Connecting to %s:%d\n", hostname, port);

    if (connect(client_fd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)
    {
        perror("Connection failed");
        close(client_fd);
        exit(EXIT_FAILURE);
    }

    printf("Connected\n");

    if (fork() == 0)
    {
        if (flag == 'b')
        {
            dup2(client_fd, STDIN_FILENO);
            dup2(client_fd, STDOUT_FILENO);
        }
        else if (flag == 'i')
        {
            dup2(client_fd, STDIN_FILENO);
        }
        else if (flag == 'o')
        {
            dup2(client_fd, STDOUT_FILENO);
        }
        close(client_fd);

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

    wait(NULL); // Wait for child process
    close(client_fd);
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

    if (fork() == 0)
    {
        if (flag == 'b' || flag == 'i')
        {
            dup2(client_fd, STDIN_FILENO);
        }
        if (flag == 'b' || flag == 'o')
        {
            dup2(client_fd, STDOUT_FILENO);
        }
        close(client_fd);

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

    wait(NULL); // Wait for child process
    close(client_fd);
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

    printf("Server: %s\n", server);

    if (t_flag)
    {
        signal(SIGALRM, close_process);
        alarm(time);
    }

    if (server && strncmp(server, "TCPS", 4) == 0)
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
        *port_str = '\0';
        int port = atoi(port_str + 1);
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
        *port_str = '\0';
        int port = atoi(port_str + 1);
        printf("Port for UDP client: %d\n", port);
        if (port > 0 && port < 65536)
            start_udp_client(hostname, port, flag_client, e_flag ? command : NULL);
    }

    return EXIT_SUCCESS;
}

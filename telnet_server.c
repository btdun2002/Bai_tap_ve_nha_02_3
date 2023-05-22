#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <string.h>
#include <sys/select.h>
#include <sys/ioctl.h>
#include <errno.h>
#include <time.h>

#define MAX_CLIENTS 64
#define MAX_USERNAME_LENGTH 32
#define MAX_PASSWORD_LENGTH 32

struct client_info
{
    int fd;
    char username[MAX_USERNAME_LENGTH];
    int authenticated;
};

int main()
{
    int listener = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (listener == -1)
    {
        perror("socket failed");
        return 1;
    }

    unsigned long ul = 1;
    ioctl(listener, FIONBIO, &ul);

    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(9000);

    if (bind(listener, (struct sockaddr *)&addr, sizeof(addr)))
    {
        perror("bind() failed");
        return 1;
    }

    if (listen(listener, 5))
    {
        perror("listen() failed");
        return 1;
    }

    fd_set fdread;

    struct client_info clients[MAX_CLIENTS] = {0};
    int num_clients = 0;
    char buf[256];
    char message[1024];

    printf("Server started. Listening on port 9000\n");

    while (1)
    {
        FD_ZERO(&fdread);
        FD_SET(listener, &fdread);
        for (int i = 0; i < num_clients; i++)
            FD_SET(clients[i].fd, &fdread);

        int ret = select(FD_SETSIZE, &fdread, NULL, NULL, NULL);
        if (ret < 0)
        {
            perror("select() failed");
            break;
        }

        if (FD_ISSET(listener, &fdread))
        {
            if (num_clients < 64)
            {
                int client = accept(listener, NULL, NULL);

                if (client == -1)
                {
                    if (errno == EWOULDBLOCK || errno == EAGAIN)
                    {
                        // Van dang cho ket noi
                        // Bo qua khong xu ly
                        continue;
                    }
                    else
                    {
                        perror("accept() failed");
                        break;
                    }
                }
                else
                {
                    clients[num_clients].fd = client;
                    unsigned long ulong = 1;
                    ioctl(client, FIONBIO, &ulong);
                    printf("New client connected %d\n", client);
                    num_clients++;
                }
            }
        }

        for (int i = 0; i < num_clients; i++)
        {
            if (FD_ISSET(clients[i].fd, &fdread))
            {
                char buf[256];
                ret = recv(clients[i].fd, buf, sizeof(buf), 0);
                if (ret == -1)
                {
                    if (errno == EWOULDBLOCK || errno == EAGAIN)
                    {
                        // Van dang doi du lieu
                        // Bo qua chuyen sang socket khac
                        continue;
                    }
                    else
                    {
                        perror("recv() failed");
                        continue;
                    }
                }
                else if (ret == 0)
                {
                    printf("Client disconnected %d\n", clients[i].fd);
                    close(clients[i].fd);
                    for (int j = i; j < num_clients - 1; j++)
                    {
                        clients[j] = clients[j + 1];
                    }
                    num_clients--;
                    i--;
                    continue;
                }
                else
                {
                    buf[strcspn(buf, "\n")] = '\0';
                    printf("Received from client %d: %s\n", clients[i].fd, buf);

                    if (clients[i].authenticated == 0)
                    {
                        // Yêu cầu thông tin đăng nhập từ client
                        char *username = strtok(buf, " ");
                        char *password = strtok(NULL, " ");

                        if (username == NULL || password == NULL)
                        {
                            send(clients[i].fd, "Invalid input format. Please send 'username password' again\n", strlen("Invalid input format. Please send 'username password' again\n"), 0);
                            close(clients[i].fd);
                            continue;
                        }

                        FILE *fp = fopen("database.txt", "r");
                        if (fp == NULL)
                        {
                            perror("Failed to open database file");
                            send(clients[i].fd, "Internal server error. Please try again later\n", strlen("Internal server error. Please try again later\n"), 0);
                            continue;
                        }

                        int authenticated = 0;
                        char line[256];
                        while (fgets(line, sizeof(line), fp) != NULL)
                        {
                            line[strcspn(line, "\n")] = '\0';
                            char db_username[MAX_USERNAME_LENGTH];
                            char db_password[MAX_PASSWORD_LENGTH];
                            sscanf(line, "%s %s", db_username, db_password);

                            if (strcmp(username, db_username) == 0 && strcmp(password, db_password) == 0)
                            {
                                authenticated = 1;
                                strcpy(clients[i].username, username);
                                clients[i].authenticated = 1;
                                break;
                            }
                        }

                        fclose(fp);

                        if (authenticated == 0)
                        {
                            send(clients[i].fd, "Invalid username or password\n", strlen("Invalid username or password\n"), 0);
                            close(clients[i].fd);
                            for (int j = i; j < num_clients - 1; j++)
                            {
                                clients[j] = clients[j + 1];
                            }
                            num_clients--;
                            i--;
                            continue;
                        }
                        else
                        {
                            send(clients[i].fd, "Authentication successful. You are now logged in\n", strlen("Authentication successful. You are now logged in\n"), 0);
                            continue;
                        }
                    }
                    else
                    {
                        // Client đã được xác thực, thực hiện lệnh và trả kết quả
                        FILE *fp = popen(buf, "r");
                        if (fp == NULL)
                        {
                            perror("Failed to execute command");
                            send(clients[i].fd, "Failed to execute command. Please try again later\n", strlen("Failed to execute command. Please try again later\n"), 0);
                            continue;
                        }

                        char result[1024];
                        int bytesRead = fread(result, 1, sizeof(result) - 1, fp);
                        result[bytesRead] = '\0';
                        pclose(fp);

                        send(clients[i].fd, result, strlen(result), 0);
                    }
                }
            }
        }
    }

    close(listener);

    printf("Done...");

    return 0;
}

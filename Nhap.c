#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <string.h>
#include <sys/ioctl.h>
#include <errno.h>
#include <time.h>
#include <poll.h>

struct client_info
{
    int fd;
    char client_name[32];
    char client_id[32];
    int is_set_info;
};

int main()
{
    int listener = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (listener == -1)
    {
        perror("socket() failed");
        return 1;
    }

    unsigned long ulong_value = 1;
    ioctl(listener, FIONBIO, &ulong_value);

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

    struct pollfd fds[64];
    int num_fds = 1;

    fds[0].fd = listener;
    fds[0].events = POLLIN;

    struct client_info clients[64] = {0};
    int num_clients = 0;
    char buf[256];
    char message[1024];
    char *client_id;
    char *client_name;
    int ret;

    printf("Server started. Listening on port 9000\n");

    while (1)
    {
        ret = poll(fds, num_fds, -1);
        if (ret < 0)
        {
            perror("poll() failed");
            break;
        }

        if (fds[0].revents & POLLIN)
        {
            if (num_clients < 64)
            {
                int client = accept(listener, NULL, NULL);

                if (client == -1)
                {
                    if (errno == EWOULDBLOCK || errno == EAGAIN)
                    {
                        // Vẫn đang chờ kết nối
                        // Bỏ qua và không xử lý
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

                    fds[num_fds].fd = client;
                    fds[num_fds].events = POLLIN;
                    num_fds++;
                }
            }
        }

        for (int i = 1; i < num_fds; i++)
        {
            if (fds[i].revents & POLLIN)
            {
                ret = recv(fds[i].fd, buf, sizeof(buf), 0);
                if (ret == -1)
                {
                    if (errno == EWOULDBLOCK || errno == EAGAIN)
                    {
                        // Vẫn đang chờ dữ liệu
                        // Bỏ qua và chuyển sang socket khác
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
                    printf("Client disconnected %d\n", fds[i].fd);
                    close(fds[i].fd);
                    for (int j = i; j < num_fds - 1; j++)
                    {
                        fds[j] = fds[j + 1];
                    }
                    num_fds--;

                    for (int j = 0; j < num_clients; j++)
                    {
                        if (clients[j].fd == fds[i].fd)
                        {
                            for (int k = j; k < num_clients - 1; k++)
                            {
                                clients[k] = clients[k + 1];
                            }
                            num_clients--;
                            break;
                        }
                    }

                    i--;
                    continue;
                }
                else
                {
                    if (clients[i - 1].is_set_info == 0)
                    {
                        printf("Received from %d: %s\n", fds[i].fd, buf);
                        buf[ret] = 0;
                        client_id = strtok(buf, ":");
                        client_name = strtok(NULL, ":");
                        if (client_name == NULL || client_id == NULL || strlen(client_name) == 0 || strlen(client_id) == 0)
                        {
                            send(fds[i].fd, "Invalid input format. Please send 'client_id: client_name' again\n", strlen("Invalid input format. Please send 'client_id: client_name' again\n"), 0);
                            close(fds[i].fd);

                            for (int j = i; j < num_fds - 1; j++)
                            {
                                fds[j] = fds[j + 1];
                            }
                            num_fds--;

                            for (int j = 0; j < num_clients; j++)
                            {
                                if (clients[j].fd == fds[i].fd)
                                {
                                    for (int k = j; k < num_clients - 1; k++)
                                    {
                                        clients[k] = clients[k + 1];
                                    }
                                    num_clients--;
                                    break;
                                }
                            }

                            i--;
                            continue;
                        }
                        strcpy(clients[i - 1].client_name, client_name);
                        strcpy(clients[i - 1].client_id, client_id);
                        clients[i - 1].is_set_info = 1;
                    }
                    else if (clients[i - 1].is_set_info == 1)
                    {
                        buf[ret] = 0;
                        time_t now = time(NULL);
                        struct tm *t = localtime(&now);
                        snprintf(message, sizeof(message), "%4d-%2d-%2d %2d:%2d:%2d %s: %s",
                                 t->tm_year + 1900, t->tm_mon + 1, t->tm_mday,
                                 t->tm_hour, t->tm_min, t->tm_sec, clients[i - 1].client_name, buf);

                        for (int j = 0; j < num_clients; j++)
                        {
                            if (j == i - 1)
                                continue;
                            ret = send(clients[j].fd, message, strlen(message), 0);
                            if (ret == -1)
                            {
                                perror("send() failed");
                                return 1;
                            }
                        }
                    }
                    else
                    {
                        perror("Error is_set_info");
                        return 1;
                    }
                }
            }
        }
    }

    close(listener);

    return 0;
}

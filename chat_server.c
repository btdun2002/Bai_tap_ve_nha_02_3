#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <string.h>
#include <poll.h>
#include <sys/ioctl.h>
#include <errno.h>
#include <time.h>

#define MAX_CLIENTS 64

struct client_info
{
    char client_name[32];
    char client_id[32];
    int is_set_info;
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

    struct pollfd fds[MAX_CLIENTS];
    int nfds = 1;

    fds[0].fd = listener;
    fds[0].events = POLLIN;

    struct client_info clients[MAX_CLIENTS] = {0};
    char *client_id;
    char *client_name;

    char buf[256];
    char message[1024];

    printf("Server started. Listening on port 9000\n");

    while (1)
    {
        int ret = poll(fds, nfds, -1);
        if (ret < 0)
        {
            perror("poll() failed");
            break;
        }

        printf("ret = %d\n", ret);

        if (fds[0].revents & POLLIN)
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
            if (nfds < MAX_CLIENTS)
            {
                printf("New client connected %d\n", client);
                fds[nfds].fd = client;
                fds[nfds].events = POLLIN;
                nfds++;

                unsigned long ul = 1;
                ioctl(client, FIONBIO, &ul);
            }
            else
            {
                printf("Too many connections\n");
                close(client);
            }
        }

        for (int i = 1; i < nfds; i++)
        {
            if (fds[i].revents & POLLIN)
            {
                ret = recv(fds[i].fd, buf, sizeof(buf), 0);
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
                    printf("Client %d disconnected.\n", fds[i].fd);
                    close(fds[i].fd);

                    if (i < nfds - 1)
                    {
                        fds[i] = fds[nfds - 1];
                        clients[i] = clients[nfds - 1];
                    }
                    nfds--;
                    i--;
                    continue;
                }
                else
                {
                    if (clients[i].is_set_info == 0)
                    {
                        buf[strcspn(buf, "\n")] = '\0';
                        printf("Received from client %d: %s\n", fds[i].fd, buf);
                        client_id = strtok(buf, ":");
                        client_name = strtok(NULL, ":");
                        if (client_name == NULL || client_id == NULL || strlen(client_name) == 0 || strlen(client_id) == 0)
                        {
                            send(fds[i].fd, "Invalid input format. Please send 'client_id: client_name' again\n", strlen("Invalid input format. Please send 'client_id: client_name' again\n"), 0);
                            close(fds[i].fd);
                            continue;
                        }
                        strcpy(clients[i].client_name, client_name);
                        strcpy(clients[i].client_id, client_id);
                        clients[i].is_set_info = 1;
                    }
                    else if (clients[i].is_set_info == 1)
                    {
                        buf[strcspn(buf, "\n")] = '\0';
                        printf("Received from client %d: %s\n", fds[i].fd, buf);
                        if (buf[0] == '@')
                        {
                            // Tin nhắn riêng
                            char *recipient_id = strtok(buf + 1, ":");
                            char *message_content = strtok(NULL, ":");
                            if (recipient_id != NULL && message_content != NULL)
                            {
                                int recipient_fd;
                                for (int j = 1; j < nfds; j++)
                                {
                                    if (j == i)
                                        continue;
                                    if (strcmp(clients[j].client_id, recipient_id) == 0)
                                    {
                                        recipient_fd = fds[j].fd;
                                        break;
                                    }
                                }
                                snprintf(message, sizeof(message), "%s: %s\n", clients[i].client_name, message_content);
                                send(recipient_fd, message, strlen(message), 0);
                            }
                            else
                            {
                                send(fds[i].fd, "Invalid input format. Please send '@recipient_id: message_content' again\n", strlen("Invalid input format. Please send '@recipient_id: message_content' again\n"), 0);
                            }
                        }
                        else
                        {
                            time_t now = time(NULL);
                            struct tm *t = localtime(&now);
                            snprintf(message, sizeof(message), "%4d-%2d-%2d %2d:%2d:%2d %s: %s\n",
                                     t->tm_year + 1900, t->tm_mon + 1, t->tm_mday,
                                     t->tm_hour, t->tm_min, t->tm_sec, clients[i].client_name, buf);

                            for (int j = 1; j < nfds; j++)
                            {
                                if (j == i)
                                    continue;
                                ret = send(fds[j].fd, message, strlen(message), 0);
                                if (ret == -1)
                                {
                                    perror("send() failed");
                                    return 1;
                                }
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

    printf("Done...");
    return 0;
}
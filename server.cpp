#include "helpers.h"

using namespace std;

typedef struct {
    bool connect;
    string id;
    int socket;
} client;

void handleSubscribe(int clientSocket, const tcpmessage &tcpmesaj, unordered_map<string, unordered_map<string, int>> &topicSubscribers, client *clients)
{
    string clientID;
    for(int i = 0; i < MAX_CLIENTS; i++)
    {
        if(clients[i].socket == clientSocket)
        {
            clientID = clients[i].id;
            break;
        }
    }
    string topic = tcpmesaj.topic;

    if (topicSubscribers.find(topic) == topicSubscribers.end())
    {
        topicSubscribers[topic] = unordered_map<string, int>();
    }

    topicSubscribers[topic][clientID] = 1;
}

void handleUnsubscribe(int clientSocket, const tcpmessage &tcpmesaj, unordered_map<string, unordered_map<string, int>> &topicSubscribers, client *clients) {
    string clientID;
    for(int i = 0; i < MAX_CLIENTS; i++)
    {
        if(clients[i].socket == clientSocket)
        {
            clientID = clients[i].id;
            break;
        }
    }
    string topic = tcpmesaj.topic;

    if (topicSubscribers.find(topic) != topicSubscribers.end())
    {
        topicSubscribers[topic].erase(clientID);
    }
}

int main(int argc, char *argv[])
{
    setvbuf(stdout, NULL, _IONBF, BUFSIZ);
    int tcpsocket, udpsocket, portserver, fdmax, rc;
    char buffer[BUFLEN];
    fd_set read_fds, tmp_fds;
    struct sockaddr_in server_addr, cli_addr;
    client clients[MAX_CLIENTS];
    for(int i = 0; i < MAX_CLIENTS; i++)
    {
        clients[i].connect = false;
        clients[i].id = "";
        clients[i].socket = -1;
    }
    unordered_map<string, unordered_map<string, int>> topicuri;

    portserver = atoi(argv[1]);

    FD_ZERO(&read_fds);
    FD_SET(0, &read_fds);
    FD_ZERO(&tmp_fds);

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(portserver);
    server_addr.sin_addr.s_addr = INADDR_ANY;

    udpsocket = socket(AF_INET, SOCK_DGRAM, 0);
    DIE(udpsocket < 0, "socket");

    rc = bind(udpsocket, (struct sockaddr *)&server_addr, sizeof(server_addr));
    DIE(rc < 0, "bind udp");

    tcpsocket = socket(AF_INET, SOCK_STREAM, 0);
    DIE(tcpsocket < 0, "socket tcp");

    int flag = 1;
    rc = setsockopt(tcpsocket, IPPROTO_TCP, TCP_NODELAY, (char *)&flag, sizeof(int));
    DIE(rc < 0, "setsockopt");

    rc = bind(tcpsocket, (struct sockaddr *)&server_addr, sizeof(server_addr));
    DIE(rc < 0, "bind tcp");

    int enable = 1;
    if (setsockopt(tcpsocket, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int)) < 0)
        perror("setsockopt(SO_REUSEADDR) failed");

    rc = listen(tcpsocket, MAX_CLIENTS);
    DIE(rc < 0, "listen");

    FD_SET(tcpsocket, &read_fds);
    FD_SET(udpsocket, &read_fds);

    fdmax = max(tcpsocket, udpsocket);
    socklen_t len = sizeof(server_addr);
    while (1)
    {
        tmp_fds = read_fds;

        rc = select(fdmax + 1, &tmp_fds, NULL, NULL, NULL);
        DIE(rc < 0, "select");

        if (FD_ISSET(0, &tmp_fds))
        {
            memset(buffer, 0, BUFLEN);
            fgets(buffer, BUFLEN, stdin);
            if (strcmp(buffer, "exit\n") == 0)
            {
                for (int i = 0; i <= fdmax; i++)
                {
                    if (FD_ISSET(i, &read_fds))
                    {
                        close(i);
                    }
                }
                break;
            }
        }

        for (int i = 1; i <= fdmax; i++)
        {
            if (FD_ISSET(i, &tmp_fds))
            {
                memset(buffer, 0, BUFLEN);
                if (i == udpsocket)
                {
                    memset(buffer, 0, BUFLEN);
                    rc = recvfrom(udpsocket, buffer, BUFLEN, 0, (struct sockaddr *)&cli_addr, &len);
                    DIE(rc < 0, "recvfrom udp");
                    udpmessage *mes = (udpmessage *)buffer;
                    
                    if (topicuri.find(mes->topic) != topicuri.end())
                    {
                        for (auto it = topicuri[mes->topic].begin(); it != topicuri[mes->topic].end(); it++)
                        {
                            if (it->second == 1)
                            {
                                int sock = -1;
                                for(int j = 0; j < MAX_CLIENTS; j++)
                                {
                                    if(clients[j].id.compare(it->first) == 0 && clients[j].connect == true)
                                    {
                                        sock = clients[j].socket;
                                        break;
                                    }
                                }
                                if(sock != -1)
                                {
                                    rc = send(sock, mes, sizeof(udpmessage), 0);
                                    DIE(rc < 0, "send udp");
                                }
                            }
                        }
                    }
                }
                else if (i == tcpsocket)
                {
                    int newsockfd = accept(tcpsocket, (struct sockaddr *)&cli_addr, &len);
                    DIE(newsockfd < 0, "accept client tcp");

                    flag = 1;
                    rc = setsockopt(newsockfd, IPPROTO_TCP, TCP_NODELAY, (char *)&flag, sizeof(int));
                    DIE(rc < 0, "setsockopt");

                    FD_SET(newsockfd, &read_fds);
                    fdmax = max(newsockfd, fdmax);
                }
                else
                {
                    rc = recv(i, buffer, BUFLEN, 0);
                    DIE(rc < 0, "recv client tcp");
                    tcpmessage tcpmesaj;
                    memset(&tcpmesaj, 0, sizeof(tcpmessage));
                    memcpy(&tcpmesaj.code, buffer, 4);
                    memcpy(&tcpmesaj.topic, &buffer[4], 50);
                    if (rc == 0)
                    {
                        for(int j = 0; j <= MAX_CLIENTS; j++)
                        {
                            if(clients[j].socket == i)
                            {
                                fprintf(stdout, "Client %s disconnected.\n", clients[j].id.c_str());
                                clients[j].connect = false;
                                clients[j].id = "";
                                clients[j].socket = -1;
                                close(i);
                                FD_CLR(i, &read_fds);
                            }
                        }
                    }
                    else
                    {   
                        char stringid[50];
                        memset(stringid, 0, 50);
                        strcpy(stringid, tcpmesaj.topic);
                        if (tcpmesaj.code == 0)
                        {
                            for(int j = 0; j < MAX_CLIENTS; j++)
                            {
                                if(strcmp(clients[j].id.c_str(), stringid) == 0 && clients[j].connect == true)
                                {
                                    fprintf(stdout, "Client %s already connected.\n", tcpmesaj.topic);
                                    close(i);
                                    FD_CLR(i, &read_fds);
                                    break;
                                }
                                if(clients[j].connect == false)
                                {
                                    clients[j].connect = true;
                                    clients[j].id = stringid;
                                    clients[j].socket = i;
                                    fprintf(stdout, "New client %s connected from %s:%d.\n", tcpmesaj.topic, inet_ntoa(cli_addr.sin_addr), ntohs(cli_addr.sin_port));
                                    break;
                                }
                            }
                        }
                        else if (tcpmesaj.code == 1)
                        {
                            handleSubscribe(i, tcpmesaj, topicuri, clients);
                        }
                        else if (tcpmesaj.code == 2)
                        {
                            handleUnsubscribe(i, tcpmesaj, topicuri, clients);
                        }
                    }
                }
            }
        }
    }
    close(tcpsocket);
    close(udpsocket);
    return 0;
}
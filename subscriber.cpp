#include "helpers.h"

using namespace std;

int main(int argc, char *argv[])
{
    setvbuf(stdout, NULL, _IONBF, BUFSIZ);

    int sockfd, rc;
    struct sockaddr_in serv_addr;
    char buffer[BUFLEN];
    fd_set read_fds, tmp_fds;
    FD_ZERO(&read_fds);
    FD_SET(0, &read_fds);
    FD_ZERO(&tmp_fds);

    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    DIE(sockfd < 0, "socket");

    int flag = 1;
    rc = setsockopt(sockfd, IPPROTO_TCP, TCP_NODELAY, (char *)&flag, sizeof(int));
    DIE(rc < 0, "setsockopt");

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(atoi(argv[3]));
    rc = inet_aton(argv[2], &serv_addr.sin_addr);
    DIE(rc == 0, "invalid ip");

    rc = connect(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr));
    DIE(rc < 0, "connect");

    tcpmessage mesaj;
    mesaj.code = 0;
    strcpy(mesaj.topic, argv[1]);
    rc = send(sockfd, &mesaj, sizeof(tcpmessage), 0);
    DIE(rc < 0, "send");

    FD_SET(sockfd, &read_fds);

    while (1)
    {
        fd_set tmp = read_fds;

        int rc = select(sockfd + 1, &tmp, NULL, NULL, NULL);
        DIE(rc < 0, "Error select\n");

        if (FD_ISSET(0, &tmp))
        {
            memset(buffer, 0, BUFLEN);
            fgets(buffer, BUFLEN, stdin);
            vector<string> cuvinte;
            istringstream iss(buffer);
            string cuvant;

            while (iss >> cuvant)
            {
                cuvinte.push_back(cuvant);
            }

            if (cuvinte[0].compare("subscribe") == 0)
            {
                mesaj.code = 1;
                strcpy(mesaj.topic, cuvinte[1].c_str());
                rc = send(sockfd, &mesaj, sizeof(tcpmessage), 0);
                DIE(rc < 0, "Error send subscribe");
                printf("Subscribed to topic %s\n", mesaj.topic);
            }
            else if (cuvinte[0].compare("unsubscribe") == 0)
            {
                mesaj.code = 2;
                strcpy(mesaj.topic, cuvinte[1].c_str());
                rc = send(sockfd, &mesaj, sizeof(tcpmessage), 0);
                DIE(rc < 0, "Error send unsubscribe");
                printf("Unsubscribed from topic %s\n", mesaj.topic);
            }
            else if (cuvinte[0].compare("exit") == 0)
            {
                break;
            }
            else
                perror("invalid command\n");
        }

        if (FD_ISSET(sockfd, &tmp))
        {
            udpmessage udpmesaj;
            memset(&udpmesaj, 0, sizeof(udpmessage));
            rc = recv(sockfd, &udpmesaj, sizeof(udpmessage), 0);
            if (rc == 0)
                break;
            DIE(rc < 0, "Error receive\n");

            if (udpmesaj.type == 0)
            {
                uint32_t rez = ntohl(*((uint32_t *)(udpmesaj.payload + 1)));
                if (udpmesaj.payload[0] == 1)
                    rez *= -1;
                printf("%s - INT - %d\n", udpmesaj.topic, rez);
            }
            else if (udpmesaj.type == 1)
            {
                double rez = ntohs(*(uint16_t *)udpmesaj.payload);
                rez /= 100;
                printf("%s - SHORT_REAL - %.2f\n", udpmesaj.topic, rez);
            }
            else if (udpmesaj.type == 2)
            {
                double rez = ntohl((*(uint32_t *)(udpmesaj.payload + 1)));
                double ptr = 1;
                ptr = pow(10, (uint8_t)udpmesaj.payload[5]);
                rez /= ptr;
                if (udpmesaj.payload[0] == 1)
                    rez = -rez;
                printf("%s - FLOAT - %lf\n", udpmesaj.topic, rez);
            }
            else if (udpmesaj.type == 3)
                printf("%s - STRING - %s\n", udpmesaj.topic, udpmesaj.payload);
        }
    }

    close(sockfd);
    return 0;
}
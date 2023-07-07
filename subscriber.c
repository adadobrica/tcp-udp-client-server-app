#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/poll.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <ctype.h>
#include <errno.h>
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <signal.h>
#include "utils.h"

/* function that parses the message sent from the server */

void parse_subscribed_topic(Message message) {
    if (message.type == 0) {
        /* means we have an INT */

        int sign = message.content[0];

        char tmp_topic[MAX_TOPIC_LEN];
        memset(tmp_topic, 0, MAX_TOPIC_LEN);
        strncpy(tmp_topic, message.topic, MAX_TOPIC_LEN);

        uint32_t int_value = htonl(*(uint32_t*)(message.content + 1));

        if (sign == 0) {
            printf("%s - INT - %d\n", message.topic, int_value);
        } else {
            printf("%s - INT - -%d\n", tmp_topic, int_value);
        }

    } else if (message.type == 1) {
        /* means we have SHORT REAL */

        uint16_t short_real_value = htons(*(uint16_t*)message.content);

        printf("%s - SHORT_REAL - %d.%02d\n", message.topic, short_real_value / 100, short_real_value % 100);


    } else if (message.type == 2) {

        /* means we have FLOAT */

        uint8_t sign = message.content[0];

        uint32_t module = htonl(*(uint32_t*)(message.content + 1));

        uint8_t power = message.content[5];

        double power_of_ten = 1;

        for (int i = 0; i < power; ++i) {
            power_of_ten *= 10;
        }

        uint32_t decimal_part = module % (uint32_t)power_of_ten;

        uint32_t integer_part = module / (uint32_t)power_of_ten;

        if (sign == 0) {
            printf("%s - FLOAT - %d.%0*d\n", message.topic, integer_part, power, decimal_part);
        } else {
            printf("%s - FLOAT - -%d.%0*d\n", message.topic, integer_part, power, decimal_part);
        }

    } else if (message.type == 3) {
        /* we have a string */

        char temp_buff_topic[MAX_TOPIC_LEN + 1];
        memset(temp_buff_topic, 0, MAX_TOPIC_LEN + 1);
        strncpy(temp_buff_topic, message.topic, MAX_TOPIC_LEN + 1);

        char temp_buff_content[MAX_CONTENT_LEN + 1];
        memset(temp_buff_content, 0, MAX_CONTENT_LEN + 1);
        strncpy(temp_buff_content, message.content, MAX_CONTENT_LEN + 1);

        printf("%s - STRING - %s\n", temp_buff_topic, temp_buff_content);

    }
}

void run_subscriber(int sockfd, char* id_client, char* ip_server, uint16_t port) {
    struct pollfd pfds[MAX_CONNECTIONS];
    int nfds = 0;

    char buf[MAX_CONTENT_LEN + MAX_TOPIC_LEN + 2];

    pfds[nfds].fd = STDIN_FILENO;
    pfds[nfds].events = POLLIN;
    nfds++;

    pfds[nfds].fd = sockfd;
    pfds[nfds].events = POLLIN;
    nfds++;

    while (1) {
        poll(pfds, nfds, -1);

        if ((pfds[1].revents & POLLIN) != 0) {
            /* receiving messages from the server */

            Message message;
            memset(&message, 0, sizeof(message));
            memset(message.content, 0, sizeof(message.content));
            memset(message.topic, 0, sizeof(message.topic));

           int rc = recv_all(sockfd, &message, sizeof(Message));

            if (rc < 0) {
                error_handler("read failed");
            }

            if (rc == 0) {
                /* if the number of bytes received is 0,
                then the server has closed its connection */

                close(sockfd);
                exit(0);
            } else {
                parse_subscribed_topic(message);
            }
        }
        else if ((pfds[0].revents & POLLIN) != 0) {
            /* reading from STDIN and parsing the buffer */

            memset(buf, 0, sizeof(buf));

            fgets(buf, sizeof(buf), stdin);

            if (strcmp(buf, "exit\n") == 0) {
                close(sockfd);
                exit(0);

           } else if (strncmp(buf, "subscribe", 9) == 0) {
                /* creating our packet and sending it to the server */

                Packet sent_packet;
                memset(sent_packet.payload, 0, sizeof(sent_packet.payload));
                sent_packet.len = strlen(buf);
                strcpy(sent_packet.payload, buf);
                sent_packet.payload[sent_packet.len - 1] = '\0';

                int rc = send_all(sockfd, &sent_packet, sizeof(sent_packet));

                if (rc < 0) {
                    error_handler("send all failed");
                }

                printf("Subscribed to topic.\n");

            } else if (strncmp(buf, "unsubscribe", 11) == 0) {
                Packet sent_packet;
                memset(sent_packet.payload, 0, sizeof(sent_packet.payload));
                sent_packet.len = strlen(buf);
                strcpy(sent_packet.payload, buf);
                sent_packet.payload[sent_packet.len - 1] = '\0';

                int rc = send_all(sockfd, &sent_packet, sizeof(sent_packet));

                if (rc < 0) {
                    error_handler("send all failed");
                }

                printf("Unsubscribed from topic.\n");
            } else {
                fprintf(stderr, "Invalid command\n");
            }
        }
    }
}

int main(int argc, char *argv[]) {
    sigaction(SIGPIPE, &(struct sigaction){ .sa_handler = SIG_IGN }, NULL);

    setvbuf(stdout, NULL, _IONBF, BUFSIZ);
    int sockfd;
    if (argc < 4) {
        fprintf(stderr, "Usage: %s <ID_Client> <IP_Server> <Port_Server>\n", argv[0]);
        exit(0);
    }

    char *id_client = argv[1];
    char *ip_server = argv[2];
    uint16_t port;
    int rc = sscanf(argv[3], "%hu", &port);

    if (rc != 1) {
        error_handler("invalid port");
    }

    /* opening a TCP client socket */

    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        error_handler("could not open TCP socket");
    }

    /* setting the server address */

    struct sockaddr_in serv_addr;
    socklen_t socket_len = sizeof(struct sockaddr_in);

    int enable = 1;
    if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int)) < 0) {
        error_handler("setsockopt(SO_REUSEADDR) failed");
    }


    if (setsockopt(sockfd, IPPROTO_TCP, TCP_NODELAY, &enable, sizeof(int)) < 0) {
        error_handler("setsockopt(TCP_NODELAY) failed");
    }

    memset(&serv_addr, 0, sizeof(struct sockaddr_in));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(port);
    rc = inet_pton(AF_INET, ip_server, &serv_addr.sin_addr);

    if (rc < 0) {
        error_handler("could not convert ip address");
    }

    /* connecting to the server */

    rc = connect(sockfd, (struct sockaddr *)&serv_addr, socket_len);

    if (rc < 0) {
        error_handler("could not connect to server");
    }

    /* sending the ID client to the server */

    Packet packet_id;
    memset(&packet_id, 0, sizeof(packet_id));
    memset(packet_id.payload, 0, sizeof(packet_id.payload));
    strncpy(packet_id.payload, id_client, strlen(id_client));
    packet_id.len = strlen(id_client) + 1;


    rc = send_all(sockfd, &packet_id, sizeof(packet_id));

    if (rc < 0) {
        error_handler("send failed");
    }

    run_subscriber(sockfd, id_client, ip_server, port);

    close(sockfd);

    return 0;
}
#include <arpa/inet.h>
#include <ctype.h>
#include <errno.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/poll.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <signal.h>
#include <netinet/tcp.h>
#include "utils.h"

/* creating an array of TCP clients */

client clients[MAX_CONNECTIONS];
int clients_size = 0;

/* function that stores messages into an array to be sent later when the client
has connected to the server again */

void store_udp_messages(Message newmessage, MessagesUDP *udp) {
    for (int k = 0; k < clients_size; ++k) {
        if (clients[k].is_connected == 0) {
            for (int l = 0; l < clients[k].sf_size; ++l) {
                if (strncmp(clients[k].sf_topics[l], newmessage.topic, MAX_TOPIC_LEN) == 0) {
                    udp->messages[udp->size] = newmessage;
                    udp->size++;
                }
            }
        } 
    }
}

/* function that sends topics to the connected clients */

void send_topics_without_sf(Message newmessage) {
    for (int k = 0; k < clients_size; ++k) {
        for (int l = 0; l < clients[k].num_subscribed_topics; l++) {
            if (strncmp(clients[k].subscribed_topics[l], newmessage.topic, MAX_TOPIC_LEN) == 0
                && clients[k].is_connected == 1) {
                int num_bytes = send_all(clients[k].client_fd, &newmessage, sizeof(newmessage));
 
                if (num_bytes < 0) {
                    error_handler("send_all");
                }
                break;
            }
        }
    }
}

/* if the client has just connected again to the server and has missed some topics
they have subscribed on, then the server sends them the topics */

void send_topics_with_sf(char new_client_id[MAX_ID_SIZE], MessagesUDP *udp, int i, 
                            struct pollfd poll_fds[MAX_CONNECTIONS]) {
    for (int j = 0; j < clients_size; ++j) {
        if (strcmp(clients[j].client_id, new_client_id) == 0) {
            for (int k = 0; k < udp->size; ++k) {
                for (int l = 0; l < clients[j].sf_size; ++l) {
                    if (strncmp(udp->messages[k].topic, clients[j].sf_topics[l],
                                MAX_TOPIC_LEN) == 0) {

                        int rc = send_all(poll_fds[i].fd, &udp->messages[l], sizeof(Message));

                        if (rc < 0) {
                            error_handler("send");
                        }

                        break;
                    }
                }
            }
        }
    }
}

/* function used when a client unsubscribes from a topic; instead of removing it, 
    i just zerorize the topic */

void unsubscribe_from_topic(struct pollfd poll_fds[MAX_CONNECTIONS], int i, char topic[MAX_TOPIC_LEN]) {
    for (int j = 0; j < clients_size; ++j) {
        if (clients[j].client_fd == poll_fds[i].fd) {
            for (int k = 0; k < clients[j].num_subscribed_topics; ++k) {
                if (strncmp(clients[j].subscribed_topics[k], topic, MAX_TOPIC_LEN) == 0) {

                    memset(clients[j].subscribed_topics[k], 0,
                            strlen(clients[j].subscribed_topics[k]));
                    break;
                }
            }
        }
    }
}

void run_server(int udpfd, MessagesUDP *udp, int tcpfd) {
    struct pollfd poll_fds[MAX_CONNECTIONS];
    memset(poll_fds, 0, sizeof(poll_fds));
    int num_clients = 3;
    int rc;

    rc = listen(tcpfd, MAX_CONNECTIONS);
    if (rc < 0) {   
        error_handler("listen");
    }

    poll_fds[0].fd = tcpfd;
    poll_fds[0].events = POLLIN;

    poll_fds[1].fd = udpfd;
    poll_fds[1].events = POLLIN;

    poll_fds[2].fd = STDIN_FILENO;
    poll_fds[2].events = POLLIN;

    int new_connect = 0;

    struct sockaddr_in cli_addr_tcp;
    socklen_t cli_len_tcp = sizeof(cli_addr_tcp);

    while (1) {
        rc = poll(poll_fds, num_clients, -1);
        if (rc < 0) {
            error_handler("poll");
        }

        for (int i = 0; i < num_clients; i++) {
            if (poll_fds[i].revents & POLLIN) {
                if (poll_fds[i].fd == tcpfd) {
                    /* the server accepts the new connection from the TCP client */

                    int newsockfd = accept(tcpfd, (struct sockaddr *)&cli_addr_tcp, &cli_len_tcp);

                    if (newsockfd < 0) {
                        error_handler("accept");
                    }

                    new_connect = 1;

                    /* adding the new client file descriptor to our array of poll fds */

                    poll_fds[num_clients].fd = newsockfd;
                    poll_fds[num_clients].events = POLLIN;
                    num_clients++;

                } else if (poll_fds[i].fd == STDIN_FILENO) {
                    /* if we have a command sent from stdin, then we check if it's "exit", 
                        otherwise we print to stderr "Invalid command */

                    char stdin_buffer[MAX_LINE];
                    memset(stdin_buffer, 0, MAX_LINE);
                    fgets(stdin_buffer, MAX_LINE, stdin);

                    if (strcmp(stdin_buffer, "exit\n") == 0) {
                        exit_command(tcpfd, udpfd, poll_fds, i, num_clients);
                    } else {
                        fprintf(stderr, "Invalid command\n");
                    } 
                } else if (poll_fds[i].fd == udpfd) {
                    char buff[MAX_TOPIC_LEN + MAX_CONTENT_LEN + 2];
                    struct sockaddr_in cli_addr;
                    socklen_t cli_len = sizeof(cli_addr);
                    memset(buff, 0, sizeof(buff));

                    int num_bytes = recvfrom(udpfd, buff, sizeof(buff), 0, (struct sockaddr *)&cli_addr, &cli_len);

                    if (num_bytes < 0) {
                        error_handler("recvfrom");
                    }
                    /* saving the message received from the UDP client */

                    Message newmessage;
                    memset(&newmessage, 0, sizeof(Message));
                    memset(newmessage.content, 0, sizeof(newmessage.content));
                    memset(newmessage.topic, 0, sizeof(newmessage.topic));

                    strncpy(newmessage.topic, buff, MAX_TOPIC_LEN);
                    memcpy(&newmessage.type, buff + MAX_TOPIC_LEN, sizeof(uint8_t));
                    memcpy(newmessage.content, buff + MAX_TOPIC_LEN + sizeof(uint8_t), MAX_CONTENT_LEN);

                    newmessage.content[strlen(newmessage.content)] = '\0';
                    newmessage.topic[strlen(newmessage.topic)] = '\0';

                    store_udp_messages(newmessage, udp);
                    send_topics_without_sf(newmessage);
                } else {
                    /* this means we have received a message from one of our TCP clients */
                    char buffer[MAX_LINE];
                    memset(buffer, 0, MAX_LINE);

                    Packet received_packet;
                    memset(&received_packet, 0, sizeof(Packet));
                    memset(received_packet.payload, 0, sizeof(received_packet.payload));

                    int num_bytes = recv_all(poll_fds[i].fd, &received_packet, sizeof(received_packet));

                    if (num_bytes < 0) {
                        perror("recv\n");
                    }

                    char new_client_id[MAX_ID_SIZE];
                    memset(new_client_id, 0, MAX_ID_SIZE);
                    strncpy(new_client_id, received_packet.payload, received_packet.len);

                    /* if the client has just connected, then it means we received their ID */

                    if (new_connect == 1) {
                        new_connect = 0;
                        /* checking if the client is already connected */

                        int get_id = get_client_id(clients, new_client_id, clients_size);

                        if (get_id == 1) {
                            printf("Client %s already connected.\n", new_client_id);

                            for (int j = i; j < num_clients - 1; j++) {
                                poll_fds[j] = poll_fds[j + 1];
                            }
                            num_clients--;

                            close(poll_fds[i].fd);
                            continue;
                        } else {
                            /* if we have a new client, then we add them to our array of clients */

                            add_client(clients, new_client_id, &clients_size, poll_fds[i].fd);

                            for (int j = 0; j < clients_size; ++j) {
                                if (strcmp(new_client_id, clients[j].client_id) == 0) {
                                    clients[j].is_connected = 1;
                                }
                            }

                            printf("New client %s connected from %s:%d.\n", new_client_id,
                                inet_ntoa(cli_addr_tcp.sin_addr), ntohs(cli_addr_tcp.sin_port));

                            send_topics_with_sf(new_client_id, udp, i, poll_fds);
                        }
                    } else {
                        /* if the client already sent their ID, then we have two possible messages: 
                         either subscribe or unsubscribe */

                        if (num_bytes == 0) {
                            /* the client disconnected */

                            char disconnected_id[MAX_ID_SIZE];
                            memset(disconnected_id, 0, MAX_ID_SIZE);

                            for (int j = 0; j < clients_size; ++j) {
                                if (clients[j].client_fd == poll_fds[i].fd) {
                                    printf("Client %s disconnected.\n", clients[j].client_id);
                                    clients[j].is_connected = 0;
                                    strcpy(disconnected_id, clients[j].client_id);
                                    break;
                                }
                            }

                            /* closing the file descriptor associated with the client, 
                                and removing the client from the array */

                            close(poll_fds[i].fd);

                            for (int j = i; j < num_clients - 1; j++) {
                                poll_fds[j] = poll_fds[j + 1];
                            }
                            num_clients--;

                            remove_client(clients, disconnected_id, &clients_size);

                        } else {
                            /* the client wants to subscribe/unsubscribe */
                            /* disassembling the message to see if the client wants
                                to subscribe or unsubscribe */

                            char topic[MAX_TOPIC_LEN];
                            memset(topic, 0, MAX_TOPIC_LEN);

                            char *token = strtok(received_packet.payload, " ");

                            if (strcmp(token, "subscribe") == 0) {
                                /* getting the  topic */
                                token = strtok(NULL, " ");
                                memcpy(topic, token, strlen(token));

                                topic[strlen(token)] = '\0';

                                /* getting the SF */

                                token = strtok(NULL, " ");

                                int sf = atoi(token);

                                /* firstly, we add the topic to the array of subscribed topics;
                                    if the topic has SF = 1, then we also add it separately to
                                    another array of topics */

                                for (int j = 0; j < clients_size; ++j) {
                                    if (clients[j].client_fd == poll_fds[i].fd) {

                                        strncpy(clients[j].subscribed_topics[clients[j].num_subscribed_topics],
                                            topic, MAX_TOPIC_LEN);
                                        clients[j].num_subscribed_topics++;

                                        if (sf == 1) {
                                            strncpy(clients[j].sf_topics[clients[j].sf_size], topic, MAX_TOPIC_LEN);
                                            clients[j].sf_size++;
                                        }

                                        break;
                                    }
                                }
                            } else if (strcmp(token, "unsubscribe") == 0) {
                                /* the client wants to unsubscribe from topic */

                                token = strtok(NULL, " ");
                                memcpy(topic, token, strlen(token));

                                topic[strlen(token)] = '\0';

                                unsubscribe_from_topic(poll_fds, i, topic);
                            } 
                        } 
                    }
                }
            } 
        }
    }
}
 
int main(int argc, char *argv[]) {
    sigaction(SIGPIPE, &(struct sigaction){ .sa_handler = SIG_IGN }, NULL);

    setvbuf(stdout, NULL, _IONBF, BUFSIZ);
    char buffer[MAX_LINE];
    memset(buffer, 0, MAX_LINE);

    if (argc != 2) {
        fprintf(stderr, "\n Usage: %s <port>\n", argv[0]);
        return 1;
    }

    /* parsing the port */

    uint16_t port;
    int rc = sscanf(argv[1], "%hu", &port);

    if (rc != 1) {
        error_handler("Given port is invalid");
    }

    /* creating the socket for TCP connection */

    int listenfd = socket(AF_INET, SOCK_STREAM, 0);

    if (listenfd < 0) {
        error_handler("Could not create TCP socket");
    }

    struct sockaddr_in serv_addr;
    socklen_t socket_len = sizeof(struct sockaddr_in);

    /* making the socket reusable */

    int enable = 1;
    if (setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int)) < 0) {
        error_handler("setsockopt(SO_REUSEADDR) failed");
    }

    if (setsockopt(listenfd, IPPROTO_TCP, TCP_NODELAY, &enable, sizeof(int)) < 0) {
        error_handler("setsockopt(TCP_NODELAY) failed");
    }

    memset(&serv_addr, 0, socket_len);
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(port);

    /* binding the server address to the socket */

    rc = bind(listenfd, (const struct sockaddr *)&serv_addr, sizeof(serv_addr));

    if (rc < 0) {
        error_handler("bind TCP");
    }

    /* opening an UDP socket */

    int udpfd = socket(AF_INET, SOCK_DGRAM, 0);

    if (udpfd < 0) {
        error_handler("Could not create UDP socket");
    }

    /* making the socket reusable */

    if (setsockopt(udpfd, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int)) < 0) {
        error_handler("setsockopt(SO_REUSEADDR) failed");
    }

    /* binding the server address to the socket */

    rc = bind(udpfd, (const struct sockaddr *)&serv_addr, sizeof(serv_addr));

    if (rc < 0) {
        error_handler("bind UDP");
    }

    MessagesUDP *udp = malloc(sizeof(MessagesUDP));
    udp->size = 0;

    run_server(udpfd, udp, listenfd);

    close(listenfd);
    close(udpfd);
    free(udp);
    
    return 0;

}
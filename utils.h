#ifndef __UTILS_H__
#define __UTILS_H__

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stddef.h>
#include <sys/socket.h>
#include <sys/types.h>

#define MAX_LINE 1024
#define MAX_TOPIC_LEN 50
#define MAX_CONTENT_LEN 1500
#define MAX_CONNECTIONS 500
#define MAX_ID_SIZE 10

/* structure that contains packets sent to/from TCP clients when they want to subscribe/unsubscribe */

typedef struct Packet {
    char payload[100];
    int len;
} Packet;

/* structure that contains messages sent from UDP clients */

typedef struct __attribute__((packed)) Message {
    char topic[MAX_TOPIC_LEN + 1];
    uint8_t type;
    char content[MAX_CONTENT_LEN + 1];
} Message;

/* structure that contains an array of messages from UDP clients */

typedef struct MessagesUDP {
    Message messages[MAX_CONNECTIONS];
    int size;
} MessagesUDP;

/* structure for the TCP clients, helps store the client ID, their subscribed topics and additional info */

typedef struct client {
    char client_id[10];
    char subscribed_topics[35][MAX_TOPIC_LEN];
    int num_subscribed_topics;
    int client_fd;
    int is_connected;
    char sf_topics[35][MAX_TOPIC_LEN];
    int sf_size;
} client;

/* handler for errors */

void error_handler(char* msg);

/* function that sends exactly len bytes */

int send_all(int sockfd, void *buf, size_t len);

/* function that receives exactly len bytes */

int recv_all(int sockfd, void *buf, size_t len);

/* function that returns client's ID from the array of clients; returns -1 if it doesn't exist */

int get_client_id(client clients[MAX_CONNECTIONS], char id[MAX_ID_SIZE], int clients_size);

/* function that removes a client's id from the array when they disconnect */

void remove_client(client *clients, char id[MAX_ID_SIZE], int *clients_size);

/* function that adds a client's id to the array when they connect */

void add_client(client *clients, char id[MAX_ID_SIZE], int *clients_size, int sockfd);

/* function used by server when reading the "exit" command from STDIN */

void exit_command(int tcpfd, int udpfd, struct pollfd poll_fds[MAX_CONNECTIONS], int i,
                    int num_clients);

#endif
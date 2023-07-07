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


void error_handler(char *msg) {
    fprintf(stderr, "%s\n", msg);
    exit(1);
}

int get_client_id(client clients[MAX_CONNECTIONS], char id[MAX_ID_SIZE], int clients_size) {

    /* this is for the special case where we have a client that subscribed to topics with
        SF = 1, since in this case the client does not get removed from the array of clients */

    for (int i = 0; i < clients_size; ++i) {
        if (strcmp(clients[i].client_id, id) == 0) {
            if (clients[i].is_connected == 0) {
                return 0;
            }
        }
    }

    /* if the client does not have any topics with SF = 1, then we can safely check the 
    array to see if there's another client with the same id */

    for (int i = 0; i < clients_size; ++i) {
        if (strcmp(clients[i].client_id, id) == 0) {
            return 1;
        }
    }
    return 0;
}

void remove_client(client *clients, char id[MAX_ID_SIZE], int *clients_size) {
    /* we do not remove the client that has subscribed to topics with SF = 1, 
    since we need to remember their ID and info */

    for (int i = 0; i < *clients_size; ++i) {
        if (strcmp(clients[i].client_id, id) == 0) {
            if (clients[i].sf_size > 0) {
                return;
            }
        }
    }

    /* otherwise, the client gets removed when they disconnect */

    for (int i = 0; i < *clients_size; ++i) {
        if (strcmp(clients[i].client_id, id) == 0) {
            for (int j = i; j < (*clients_size) - 1; ++j) {
                clients[j] = clients[j + 1];
            }
            (*clients_size)--;
            break;
        }
    }
}

void add_client(client *clients, char id[MAX_ID_SIZE], int *clients_size, int sockfd) {
    strcpy(clients[*clients_size].client_id, id);
    clients[*clients_size].client_fd = sockfd;
    (*clients_size)++;
}

int recv_all(int sockfd, void *buffer, size_t len) {
    size_t bytes_received = 0;
    size_t bytes_remaining = len;
    char *buff = buffer;

    size_t num_bytes = 0;

    num_bytes = recv(sockfd, buff + bytes_received, len, 0);

    if (num_bytes < 0) {
        return -1;
    }

    if (num_bytes == 0) {
        return 0;
    }

    bytes_received += num_bytes;
    bytes_remaining -= num_bytes;

    while (bytes_remaining) {
        num_bytes = recv(sockfd, buff + bytes_received, len, 0);

        if (num_bytes < 0) {
            break;
        }

        bytes_received += num_bytes;
        bytes_remaining -= num_bytes;

        if (bytes_remaining == 0) {
            break;
        }

    }

    return recv(sockfd, buffer, len, 0);

}

int send_all(int sockfd, void *buffer, size_t len) {
    size_t bytes_sent = 0;
    size_t bytes_remaining = len;
    char *buff = buffer;
    size_t num_bytes = 0;
  
    while (bytes_remaining) {
        num_bytes = send(sockfd, buff + bytes_sent, len, 0);

        if (num_bytes < 0) {
            break;
        }

        bytes_remaining -= num_bytes;

        bytes_sent += num_bytes;
    }

    return send(sockfd, buffer, len, 0);
}

void exit_command(int tcpfd, int udpfd, struct pollfd poll_fds[MAX_CONNECTIONS], int i,
                    int num_clients) {
    for (int j = 0; j < num_clients; j++) {
        if (poll_fds[j].fd != tcpfd && poll_fds[j].fd != udpfd
            && poll_fds[j].fd != STDIN_FILENO && i != j) {
            close(poll_fds[j].fd);
        }
    }
    close(tcpfd);
    close(udpfd);
    exit(0);
}
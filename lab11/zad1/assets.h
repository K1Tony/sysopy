#ifndef ASSETS_H
#define ASSETS_H

#define MAX_BACKLOG 5
#define MAX_CLIENT_NAME_SIZE 256

#define MAX_MSG_LENGTH 1024

#include "sys/socket.h"
#include "sys/types.h"
#include "netinet/in.h"
#include "arpa/inet.h"

typedef struct message {
    char msg[MAX_MSG_LENGTH];
    int sockfd;
} message_t;

typedef struct client {
    int sockfd;
    char name[MAX_CLIENT_NAME_SIZE];
    struct sockaddr_in addr;
} client_t;

typedef struct server {
    int sockfd;
    struct sockaddr_in addr;
} server_t;

#endif // ASSETS_H
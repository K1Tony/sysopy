#ifndef ASSETS_H
#define ASSETS_H

#define MAX_BACKLOG 5
#define MAX_CLIENT_NAME_SIZE 256

#define MAX_MSG_LENGTH 4096

#define MAX_MSG_TYPE_LENGTH 5

#include "sys/socket.h"
#include "sys/types.h"
#include "netinet/in.h"
#include "arpa/inet.h"

#include "pthread.h"

typedef struct message {
    char type[MAX_MSG_TYPE_LENGTH];
    char author[MAX_CLIENT_NAME_SIZE];
    char msg[MAX_MSG_LENGTH];
    int dest;
    int src;
} message_t;

typedef struct client {
    int sockfd;
    int client_id;
    char name[MAX_CLIENT_NAME_SIZE];
    struct sockaddr_in *addr;
    pthread_mutex_t mutex;
} client_t;

typedef struct server {
    int sockfd;
    struct sockaddr_in *addr;
} server_t;

#endif // ASSETS_H
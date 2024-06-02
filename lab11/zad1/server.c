#define _DEFAULT_SOURCE
#define _XOPEN_SOURCE_EXTENDED 700

#include "sys/socket.h"
#include "sys/types.h"
#include "netinet/in.h"
#include "arpa/inet.h"

#include "stdio.h"
#include "stdlib.h"
#include "memory.h"

#include "signal.h"
#include "unistd.h"

#include "pthread.h"

// my include
#include "assets.h"

volatile int run = 1;

int server_sockfd;
int client_sockfd[MAX_BACKLOG];

socklen_t addr_size = sizeof(struct sockaddr_in);

int free_socket;

client_t clients[MAX_BACKLOG];
message_t client_messages[MAX_BACKLOG];

int client_count = 0;

char welcome[MAX_MSG_LENGTH] = {0}, goodbye[MAX_MSG_LENGTH] = {0}, all_clients[(MAX_CLIENT_NAME_SIZE + 20) * MAX_BACKLOG];

// threading
pthread_t acceptor_thread;
pthread_t receiver_threads[MAX_BACKLOG];

void handle(int signum) {
    for (size_t i = 0; i < MAX_BACKLOG; i++) {
        shutdown(clients[i].sockfd, SHUT_RDWR);
        close(clients[i].sockfd);
    }
    close(server_sockfd);
    signum = 0;
    run = signum;
}

void *acceptor_thread_fun(void *arg) {
    server_t *serv = (server_t *) arg;
    while (run) {
        int new_socket = accept(serv->sockfd, (struct sockaddr *) serv->addr, &addr_size);
        if (client_count == MAX_BACKLOG) {
            send(new_socket, "Sorry, chat is full :<", MAX_MSG_LENGTH - 1, 0);
            continue;
        }
        for (int i = 0; i < MAX_BACKLOG; i++) {
            if (clients[i].sockfd == -1) {
                free_socket = i;
                break;
            }
        }
        client_count++;
        clients[free_socket].sockfd = new_socket;
        clients[free_socket].client_id = free_socket;
        recv(new_socket, clients[free_socket].name, MAX_CLIENT_NAME_SIZE - 1, 0);

        sprintf(welcome, "Welcome to the chat %s!", clients[free_socket].name);
        send(new_socket, welcome, MAX_MSG_LENGTH - 1, 0);
        memset(welcome, 0, sizeof(welcome));

        sprintf(welcome, "%s has entered the chat!", clients[free_socket].name);
        for (int i = 0; i < MAX_BACKLOG; i++) {
            if (i != free_socket) {
                send(clients[i].sockfd, welcome, MAX_MSG_LENGTH - 1, 0);
            }
        }
    }
    return NULL;
}

void *receiver_thread_fun(void *arg) {
    client_t *cli = (client_t *) arg;
    while (run) {
        if (cli->sockfd == -1) continue;
        recv(cli->sockfd, &client_messages[cli->client_id], MAX_MSG_LENGTH, 0);
        printf("\n!! --- Message received! --- !!\n%d\n", cli->client_id);
        if (strcmp(client_messages[cli->client_id].type, "LIST") == 0) {
            int msg_idx = 0;
            for (int i = 0; i < MAX_BACKLOG; i++) {
                if (&clients[i] == cli || clients[i].client_id == -1) {
                    continue;
                }
                sprintf(all_clients + msg_idx, "%s sockfd: %d\n", clients[i].name, clients[i].sockfd);
                msg_idx = strlen(all_clients);
            }
            send(cli->sockfd, all_clients, MAX_CLIENT_NAME_SIZE - 1, 0);
            memset(all_clients, 0, MAX_MSG_LENGTH);
        } else if (strcmp(client_messages[cli->client_id].type, "ALL") == 0) {
            for (int i = 0; i < MAX_BACKLOG; i++) {
                if (&clients[i] == cli || clients[i].client_id == -1) {
                    continue;
                }
                send(clients[i].sockfd, client_messages[cli->client_id].msg, MAX_MSG_LENGTH - 1, 0);
            }
        } else if (strcmp(client_messages[cli->client_id].type, "ONE") == 0) {
            send(client_messages[cli->client_id].dest, client_messages[cli->client_id].msg, MAX_MSG_LENGTH - 1, 0);
        } else if (strcmp(client_messages[cli->client_id].type, "STOP") == 0) {
            send(cli->sockfd, "TERMINATE", MAX_MSG_LENGTH - 1, 0);
            sprintf(goodbye, "%s has left the chat!", cli->name);
            cli->client_id = -1;
            cli->sockfd = -1;
            memset(cli->name, 0, MAX_CLIENT_NAME_SIZE);
            cli->addr = NULL;
            client_count--;
            for (int i = 0; i < MAX_BACKLOG; i++) {
                send(clients[i].sockfd, goodbye, MAX_MSG_LENGTH - 1, 0);
            }
            memset(goodbye, 0, MAX_MSG_LENGTH);
        } 
    }

    return NULL;
}

int main(int argc, char **argv) {

    if (argc != 2) {
        fprintf(stderr, "Invalid argument count!\n");
        return EXIT_FAILURE;
    }

    in_port_t port_no = atoi(argv[1]);

    struct sockaddr_in sa_addr;
    sa_addr.sin_family = AF_INET;
    sa_addr.sin_port = port_no;
    sa_addr.sin_addr.s_addr = INADDR_ANY;

    server_sockfd = socket(AF_INET, SOCK_STREAM, 0);

    // struct sockaddr_in client_addrs[MAX_BACKLOG];

    server_t serv;
    serv.sockfd = server_sockfd;
    serv.addr = &sa_addr;

    for (short i = 0; i < MAX_BACKLOG; i++) {
        clients[i].client_id = -1;
        clients[i].sockfd = -1;
        memset(clients[i].name, 0, MAX_CLIENT_NAME_SIZE);
        clients[i].addr = NULL;

        memset(client_messages[i].msg, 0, MAX_MSG_LENGTH);
    }

    if (server_sockfd < 0) {
        perror("Server socket creation");
        return EXIT_FAILURE;
    }

    if (bind(server_sockfd, (struct sockaddr*) &sa_addr, addr_size) < 0) {
        perror("Server bind");
        return EXIT_FAILURE;
    }

    if (listen(server_sockfd, MAX_BACKLOG) < 0) {
        perror("Server lsiten");
        return EXIT_FAILURE;
    }

    pthread_create(&acceptor_thread, NULL, acceptor_thread_fun, &serv);
    for (int i = 0; i < MAX_BACKLOG; i++) {
        pthread_create(&receiver_threads[i], NULL, receiver_thread_fun, &clients[i]);
    }

    // Ctrl+C
    signal(SIGINT, handle);
    
    while (run) {}

    for (size_t i = 0; i < MAX_BACKLOG; i++) {
        shutdown(i, SHUT_RDWR);
        close(i);
    }

    close(server_sockfd);

    return EXIT_SUCCESS;
}
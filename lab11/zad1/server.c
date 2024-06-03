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
message_t client_messages[MAX_BACKLOG], welcome_msg, goodbye_msg, list_msg;

int client_count = 0;

char welcome[MAX_MSG_LENGTH] = {0}, goodbye[MAX_MSG_LENGTH] = {0}, all_clients[(MAX_CLIENT_NAME_SIZE + 20) * MAX_BACKLOG];

// threading
pthread_t acceptor_thread;
pthread_t receiver_threads[MAX_BACKLOG];

void handle(int signum) {
    pthread_cancel(acceptor_thread);
    for (size_t i = 0; i < MAX_BACKLOG; i++) {
        pthread_mutex_destroy(&clients[i].mutex);
        pthread_cancel(receiver_threads[i]);

        shutdown(clients[i].sockfd, SHUT_RDWR);
        close(clients[i].sockfd);
    }
    close(server_sockfd);
    signum = 0;
    run = signum;
}

void *acceptor_thread_fun(void *arg) {
    server_t *serv = (server_t *) arg;
    pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS, NULL);
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

        pthread_mutex_unlock(&clients[free_socket].mutex);
    }
    return NULL;
}

void *receiver_thread_fun(void *arg) {
    client_t *cli = (client_t *) arg;
    pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS, NULL);

    pthread_mutex_lock(&cli->mutex);
    printf("Mutex unlocked!");
    while (run) {
        if (cli->sockfd == -1) continue;
        recv(cli->sockfd, &client_messages[cli->client_id], sizeof(message_t), 0);
        printf("\n!! --- Message received! --- !!\n%d\n", client_messages[cli->client_id].dest);
        if (strcmp(client_messages[cli->client_id].type, "LIST") == 0) {
            int msg_idx = 0;
            for (int i = 0; i < MAX_BACKLOG; i++) {
                if (clients[i].sockfd == -1) {
                    continue;
                }
                sprintf(list_msg.msg + msg_idx, "%s; %d\n", clients[i].name, clients[i].sockfd);
                msg_idx = strlen(list_msg.msg);
            }
            send(cli->sockfd, &list_msg, sizeof(message_t), 0);
        } else if (strcmp(client_messages[cli->client_id].type, "ALL") == 0) {
            for (int i = 0; i < MAX_BACKLOG; i++) {
                if (&clients[i] == cli || clients[i].client_id == -1) {
                    continue;
                }
                printf("%d %s\n", clients[i].sockfd, clients[i].name);
                send(clients[i].sockfd, &client_messages[cli->client_id], sizeof(message_t), 0);
            }
        } else if (strcmp(client_messages[cli->client_id].type, "ONE") == 0) {
            printf("%s %d\n", client_messages[cli->client_id].msg, client_messages[cli->client_id].dest);

            send(client_messages[cli->client_id].dest, &client_messages[cli->client_id], sizeof(message_t), 0);
        } else if (strcmp(client_messages[cli->client_id].type, "STOP") == 0) {
            sprintf(goodbye_msg.msg, "%s has left the chat!", client_messages[cli->client_id].author);
            cli->client_id = -1;
            cli->sockfd = -1;
            memset(cli->name, 0, MAX_CLIENT_NAME_SIZE);
            cli->addr = NULL;
            client_count--;
            for (int i = 0; i < MAX_BACKLOG; i++) {
                send(clients[i].sockfd, &goodbye_msg, sizeof(message_t), 0);
            }
            memset(goodbye_msg.msg, 0, MAX_MSG_LENGTH);
        } else if (strcmp(client_messages[cli->client_id].type, "INIT") == 0) {
            strncpy(clients[cli->client_id].name, client_messages[cli->client_id].author, MAX_CLIENT_NAME_SIZE);
            sprintf(welcome_msg.msg, "Welcome to the chat %s!", client_messages[cli->client_id].author);
            send(cli->sockfd, &welcome_msg, sizeof(message_t), 0);
            sprintf(welcome_msg.msg, "%s has entered the chat!", client_messages[cli->client_id].author);
            for (int i = 0; i < MAX_BACKLOG; i++) {
                if (i == cli->client_id || clients[i].sockfd == -1) {
                    continue;
                }
                send(clients[i].sockfd, &welcome_msg, sizeof(message_t), 0);
            }
            memset(welcome, 0, MAX_MSG_LENGTH);
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

    strcpy(welcome_msg.author, "Server");
    strcpy(goodbye_msg.author, "Server");
    strcpy(list_msg.author, "Server");

    for (short i = 0; i < MAX_BACKLOG; i++) {
        clients[i].client_id = -1;
        clients[i].sockfd = -1;
        memset(clients[i].name, 0, MAX_CLIENT_NAME_SIZE);
        clients[i].addr = NULL;

        memset(client_messages[i].msg, 0, MAX_MSG_LENGTH);

        pthread_mutex_init(&clients[i].mutex, NULL);
        pthread_mutex_lock(&clients[i].mutex);
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

    // Ctrl+C
    signal(SIGINT, handle);

    pthread_create(&acceptor_thread, NULL, acceptor_thread_fun, &serv);
    for (int i = 0; i < MAX_BACKLOG; i++) {
        pthread_create(&receiver_threads[i], NULL, receiver_thread_fun, &clients[i]);
    }

    pthread_join(acceptor_thread, NULL);
    for (int i = 0; i < MAX_BACKLOG; i++) {
        pthread_join(receiver_threads[i], NULL);
    }

    return EXIT_SUCCESS;
}
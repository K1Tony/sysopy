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

int free_socket = 0;

client_t clients[MAX_BACKLOG];

int client_count = 0;

char welcome[MAX_MSG_LENGTH] = {0};

// threading
pthread_t acceptor_thread;
pthread_t receiver_threads[MAX_BACKLOG];

void handle(int signum) {
    for (size_t i = 0; i < MAX_BACKLOG; i++) {
        shutdown(clients[i].sockfd, SHUT_RDWR);
        close(clients[i].sockfd);
    }
    close(server_sockfd);
    run = 0;
}

void *acceptor_thread_fun(void *arg) {
    server_t *serv = (server_t *) arg;
    while (run) {
        int new_socket = accept(serv->sockfd, (struct sockaddr *) serv->addr, &addr_size);
        if (client_count == MAX_BACKLOG) {
            send(new_socket, "Sorry, chat is full :<", MAX_MSG_LENGTH - 1, 0);
            continue;
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

        free_socket++;
        free_socket %= MAX_BACKLOG;
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
    socklen_t client_addr_sizes[MAX_BACKLOG];

    message_t client_messages[MAX_BACKLOG];

    server_t serv;
    serv.sockfd = server_sockfd;
    serv.addr = &sa_addr;

    pthread_create(&acceptor_thread, NULL, acceptor_thread_fun, (void *) &serv);

    for (short i = 0; i < MAX_BACKLOG; i++) {
        client_sockfd[i] = -1;
        client_addr_sizes[i] = addr_size;

        client_messages[i].sockfd = server_sockfd;
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

    // Ctrl+C
    signal(SIGINT, handle);
    
    // while (run) {
    //     int new_socket = accept(server_sockfd, (struct sockaddr *) &sa_addr, client_addr_sizes + free_addr);
    //     if (!run) break;
    //     if (new_socket >= 0) {
    //         printf("ACCEPTED\n");
    //         recv(new_socket, welcome, MAX_CLIENT_NAME_SIZE - 1, 0);
    //         sprintf(welcome + strlen(welcome), " has entered the chat!");
    //         client_sockfd[free_addr] = new_socket;
    //         for (size_t i = 0; i < MAX_BACKLOG; i++) {
    //             send(client_sockfd[i], welcome, sizeof(welcome), 0);
    //         }
    //         free_addr++;
    //         free_addr %= MAX_BACKLOG;
    //     }
    //     for (size_t i = 0; i < MAX_BACKLOG; i++) {
    //         ssize_t recvlen = recv(client_sockfd[i], client_messages[i].msg, MAX_MSG_LENGTH - 1, 0);
    //         if (strlen(client_messages[i].msg) > 0) {
    //             printf("Client %d: %s\n", client_sockfd[i], client_messages[i].msg);
    //         }
    //         if (recvlen < 0) {
    //             printf("Error\n");
    //         }
    //     }
    // }
    while (run) {}

    for (size_t i = 0; i < MAX_BACKLOG; i++) {
        shutdown(i, SHUT_RDWR);
        close(i);
    }

    close(server_sockfd);

    return EXIT_SUCCESS;
}
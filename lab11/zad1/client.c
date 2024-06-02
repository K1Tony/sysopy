#define _DEFAULT_SOURCE
#define _XOPEN_SOURCE_EXTENDED 1

#include "sys/socket.h"
#include "sys/types.h"
#include "netinet/in.h"
#include "arpa/inet.h"

#include "stdio.h"
#include "stdlib.h"

#include "signal.h"
#include "unistd.h"

#include "string.h"

#include "pthread.h"

// my include
#include "assets.h"

// pthread_mutex_t sender_mutex, receiver_mutex;


volatile int run = 1;

// void *read_msg(void *arg) {
//     pthread_mutex_lock(&receiver_mutex);
// }

// void *send_msg(void *arg) {
//     pthread_mutex_lock(&sender_mutex);
// }

client_t self;

pthread_t sender_thread, receiver_thread;

message_t msg_received, msg_to_send;

void *sender_thread_fun(void *arg) {
    arg = NULL;
    while (run) {
        int len = 0;
        char c = fgetc(stdin);
        while (run && len < MAX_MSG_LENGTH - 1 && c != '\n') {
            msg_to_send.msg[len] = c;
            c = fgetc(stdin);
            len++;
        }
        size_t msg_start = 0;
        if (strncmp(msg_to_send.msg, "LIST", 4) == 0) {
            strncpy(msg_to_send.type, "LIST", MAX_MSG_TYPE_LENGTH);
        } else if (strncmp(msg_to_send.msg, "ALL", 3) == 0) {
            strncpy(msg_to_send.type, "ALL", MAX_MSG_TYPE_LENGTH);
            msg_start = 4;
        } else if (strncmp(msg_to_send.msg, "ONE", 3) == 0) {
            strncpy(msg_to_send.type, "ONE", MAX_MSG_TYPE_LENGTH);
            msg_start = 4;
            msg_to_send.dest = atoi(msg_to_send.msg + msg_start);
            while (msg_to_send.msg[msg_start] - '0' >= 0 && msg_to_send.msg[msg_start] - '0' <= 9) {
                msg_start++;
            }
            msg_start++;
        } else if (strncmp(msg_to_send.msg, "STOP", 4) == 0) {
            strncpy(msg_to_send.type, "STOP", MAX_MSG_TYPE_LENGTH);
        }
        if (!run) break;
        printf("\n!! --- Message sent! --- !!\n\n");
        send(self.sockfd, &msg_to_send, sizeof(msg_to_send), 0);
        memset(msg_to_send.msg, 0, MAX_MSG_LENGTH);
        memset(msg_to_send.type, 0, MAX_MSG_TYPE_LENGTH);
    }

    return NULL;
}

void *receiver_thread_fun(void *arg) {
    arg = NULL;
    while (run) {
        recv(self.sockfd, msg_received.msg, MAX_MSG_LENGTH - 1, 0);
        if (strcmp(msg_received.msg, "TERMINATE") == 0) {
            run = 0;
            break;
        }
        printf("%s\n", msg_received.msg);
        memset(msg_received.msg, 0, MAX_MSG_LENGTH);
    }

    return NULL;
}

void handle(int signum) {
    signum = 0;
    run = signum;
    close(self.sockfd);
}

int main(int argc, char **argv) {

    if (argc != 4) {
        fprintf(stderr, "Invalid argument count! Expected 3, got %d\n", argc - 1);
        return EXIT_FAILURE;
    }
    
    in_port_t server_port = atoi(argv[2]);
    struct in_addr ip_addr;
    if (inet_aton(argv[3], &ip_addr) < 0) {
        perror("Client IP address");
        return EXIT_FAILURE;
    }

    int socket_fd = socket(AF_INET, SOCK_STREAM, 0);

    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = server_port;
    server_addr.sin_addr.s_addr = ip_addr.s_addr;

    if (connect(socket_fd, (struct sockaddr *) &server_addr, sizeof(server_addr)) < 0) {
        perror("Client connect");
        return EXIT_FAILURE;
    }

    strncpy(self.name, argv[1], MAX_CLIENT_NAME_SIZE);
    self.sockfd = socket_fd;

    send(socket_fd, self.name, MAX_CLIENT_NAME_SIZE - 1, 0);

    signal(SIGINT, handle);

    memset(msg_received.msg, 0, MAX_MSG_LENGTH);
    recv(socket_fd, msg_received.msg, MAX_MSG_LENGTH - 1, 0);
    if (strlen(msg_received.msg) > 0 && run) {
        printf("%s\n", msg_received.msg);
        memset(msg_received.msg, 0, MAX_MSG_LENGTH);
    }

    printf("In order to chat type one of the following commands, and arguments if needed.\nLIST - (no args) list all active users\nALL - (arg: message) send your message to everyone\nONE - (args: sockfd, message) send your message to user with \"sockfd\" socket descriptor\nSTOP (or Ctrl+C) - quit the chat\n");

    pthread_create(&receiver_thread, NULL, receiver_thread_fun, &self);
    pthread_create(&sender_thread, NULL, sender_thread_fun, NULL);

    while (run) {}

    close(socket_fd);

    return EXIT_SUCCESS;
}
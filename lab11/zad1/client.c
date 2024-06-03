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

client_t self;

pthread_t sender_thread, receiver_thread, welcome_thread;

pthread_mutex_t receiver_init_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t receiver_init_cond = PTHREAD_COND_INITIALIZER;

message_t msg_received, msg_to_send;

char temp_msg[MAX_MSG_LENGTH + MAX_CLIENT_NAME_SIZE];

void *sender_thread_fun(void *arg) {
    arg = NULL;

    pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS, NULL);
    pthread_mutex_lock(&self.mutex);
    while (run) {
        int len = 0;
        char c = fgetc(stdin);

        strncpy(msg_to_send.author, self.name, MAX_CLIENT_NAME_SIZE);
        while (run && len < MAX_MSG_LENGTH - 1 && c != '\n') {
            temp_msg[len] = c;
            c = fgetc(stdin);
            len++;
        }
        size_t msg_start = 0;
        if (strncmp(temp_msg, "LIST", 4) == 0) {
            strncpy(msg_to_send.type, "LIST", MAX_MSG_TYPE_LENGTH);
        } else if (strncmp(temp_msg, "ALL", 3) == 0) {
            strncpy(msg_to_send.type, "ALL", MAX_MSG_TYPE_LENGTH);
            msg_start = 4;
            strncpy(msg_to_send.msg, temp_msg + msg_start, MAX_MSG_LENGTH - 1);
        } else if (strncmp(temp_msg, "ONE", 3) == 0) {
            strncpy(msg_to_send.type, "ONE", MAX_MSG_TYPE_LENGTH);
            msg_start = 4;
            msg_to_send.dest = atoi(temp_msg + msg_start);
            printf("%d\n", msg_to_send.dest);
            while (temp_msg[msg_start] - '0' >= 0 && temp_msg[msg_start] - '0' <= 9) {
                msg_start++;
            }
            msg_start++;
            strncpy(msg_to_send.msg, temp_msg + msg_start, MAX_MSG_LENGTH - 1);
            printf("%s\n", msg_to_send.msg);
        } else if (strncmp(temp_msg, "STOP", 4) == 0) {
            raise(SIGINT);
            break;
        }
        if (!run) break;
        printf("\n!! --- Message sent! --- !!\n\n");
        send(self.sockfd, &msg_to_send, sizeof(msg_to_send), 0);
        memset(msg_to_send.msg, 0, MAX_MSG_LENGTH);
        memset(msg_to_send.type, 0, MAX_MSG_TYPE_LENGTH);
        memset(temp_msg, 0, sizeof(temp_msg) * sizeof(char));
    }

    return NULL;
}

void *receiver_thread_fun(void *arg) {
    arg = NULL;
    pthread_setcancelstate(PTHREAD_CANCEL_ASYNCHRONOUS, NULL);

    while (run) {
        pthread_cond_broadcast(&receiver_init_cond);
        recv(self.sockfd, &msg_received, sizeof(message_t), 0);
        if (strcmp(msg_received.msg, "TERMINATE") == 0) {
            break;
        }
        printf("%s : %s\n", msg_received.author, msg_received.msg);
        memset(msg_received.msg, 0, MAX_MSG_LENGTH);
        memset(msg_received.author, 0, MAX_CLIENT_NAME_SIZE);
        memset(msg_received.type, 0, MAX_MSG_TYPE_LENGTH);
    }

    return NULL;
}

void *welcome_thread_fun(void *arg) {
    pthread_cond_wait(&receiver_init_cond, &receiver_init_mutex);

    strncpy(msg_to_send.author, self.name, MAX_CLIENT_NAME_SIZE);
    strcpy(msg_to_send.type, "INIT");
    
    send(self.sockfd, &msg_to_send, sizeof(message_t), 0);
    
    memset(msg_to_send.type, 0, MAX_MSG_TYPE_LENGTH);
    memset(msg_to_send.msg, 0, MAX_MSG_LENGTH);

    return NULL;
}

void handle(int signum) {
    signum = 0;
    run = signum;
    strcpy(msg_to_send.type, "STOP");
    send(self.sockfd, &msg_to_send, sizeof(msg_to_send), 0);
    pthread_cancel(sender_thread);
    pthread_cancel(receiver_thread);

    pthread_mutex_destroy(&self.mutex);

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

    signal(SIGINT, handle);

    pthread_mutex_init(&self.mutex, NULL);
    pthread_mutex_lock(&self.mutex);

    if (connect(socket_fd, (struct sockaddr *) &server_addr, sizeof(server_addr)) < 0) {
        perror("Client connect");
        return EXIT_FAILURE;
    }

    strncpy(self.name, argv[1], MAX_CLIENT_NAME_SIZE);
    self.sockfd = socket_fd;

    pthread_mutex_lock(&receiver_init_mutex);

    pthread_create(&welcome_thread, NULL, welcome_thread_fun, NULL);
    pthread_create(&receiver_thread, NULL, receiver_thread_fun, &self);
    pthread_create(&sender_thread, NULL, sender_thread_fun, NULL);

    memset(msg_received.msg, 0, MAX_MSG_LENGTH);
    // recv(socket_fd, msg_received.msg, MAX_MSG_LENGTH - 1, 0);
    // if (strlen(msg_received.msg) > 0 && run) {
    //     printf("%s\n", msg_received.msg);
    //     memset(msg_received.msg, 0, MAX_MSG_LENGTH);
    // }
    pthread_mutex_unlock(&self.mutex);

    printf("In order to chat type one of the following commands, and arguments if needed.\nLIST - (no args) list all active users\nALL - (arg: message) send your message to everyone\nONE - (args: sockfd, message) send your message to user with \"sockfd\" socket descriptor\nSTOP (or Ctrl+C) - quit the chat\n");

    pthread_join(sender_thread, NULL);
    pthread_join(receiver_thread, NULL);

    return EXIT_SUCCESS;
}
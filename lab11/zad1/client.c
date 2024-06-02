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

void handle(int signum) {
    run = 0;
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

    message_t msg_to_send;
    msg_to_send.sockfd = socket_fd;

    message_t msg_received;
    msg_received.sockfd = socket_fd;

    memset(msg_received.msg, 0, MAX_MSG_LENGTH);

    while (run) {
        recv(socket_fd, msg_received.msg, MAX_MSG_LENGTH - 1, 0);
        if (strlen(msg_received.msg) > 0) {
            printf("%s\n", msg_received.msg);
        }
    }

    close(socket_fd);

    return EXIT_SUCCESS;
}
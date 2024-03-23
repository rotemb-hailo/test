// NOTE: This file was based on tcp_client.c and tcp_server.c from rec 10

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>

#define BUFFER_SIZE (1 * 1024 * 1024) // 1MB in bytes


// Function declarations:
void SIGINT_handler(int signum);

void print_statistics();

// Global variables:
uint16_t pcc_total[95];
int connfd = -1;
int waiting_for_clients = 1;

void SIGINT_handler(int signum) {
    if (connfd == -1) {
        print_statistics();
    }
    waiting_for_clients = 0;
}

void print_statistics() {
    int i;
    for (i = 0; i < 95; i++) {
        printf("char '%c' : %hu times\n", (char) (i + 32), pcc_total[i]);
    }
    exit(0);
}

int prepare_socket(int port, int *listenfd) {
    const int enable = 1;
    struct sockaddr_in serv_addr;

    // create the TCP connection (socket + listen + bind):
    if ((*listenfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("Could not create socket. \n");
        return -1;
    }
    if (setsockopt(*listenfd, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int)) < 0) {
        perror("setsockopt failed. \n");
        return -1;
    }
    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    serv_addr.sin_port = htons(port);
    if (bind(*listenfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) != 0) {
        perror("Bind Failed. \n");
        return -1;
    }
    if (listen(*listenfd, 10) != 0) {
        perror("Listen Failed. \n");
        return -1;
    }

    return 0;
}

int recv_n(int connfd, void *buffer, int size) {
    int total_sent, not_sent, cur_sent;
    total_sent = 0;
    not_sent = size;
    while (not_sent > 0) {
        cur_sent = read(connfd, (char *) buffer + total_sent, not_sent);
        if (cur_sent == 0 || (cur_sent < 0 && (errno == ETIMEDOUT || errno == ECONNRESET || errno == EPIPE))) {
            perror("Failed to receive N from the client (EOF, ETIMEDOUT, ECONNRESET, EPIPE). \n");
            return -1;
        } else if (cur_sent < 0) {
            perror("Failed to receive N from the client (Not handled error condition). \n");
            return -1;
        } else {
            total_sent += cur_sent;
            not_sent -= cur_sent;
        }
    }
    return 0;
}

int send_c(int connfd, uint16_t C) {
    int total_sent, not_sent, cur_sent;
    total_sent = 0;
    not_sent = sizeof(uint16_t);
    while (not_sent > 0) {
        cur_sent = write(connfd, (char *) &C + total_sent, not_sent);
        if (cur_sent == 0 || (cur_sent < 0 && (errno == ETIMEDOUT || errno == ECONNRESET || errno == EPIPE))) {
            perror("Failed to send C to the client (EOF, ETIMEDOUT, ECONNRESET, EPIPE). \n");
            return -1;
        } else if (cur_sent < 0) {
            perror("Failed to send C to the client (Not handled error condition). \n");
            return -1;
        } else {
            total_sent += cur_sent;
            not_sent -= cur_sent;
        }
    }
    return 0;
}

int main(int argc, char *argv[]) {
    int total_sent, not_sent, cur_sent, message_len, i;
    int listenfd = -1;
    uint16_t N, C;
    C = 0;

    char buffer[BUFFER_SIZE]; // buffer with less than 1MB
    uint16_t temp_pcc_total[95];
    struct sigaction new_sigint_action = {
            .sa_handler = SIGINT_handler,
            .sa_flags = SA_RESTART};

    // Initialize pcc_total:
    memset(pcc_total, 0, sizeof(pcc_total));

    // Handling of SIGINT:
    if (sigaction(SIGINT, &new_sigint_action, NULL) == -1) {
        perror("Signal handle registration failed");
        exit(1);
    }

    // Check correct number of arguments:
    if (argc != 2) {
        perror("You should pass the server's port. \n");
        exit(1);
    }

    if (prepare_socket(atoi(argv[1]), &listenfd) < 0) {
        exit(1);
    }

    while (waiting_for_clients) {
        // Accept a connection:
        connfd = accept(listenfd, NULL, NULL);
        if (connfd < 0) {
            perror("Accept Failed. \n");
            exit(1);
        }

        if (recv_n(connfd, &N, sizeof(uint16_t)) < 0) {
            continue;
        }

        N = ntohs(N);

        // Receive N bytes (the file content) and calculating statistics:
        memset(temp_pcc_total, 0, sizeof(temp_pcc_total));
        total_sent = 0;
        not_sent = N;
        while (not_sent > 0) {
            message_len = sizeof(buffer) < not_sent ? sizeof(buffer) : not_sent;

            cur_sent = read(connfd, (char *) &buffer, message_len);
            if (cur_sent == 0 || (cur_sent < 0 && (errno == ETIMEDOUT || errno == ECONNRESET || errno == EPIPE))) {
                perror("Failed to receive the file content from the client (EOF, ETIMEDOUT, ECONNRESET, EPIPE). \n");
                close(connfd);
                not_sent = 0;
                connfd = -1;
            } else if (cur_sent < 0) {
                perror("Failed to receive the file content from the client (Not handled error condition). \n");
                close(connfd);
                exit(1);
            } else {
                for (i = 0; i < cur_sent; i++) {
                    if (buffer[i] >= 32 && buffer[i] <= 126) {
                        temp_pcc_total[(int) (buffer[i]) - 32]++;
                        C++;
                    }
                }
                total_sent += cur_sent;
                not_sent -= cur_sent;
            }
        }
        if (connfd == -1) {
            continue;
        }

        C = htons(C);

        if (send_c(connfd, C) < 0) {
            continue;
        }

        for (i = 0; i < 95; i++) {
            pcc_total[i] += temp_pcc_total[i];
            temp_pcc_total[i] = 0;
        }
        close(connfd);
        connfd = -1;
        C = 0;
    }
    print_statistics();
}
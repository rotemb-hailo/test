
// NOTE: This file was based on tcp_client.c and tcp_server.c from rec 10

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>


// Function declarations:
void SIGINT_handler(int signum);

void print_statistics();

// Global variables:
uint32_t pcc_total[95];
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

int main(int argc, char *argv[]) {
    int total_sent, not_sent, cur_sent, message_len, i;
    int listenfd = -1;
    const int enable = 1;
    uint32_t N, C;
    C = 0;

    char buffer[1000000]; // buffer with less than 1MB
    uint32_t temp_pcc_total[95];
    struct sigaction new_sigint_action = {
            .sa_handler = SIGINT_handler,
            .sa_flags = SA_RESTART};
    struct sockaddr_in serv_addr;

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

    // create the TCP connection (socket + listen + bind):
    if ((listenfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("Could not create socket. \n");
        exit(1);
    }
    if (setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int)) < 0) {
        perror("setsockopt failed. \n");
        exit(1);
    }
    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    serv_addr.sin_port = htons(atoi(argv[1]));
    if (bind(listenfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) != 0) {
        perror("Bind Failed. \n");
        exit(1);
    }
    if (listen(listenfd, 10) != 0) {
        perror("Listen Failed. \n");
        exit(1);
    }

    while (waiting_for_clients) {
        // Accept a connection:
        connfd = accept(listenfd, NULL, NULL);
        if (connfd < 0) {
            perror("Accept Failed. \n");
            exit(1);
        }

        // Receive N (the number of bytes that will be transferred):
        total_sent = 0;
        not_sent = 4;
        while (not_sent > 0) {
            cur_sent = read(connfd, (char *) &N + total_sent, not_sent);
            if (cur_sent == 0 || (cur_sent < 0 && (errno == ETIMEDOUT || errno == ECONNRESET || errno == EPIPE))) {
                perror("Failed to receive N from the client (EOF, ETIMEDOUT, ECONNRESET, EPIPE). \n");
                close(connfd);
                not_sent = 0;
                connfd = -1;
            } else if (cur_sent < 0) {
                perror("Failed to receive N from the client (Not handled error condition). \n");
                close(connfd);
                exit(1);
            } else {
                total_sent += cur_sent;
                not_sent -= cur_sent;
            }
        }
        if (connfd == -1) {
            continue;
        }
        N = ntohl(N);

        // Receive N bytes (the file content) and calculating statistics:
        memset(temp_pcc_total, 0, sizeof(temp_pcc_total));
        total_sent = 0;
        not_sent = N;
        while (not_sent > 0) {
            if (sizeof(buffer) < not_sent) {
                message_len = sizeof(buffer);
            } else {
                message_len = not_sent;
            }
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
        C = htonl(C);

        // Send C (the number of printable characters):
        total_sent = 0;
        not_sent = 4;
        while (not_sent > 0) {
            cur_sent = write(connfd, (char *) &C + total_sent, not_sent);
            if (cur_sent == 0 || (cur_sent < 0 && (errno == ETIMEDOUT || errno == ECONNRESET || errno == EPIPE))) {
                perror("Failed to send C to the client (EOF, ETIMEDOUT, ECONNRESET, EPIPE). \n");
                close(connfd);
                not_sent = 0;
                connfd = -1;
            } else if (cur_sent < 0) {
                perror("Failed to send C to the client (Not handled error condition). \n");
                close(connfd);
                exit(1);
            } else {
                total_sent += cur_sent;
                not_sent -= cur_sent;
            }
        }
        if (connfd == -1) {
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
// NOTE: This file was based on tcp_client.c and tcp_server.c from rec 10

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <stdint.h>
#include <fcntl.h> // Added for open

#define BUFFER_SIZE (1 * 1024 * 1024) // 1MB in bytes

int validate_arguments_and_open_file(int argc, char *argv[], int *file_fd) {
    if (argc != 4) {
        perror("You should pass correct number of arguments: 1 - server's IP address, 2 - server's port, 3 - path of the file to send. \n");
        return -1;
    }

    *file_fd = open(argv[3], O_RDONLY);
    if (*file_fd < 0) {
        perror("Failed to open the file. \n");
        return -1;
    }

    return 0;
}

int create_and_open_socket(int *socket_fd, char *ip_address, int port) {
    if ((*socket_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("Could not create socket. \n");
        return -1;
    }

    struct sockaddr_in serv_addr;
    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(port);
    if (inet_pton(AF_INET, ip_address, &serv_addr.sin_addr) != 1) {
        perror("Ip address is not valid. \n");
        return -1;
    }
    if (connect(*socket_fd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0) {
        perror("Connect Failed. \n");
        return -1;
    }

    return 0;
}

int check_file_size(int file_fd, uint16_t *N) {
    // Seek to the end of the file
    if (lseek(file_fd, 0, SEEK_END) < 0) {
        perror("lseek Failed. \n");
        return -1;
    }

    // Get the current position, which is the size of the file
    off_t size = lseek(file_fd, 0, SEEK_CUR);
    if (size < 0) {
        perror("lseek Failed. \n");
        return -1;
    }

    *N = size;

    // Seek back to the beginning of the file
    if (lseek(file_fd, 0, SEEK_SET) < 0) {
        perror("lseek Failed. \n");
        return -1;
    }

    return 0;
}

int send_n(int sockfd, uint16_t N) {
    int total_sent, not_sent, cur_sent;
    uint16_t N_for_sending = htons(N);

    total_sent = 0;
    not_sent = sizeof(uint16_t);
    while (not_sent > 0) {
        cur_sent = write(sockfd, (char *) &N_for_sending + total_sent, not_sent);
        if (cur_sent <= 0) {
            perror("Failed to send N to the server. \n");
            return -1;
        }
        total_sent += cur_sent;
        not_sent -= cur_sent;
    }

    return 0;
}

int send_file_content(int sockfd, int fd, uint16_t N) {
    int total_sent, not_sent, cur_sent, cur_read, message_len;
    char buffer[BUFFER_SIZE];

    total_sent = 0;
    not_sent = N;
    while (not_sent > 0) {
        if (sizeof(buffer) < not_sent) {
            message_len = sizeof(buffer);
        } else {
            message_len = not_sent;
        }
        cur_read = read(fd, buffer, message_len);
        memset(buffer + cur_read, 0, message_len - cur_read);
        cur_sent = write(sockfd, buffer, cur_read);
        if (cur_sent <= 0) {
            perror("Failed to send file content to the server. \n");
            return -1;
        }
        total_sent += cur_sent;
        not_sent -= cur_sent;
        lseek(fd, total_sent, SEEK_SET);
    }

    return 0;
}

int recv_c(int sockfd, uint16_t *C) {
    int total_sent, not_sent, cur_sent;
    uint16_t C_received;

    total_sent = 0;
    not_sent = sizeof(uint16_t);
    while (not_sent > 0) {
        cur_sent = read(sockfd, (char *) &C_received + total_sent, not_sent);
        if (cur_sent <= 0) {
            perror("Failed to receive C from the server. \n");
            return -1;
        }
        total_sent += cur_sent;
        not_sent -= cur_sent;
    }

    *C = C_received;
    return 0;
}

int main(int argc, char *argv[]) {
    int total_sent, not_sent, cur_sent, cur_read, message_len;
    int sockfd = -1, fd = -1;
    uint16_t N, N_for_sending, C;
    char buffer[BUFFER_SIZE]; // buffer with less than 1MB

    struct sockaddr_in serv_addr;

    if (validate_arguments_and_open_file(argc, argv, &fd) < 0) {
        exit(1);
    }

    if (create_and_open_socket(&sockfd, argv[1], atoi(argv[2])) < 0) {
        exit(1);
    }

    if (check_file_size(fd, &N) < 0) {
        exit(1);
    }

    if (send_n(sockfd, N) < 0) {
        exit(1);
    }

    if (send_file_content(sockfd, fd, N) < 0) {
        exit(1);
    }

    close(fd);

    if (recv_c(sockfd, &C) < 0) {
        exit(1);
    }

    close(sockfd);
    C = ntohs(C);
    printf("# of printable characters: %hu\n", C);
    exit(0);
}
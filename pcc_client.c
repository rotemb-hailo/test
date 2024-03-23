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

int main(int argc, char *argv[]) {
    int total_sent, not_sent, cur_sent, cur_read, message_len;
    int sockfd = -1;
    uint16_t N, N_for_sending, C;
    char buffer[1000000]; // buffer with less than 1MB
    int fd;

    struct sockaddr_in serv_addr;

    // Check correct number of arguments:
    if (argc != 4) {
        perror("You should pass correct number of arguments: 1 - server's IP address, 2 - server's port, 3 - path of the file to send. \n");
        exit(1);
    }
    // We can assume a valid IP address and a 16-bit unsigned integer for the port, so we only check that we can open the file

    // Open the file for reading:
    fd = open(argv[3], O_RDONLY);
    if (fd == -1) {
        perror("Failed to open the file. \n");
        exit(1);
    }

    // Create the TCP connection (socket + connect):
    if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("Could not create socket. \n");
        exit(1);
    }
    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(atoi(argv[2]));
    if (inet_pton(AF_INET, argv[1], &serv_addr.sin_addr) != 1) {
        perror("Ip address is not valid. \n");
        exit(1);
    }
    if (connect(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0) {
        perror("Connect Failed. \n");
        exit(1);
    }

    // Check the file size:
    if (lseek(fd, 0, SEEK_END) < 0) {
        perror("lseek Failed. \n");
        exit(1);
    }
    N = lseek(fd, 0, SEEK_CUR);
    if (lseek(fd, 0, SEEK_SET) < 0) {
        perror("lseek Failed. \n");
        exit(1);
    }
    N_for_sending = htons(N);

    // Send N (the number of bytes that will be transferred):
    total_sent = 0;
    not_sent = sizeof(uint16_t);
    while (not_sent > 0) {
        cur_sent = write(sockfd, (char *) &N_for_sending + total_sent, not_sent);
        if (cur_sent <= 0) {
            close(sockfd);
            perror("Failed to send N to the server. \n");
            exit(1);
        }
        total_sent += cur_sent;
        not_sent -= cur_sent;
    }

    // Send the N bytes (the file content):
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
            close(sockfd);
            perror("Failed to send file content to the server. \n");
            exit(1);
        }
        total_sent += cur_sent;
        not_sent -= cur_sent;
        lseek(fd, total_sent, SEEK_SET);
    }
    close(fd);

    // Receive C (the number of printable characters)
    total_sent = 0;
    not_sent = sizeof(uint16_t);
    while (not_sent > 0) {
        cur_sent = read(sockfd, (char *) &C + total_sent, not_sent);
        if (cur_sent <= 0) {
            close(sockfd);
            perror("Failed to receive C from the server. \n");
            exit(1);
        }
        total_sent += cur_sent;
        not_sent -= cur_sent;
    }

    close(sockfd);
    C = ntohs(C);
    printf("# of printable characters: %hu\n", C);
    exit(0);
}
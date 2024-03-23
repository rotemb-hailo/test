#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>

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

int send_n(int socket_fd, uint16_t N) {
    int total_bytes_sent, total_bytes_to_send, current_bytes_sent;
    uint16_t N_for_sending = htonl(N);

    total_bytes_sent = 0;
    total_bytes_to_send = sizeof(N);

    while (total_bytes_to_send > 0) {
        current_bytes_sent = write(socket_fd, (char *) &N_for_sending + total_bytes_sent, total_bytes_to_send);
        if (current_bytes_sent <= 0) {
            close(socket_fd);
            perror("Failed to send N to the server. \n");
            return -1;
        }
        total_bytes_sent += current_bytes_sent;
        total_bytes_to_send -= current_bytes_sent;
    }

    return 0;
}

int send_file_content(int socket_fd, int file_fd, uint16_t N) {
    int total_bytes_sent = 0;
    int total_bytes_to_send = N;
    int current_bytes_sent;
    char buffer[BUFFER_SIZE];

    while (total_bytes_to_send > 0) {
        int message_len = (BUFFER_SIZE < total_bytes_to_send) ? BUFFER_SIZE : total_bytes_to_send;

        int current_bytes_read = read(file_fd, buffer, message_len);
        if (current_bytes_read < 0) {
            perror("Failed to read from file. \n");
            return -1;
        }

        current_bytes_sent = write(socket_fd, buffer, current_bytes_read);
        if (current_bytes_sent <= 0) {
            perror("Failed to send file content to the server. \n");
            return -1;
        }

        total_bytes_sent += current_bytes_sent;
        total_bytes_to_send -= current_bytes_sent;
    }

    return 0;
}

int receive_c(int socket_fd, uint16_t *C) {
    int total_bytes_sent, total_bytes_to_send, current_bytes_sent;

    total_bytes_sent = 0;
    total_bytes_to_send = sizeof(*C);

    while (total_bytes_to_send > 0) {
        current_bytes_sent = read(socket_fd, (char *) C + total_bytes_sent, total_bytes_to_send);
        if (current_bytes_sent <= 0) {
            close(socket_fd);
            perror("Failed to receive C from the server. \n");
            return -1;
        }
        total_bytes_sent += current_bytes_sent;
        total_bytes_to_send -= current_bytes_sent;
    }

    return 0;
}

int main(int argc, char *argv[]) {
    int socket_fd = -1, file_fd = -1;
    uint16_t N, C;

    if (validate_arguments_and_open_file(argc, argv, &file_fd) < 0) {
        exit(1);
    }

    if (create_and_open_socket(&socket_fd, argv[1], atoi(argv[2])) < 0) {
        close(file_fd);
        exit(1);
    }

    if (check_file_size(file_fd, &N) < 0) {
        close(socket_fd);
        exit(1);
    }

    if (send_n(socket_fd, N) < 0) {
        close(socket_fd);
        exit(1);
    }

    if (send_file_content(socket_fd, file_fd, N) < 0) {
        close(socket_fd);
        exit(1);
    }
    close(file_fd);

    if (receive_c(socket_fd, &C) < 0) {
        close(socket_fd);
        exit(1);
    }
    close(socket_fd);

    C = ntohl(C);
    printf("# of printable characters: %hu\n", C);
    exit(0);
}
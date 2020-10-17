#include <netinet/in.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <liburing.h>

#define OP_CODE_ACCEPT       0

struct queue_entry {
    int operation_code;
};

int server_socket = -1;
static struct io_uring ring;

int setup_listening_socket(int port) {
    int sock;
    struct sockaddr_in srv_addr;

    sock = socket(PF_INET, SOCK_STREAM, 0);
    if (sock == -1) {
        perror("socket == -1");
    }

    int enable = 1;
    if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int)) < 0) {
        perror("setsockopt < 0");
        exit(1);
    }

    memset(&srv_addr, 0, sizeof(srv_addr));
    srv_addr.sin_family = AF_INET;
    srv_addr.sin_port = htons(port);
    srv_addr.sin_addr.s_addr = htonl(INADDR_ANY);

    if (bind(sock, (const struct sockaddr *) &srv_addr, sizeof(srv_addr)) < 0) {
        perror("bind < 0");
        exit(1);
    }

    if (listen(sock, 10) < 0) {
        perror("bind < 0");
        exit(1);
    }

    return (sock);
}

void handle_sigint(int signo) {
    io_uring_queue_exit(&ring);
    exit(0);
}

void enqueue_accept() {
    struct queue_entry *entry = malloc(sizeof(*entry));
    entry->operation_code = OP_CODE_ACCEPT;

    struct io_uring_sqe *sqe = io_uring_get_sqe(&ring);
    io_uring_prep_accept(sqe, server_socket, NULL, NULL, 0);
    io_uring_sqe_set_data(sqe, entry);
    io_uring_submit(&ring);
}

// polling mode is a privileged operation (https://kernel.dk/io_uring.pdf)
int main() {
    unsigned entries = 64;

//    struct io_uring_params params;
//    memset(&params, 0, sizeof(params));

//    params.flags |= IORING_SETUP_SQPOLL;
//    params.sq_thread_idle = 2000;

//    int ring_init_response = io_uring_queue_init_params(entries, &ring, &params);
    int ring_init_response = io_uring_queue_init(entries, &ring, IORING_SETUP_SQPOLL);
	if (ring_init_response < 0) {
		perror("io_uring_queue_init < 0");
		return -1;
	}

    server_socket = setup_listening_socket(3000);

    enqueue_accept(0);

    while(1) {
        struct io_uring_cqe *cqe;
        if(io_uring_wait_cqe(&ring, &cqe) < 0) {
            continue;
        }

        if (cqe->res < 0) {
            printf("cqe->res:%s\n", strerror(-cqe->res));
            io_uring_queue_exit(&ring);
            exit(1);
        }

        struct queue_entry *entry = (struct queue_entry *) cqe->user_data;
        switch (entry->operation_code) {
            case OP_CODE_ACCEPT:
                printf("OP_CODE_ACCEPT Complete\n");
                exit(0);
                break;
        }

        io_uring_cqe_seen(&ring, cqe);
    }

    return 0;
}

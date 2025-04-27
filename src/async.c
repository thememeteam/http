#define _GNU_SOURCE

#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/socket.h>
#include <netdb.h>
#include <sys/signal.h>
#include <liburing.h>

#define BUF_SIZE 4096

typedef enum __event_type {
    EVENT_ACCEPT,
    EVENT_READ,
    EVENT_WRITE
} event_type;

typedef struct __req_data {
    event_type event;
    int client_fd;
    char* buf;
} req_data;

struct io_uring ring;
int sockfd;
struct sockaddr_in addr;
socklen_t addr_len = sizeof(addr);

void* mzero(int size)
{
    void* ptr = malloc(size);
    memset(ptr, 0, size);
    return ptr;
}

void add_accept_request()
{
    req_data* data = mzero(sizeof(req_data));
    data->event = EVENT_ACCEPT;
    struct io_uring_sqe* sqe = io_uring_get_sqe(&ring);
    io_uring_sqe_set_data(sqe, data);
    io_uring_prep_accept(sqe, sockfd, (struct sockaddr*)&addr, &addr_len, 0);
}

void add_read_request(int client_fd)
{
    char* buf = mzero(BUF_SIZE);
    req_data* data = mzero(sizeof(req_data));
    data->event = EVENT_READ;
    data->client_fd = client_fd;
    data->buf = buf;
    struct io_uring_sqe* sqe = io_uring_get_sqe(&ring);
    io_uring_sqe_set_data(sqe, data);
    io_uring_prep_read(sqe, client_fd, buf, BUF_SIZE, 0);
}

void add_write_request(int client_fd, char* buf, int len)
{
    req_data* data = mzero(sizeof(req_data));
    data->event = EVENT_WRITE;
    data->client_fd = client_fd;
    data->buf = buf;
    struct io_uring_sqe* sqe = io_uring_get_sqe(&ring);

    io_uring_sqe_set_data(sqe, data);
    io_uring_prep_write(sqe, client_fd, buf, len, 0);
}

int main()
{
    signal(SIGPIPE, SIG_IGN);

	memset(&addr, 0, sizeof(struct sockaddr_in));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(8080);

	sockfd = socket(AF_INET, SOCK_STREAM, 0);
	int yes = 1;
	setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));
	int s = bind(sockfd, &addr, sizeof(addr));
	if(s < 0)
	{
		perror("bind");
	}

	s = listen(sockfd, SOMAXCONN);

    io_uring_queue_init(512, &ring, 0);

    add_accept_request();

    struct io_uring_cqe* cqe;

    while(1)
    {
        io_uring_submit_and_wait(&ring, 1);

        unsigned head;
        int i = 0;

        io_uring_for_each_cqe(&ring, head, cqe) {
            req_data* data = (req_data*)io_uring_cqe_get_data(cqe);
    
            switch(data->event)
            {
                case EVENT_ACCEPT:
                    add_accept_request();
                    add_read_request(cqe->res);
                    free(data);
                    break;
    
                case EVENT_READ:
                    if(cqe->res == 0)
                    {
                        close(data->client_fd);
                        free(data->buf);
                        free(data);
                        break;
                    }

                    char* in_buf = data->buf;
                    char *body = strstr(in_buf, "\r\n\r\n");
                    body = body ? body + 4 : "";
    
                    char* out_buf = malloc(BUF_SIZE);

                    snprintf(out_buf,
                        BUF_SIZE,
                        "HTTP/1.1 200 OK\r\n"
                        "Content-Type: text/plain\r\n"
                        "Connection: keep-alive\r\n"
                        "Content-Length: %lu\r\n"
                        "\r\n"
                        "%s\r\n", strlen(body) + 2, body);
    
                    add_write_request(data->client_fd, out_buf, strlen(out_buf));
    
                    free(in_buf);
                    free(data);
                    break;
    
                case EVENT_WRITE:
                    int client_fd = data->client_fd;
                    free(data->buf);
                    free(data);

                    add_read_request(client_fd);

                    break;
            }
            i++;
        }

        io_uring_cq_advance(&ring, i);
    }

    shutdown(sockfd, SHUT_RDWR);
}

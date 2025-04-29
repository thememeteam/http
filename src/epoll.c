#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <errno.h>

#define PORT 8080
#define MAX_EVENTS 256
#define BUFFER_SIZE 4096

int set_nonblocking(int fd) {
    int flags = fcntl(fd, F_GETFL, 0);
    return fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

void handle_connection(int client_fd) {
    char buffer[BUFFER_SIZE] = {0};
    int bytes = read(client_fd, buffer, BUFFER_SIZE - 1);
    if (bytes <= 0) return;

    // Look for body after headers
    char *body = strstr(buffer, "\r\n\r\n");
    if (body) body += 4;

    const char *response_fmt =
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: text/plain\r\n"
        "Content-Length: %ld\r\n"
        "\r\n%s";

    char response[BUFFER_SIZE];
    if (body && strlen(body) > 0) {
    char modified_body[BUFFER_SIZE];
    if (strncmp(body, "hello", 5) == 0) {
        snprintf(modified_body, sizeof(modified_body), "how are you\n");
    } else {
        snprintf(modified_body, sizeof(modified_body), "wrong go ahead and change pls\n");
    }
    snprintf(response, sizeof(response), response_fmt, strlen(modified_body), modified_body);
}

    send(client_fd, response, strlen(response), 0);
}

int main() {
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    set_nonblocking(server_fd);

    struct sockaddr_in addr = {0};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(PORT);

    bind(server_fd, (struct sockaddr *)&addr, sizeof(addr));
    listen(server_fd, SOMAXCONN);

    int epoll_fd = epoll_create1(0);
    struct epoll_event ev = { .events = EPOLLIN, .data.fd = server_fd };
    epoll_ctl(epoll_fd, EPOLL_CTL_ADD, server_fd, &ev);

    struct epoll_event events[MAX_EVENTS];
    printf("Listening on port %d...\n", PORT);

    while (1) {
        int nfds = epoll_wait(epoll_fd, events, MAX_EVENTS, -1);
        for (int i = 0; i < nfds; i++) {
            if (events[i].data.fd == server_fd) {
                int client_fd = accept(server_fd, NULL, NULL);
                set_nonblocking(client_fd);
                ev.events = EPOLLIN | EPOLLET;
                ev.data.fd = client_fd;
                epoll_ctl(epoll_fd, EPOLL_CTL_ADD, client_fd, &ev);
            } else {
                handle_connection(events[i].data.fd);
                close(events[i].data.fd);
            }
        }
    }

    close(server_fd);
    return 0;
}

#define _GNU_SOURCE

#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/socket.h>
#include <netdb.h>
#include <sys/signal.h>

#define BUFFER_SIZE 4096

int main()
{
	printf("Hello world!\n");
	signal(SIGPIPE, SIG_IGN);

	struct sockaddr_in addr;

	memset(&addr, 0, sizeof(struct sockaddr_in));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(8081);

	int sockfd = socket(AF_INET, SOCK_STREAM, 0);
	int yes = 1;
	setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));
	int s = bind(sockfd, &addr, sizeof(addr));
	if(s < 0)
	{
		perror("bind");
	}

	s = listen(sockfd, SOMAXCONN);

	char buffer[BUFFER_SIZE];
	int reqcount = 0;

	while (1) {
        int client_fd = accept(sockfd, NULL, NULL);
        if (client_fd < 0) {
            perror("accept failed");
            continue;
        }

        memset(buffer, 0, BUFFER_SIZE);
		read(client_fd, buffer, BUFFER_SIZE);
		
		char *body = strstr(buffer, "\r\n\r\n");
		body = body ? body + 4 : "";

		char out_buf[BUFFER_SIZE];

		snprintf(out_buf,
			BUFFER_SIZE,
			"HTTP/1.1 200 OK\r\n"
			"Content-Type: text/plain\r\n"
			"Connection: close\r\n"
			"Content-Length: %lu\r\n"
			"\r\n"
			"%s\r\n", strlen(body) + 2, body);

		send(client_fd, out_buf, strlen(out_buf), 0);
		close(client_fd);

		continue;
    }

	shutdown(sockfd, SHUT_RDWR);
}

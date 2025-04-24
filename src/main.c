#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <stdio.h>

#pragma comment(lib, "Ws2_32.lib")

#define PORT "5000"

int main() {
    WSADATA wsaData;
    WSAStartup(MAKEWORD(2, 2), &wsaData);

    struct addrinfo hints = {0}, *res = NULL;
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;
    hints.ai_flags = AI_PASSIVE;

    if (getaddrinfo(NULL, PORT, &hints, &res) != 0) {
        printf("[!] getaddrinfo failed\n");
        return 1;
    }

    SOCKET listenSocket = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if (listenSocket == INVALID_SOCKET) {
        printf("[!] Failed to create socket\n");
        freeaddrinfo(res);
        WSACleanup();
        return 1;
    }

    if (bind(listenSocket, res->ai_addr, (int)res->ai_addrlen) == SOCKET_ERROR) {
        printf("[!] Bind failed: %d\n", WSAGetLastError());
        closesocket(listenSocket);
        freeaddrinfo(res);
        WSACleanup();
        return 1;
    }

    freeaddrinfo(res);

    if (listen(listenSocket, SOMAXCONN) == SOCKET_ERROR) {
        printf("[!] Listen failed\n");
        closesocket(listenSocket);
        WSACleanup();
        return 1;
    }

    printf("[+] Listening on http://localhost:%s/ ...\n", PORT);

    while (1) {
        printf("[*] Waiting for new client...\n");
        SOCKET client = accept(listenSocket, NULL, NULL);
        if (client == INVALID_SOCKET) {
            printf("[!] Accept failed\n");
            continue;
        }

        printf("[+] Accepted client socket: %d\n", (int)client);

        const char* response =
            "HTTP/1.1 200 OK\r\n"
            "Content-Length: 13\r\n"
            "Content-Type: text/plain\r\n"
            "\r\n"
            "Hello, world!";

        send(client, response, (int)strlen(response), 0);
        printf("[+] Response sent to socket: %d\n", (int)client);

        closesocket(client);
    }

    closesocket(listenSocket);
    WSACleanup();
    return 0;
}

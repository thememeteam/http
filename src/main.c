#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <winsock2.h>
#include <mswsock.h>
#include <ws2tcpip.h>
#include <stdio.h>

#pragma comment(lib, "Ws2_32.lib")

#define PORT "5000"
#define BUFFER_SIZE 1024

typedef struct {
    OVERLAPPED overlapped;
    WSABUF wsaBuf;
    char buffer[BUFFER_SIZE];
    SOCKET clientSocket;
} ClientContext;

DWORD WINAPI workerThread(LPVOID lpParam) {
    HANDLE iocp = (HANDLE)lpParam;
    DWORD bytesTransferred;
    ULONG_PTR completionKey;
    ClientContext* context;

    while (1) {
        BOOL success = GetQueuedCompletionStatus(iocp, &bytesTransferred, &completionKey, (LPOVERLAPPED*)&context, INFINITE);
        if (!success || bytesTransferred == 0) {
            if (context) {
                printf("[!] Client disconnected or error\n");
                closesocket(context->clientSocket);
                free(context);
            }
            continue;
        }

        printf("[*] Processing request from socket: %d\n", (int)context->clientSocket);

        const char* response =
            "HTTP/1.1 200 OK\r\n"
            "Content-Length: 13\r\n"
            "Content-Type: text/plain\r\n"
            "\r\n"
            "Hello, world!";

        send(context->clientSocket, response, (int)strlen(response), 0);
        printf("[+] Response sent to socket: %d\n", (int)context->clientSocket);
        closesocket(context->clientSocket);
        free(context);
    }
}

int main() {
    WSADATA wsaData;
    WSAStartup(MAKEWORD(2,2), &wsaData);

    struct addrinfo hints = {0}, *res;
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;

    getaddrinfo(NULL, PORT, &hints, &res);

    SOCKET listenSocket = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    bind(listenSocket, res->ai_addr, (int)res->ai_addrlen);
    listen(listenSocket, SOMAXCONN);

    HANDLE iocp = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 0);
    CreateIoCompletionPort((HANDLE)listenSocket, iocp, 0, 0);

    // Launch a worker thread
    CreateThread(NULL, 0, workerThread, iocp, 0, NULL);

    printf("[+] Listening on http://localhost:%s/ ...\n", PORT);

    while (1) {
        printf("[*] Waiting for new client...\n");
        SOCKET client = accept(listenSocket, NULL, NULL);
        if (client == INVALID_SOCKET) continue;

        printf("[+] Accepted client socket: %d\n", (int)client);

        u_long mode = 1;
        ioctlsocket(client, FIONBIO, &mode); // Set non-blocking

        ClientContext* context = (ClientContext*)calloc(1, sizeof(ClientContext));
        context->clientSocket = client;
        context->wsaBuf.buf = context->buffer;
        context->wsaBuf.len = BUFFER_SIZE;

        CreateIoCompletionPort((HANDLE)client, iocp, 0, 0);

        DWORD flags = 0, bytes = 0;
        int r = WSARecv(client, &context->wsaBuf, 1, &bytes, &flags, &context->overlapped, NULL);
        if (r == SOCKET_ERROR && WSAGetLastError() != WSA_IO_PENDING) {
            printf("[!] Failed to post WSARecv\n");
            closesocket(client);
            free(context);
        }
    }
    //Cleanup (idk why tho)
    freeaddrinfo(res);
    closesocket(listenSocket);
    WSACleanup();
    return 0;
}

#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>

#pragma comment(lib, "Ws2_32.lib")

#define PORT "5000"
#define WORKER_THREADS 4

typedef struct {
    OVERLAPPED overlapped;
    WSABUF wsaBuf;
    char buffer[1024];
    SOCKET socket;
    BOOL isSend;
} PER_IO_CONTEXT;

DWORD WINAPI WorkerThread(LPVOID lpParam) {
    HANDLE iocp = (HANDLE)lpParam;
    DWORD bytesTransferred;
    ULONG_PTR completionKey;
    LPOVERLAPPED overlapped;

    while (1) {
        BOOL result = GetQueuedCompletionStatus(iocp, &bytesTransferred, &completionKey, &overlapped, INFINITE);
        if (!result || overlapped == NULL) {
            continue;
        }

        PER_IO_CONTEXT* ctx = (PER_IO_CONTEXT*)overlapped;
        SOCKET client = ctx->socket;

        if (bytesTransferred == 0) {
            closesocket(client);
            free(ctx);
            continue;
        }

        if (!ctx->isSend) {
            // Don't null-terminate, treat data as binary
            char header[256];
            int headerLen = snprintf(header, sizeof(header),
                "HTTP/1.1 200 OK\r\n"
                "Content-Length: %d\r\n"
                "Content-Type: text/plain\r\n"
                "Connection: keep-alive\r\n"
                "\r\n", bytesTransferred);

            if (headerLen + bytesTransferred > sizeof(ctx->buffer)) {
                // Cap the payload if it would overflow the buffer
                bytesTransferred = sizeof(ctx->buffer) - headerLen;
            }

            memmove(ctx->buffer + headerLen, ctx->buffer, bytesTransferred);
            memcpy(ctx->buffer, header, headerLen);
            ctx->wsaBuf.buf = ctx->buffer;
            ctx->wsaBuf.len = headerLen + bytesTransferred;
            ctx->isSend = TRUE;
            ZeroMemory(&ctx->overlapped, sizeof(OVERLAPPED));

            int s = WSASend(client, &ctx->wsaBuf, 1, NULL, 0, &ctx->overlapped, NULL);
            if (s == SOCKET_ERROR && WSAGetLastError() != WSA_IO_PENDING) {
                closesocket(client);
                free(ctx);
            }
        } else {
            ctx->isSend = FALSE;
            ctx->wsaBuf.len = sizeof(ctx->buffer);
            ctx->wsaBuf.buf = ctx->buffer;
            ZeroMemory(&ctx->overlapped, sizeof(OVERLAPPED));

            DWORD flags = 0;
            int r = WSARecv(client, &ctx->wsaBuf, 1, NULL, &flags, &ctx->overlapped, NULL);
            if (r == SOCKET_ERROR && WSAGetLastError() != WSA_IO_PENDING) {
                closesocket(client);
                free(ctx);
            }
        }
    }
}

int main() {
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        //printf("[!] WSAStartup failed\n");
        return 1;
    }

    struct addrinfo hints = { 0 }, *res = NULL;
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;
    hints.ai_flags = AI_PASSIVE;

    if (getaddrinfo(NULL, PORT, &hints, &res) != 0) {
        //printf("[!] getaddrinfo failed\n");
        WSACleanup();
        return 1;
    }

    SOCKET listenSock = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if (listenSock == INVALID_SOCKET) {
        //printf("[!] Failed to create socket\n");
        freeaddrinfo(res);
        WSACleanup();
        return 1;
    }

    // Optional: Set SO_REUSEADDR to make quick restarts easier
    BOOL opt = TRUE;
    setsockopt(listenSock, SOL_SOCKET, SO_REUSEADDR, (const char*)&opt, sizeof(opt));

    if (bind(listenSock, res->ai_addr, (int)res->ai_addrlen) == SOCKET_ERROR) {
        //printf("[!] Bind failed: %d\n", WSAGetLastError());
        closesocket(listenSock);
        freeaddrinfo(res);
        WSACleanup();
        return 1;
    }

    freeaddrinfo(res);

    if (listen(listenSock, SOMAXCONN) == SOCKET_ERROR) {
        //printf("[!] Listen failed: %d\n", WSAGetLastError());
        closesocket(listenSock);
        WSACleanup();
        return 1;
    }

    HANDLE iocp = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 0);
    if (iocp == NULL) {
        //printf("[!] Failed to create IOCP\n");
        closesocket(listenSock);
        WSACleanup();
        return 1;
    }

    for (int i = 0; i < WORKER_THREADS; i++) {
        HANDLE hThread = CreateThread(NULL, 0, WorkerThread, iocp, 0, NULL);
        if (hThread == NULL) {
            //printf("[!] Failed to create worker thread\n");
            closesocket(listenSock);
            CloseHandle(iocp);
            WSACleanup();
            return 1;
        }
        CloseHandle(hThread); // We don't need the thread handle
    }

    printf("[+] Listening on http://localhost:%s/ ...\n", PORT);

    while (1) {
        SOCKET client = accept(listenSock, NULL, NULL);
        if (client == INVALID_SOCKET) {
            printf("[!] Accept failed: %d\n", WSAGetLastError());
            continue;
        }

        if (CreateIoCompletionPort((HANDLE)client, iocp, (ULONG_PTR)client, 0) == NULL) {
            //printf("[!] Failed to associate client socket with IOCP\n");
            closesocket(client);
            continue;
        }

        PER_IO_CONTEXT* ctx = (PER_IO_CONTEXT*)calloc(1, sizeof(PER_IO_CONTEXT));
        if (!ctx) {
            //printf("[!] Failed to allocate PER_IO_CONTEXT\n");
            closesocket(client);
            continue;
        }

        ctx->socket = client;
        ctx->wsaBuf.buf = ctx->buffer;
        ctx->wsaBuf.len = sizeof(ctx->buffer);
        ctx->isSend = FALSE;
        ZeroMemory(&ctx->overlapped, sizeof(OVERLAPPED));

        DWORD flags = 0;
        int r = WSARecv(client, &ctx->wsaBuf, 1, NULL, &flags, &ctx->overlapped, NULL);
        if (r == SOCKET_ERROR && WSAGetLastError() != WSA_IO_PENDING) {
            //printf("[!] Initial WSARecv failed: %d\n", WSAGetLastError());
            closesocket(client);
            free(ctx);
        }
    }

    closesocket(listenSock);
    CloseHandle(iocp);
    WSACleanup();
    return 0;
}

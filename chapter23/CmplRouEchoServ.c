#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <winsock2.h>

#define BUF_SIZE 50
void errorHandling(char *message);
void CALLBACK readCompletionRoutine(DWORD dwError, DWORD cbTransferred, LPWSAOVERLAPPED lpOverlapped, DWORD dwFlags);
void CALLBACK writeCompletionRoutine(DWORD dwError, DWORD cbTransferred, LPWSAOVERLAPPED lpOverlapped, DWORD dwFlags);

struct AioArgument {
    SOCKET sock;
    // malloc will allocate correct BUF_SIZE size memory, not pointer size
    char buf[BUF_SIZE];
    WSABUF wsaBuf;
};

int main(int argc, char *argv[]) {
    if (argc != 2) {
        argv[1] = "9190";
    }
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        errorHandling("WSAStartup() error!");
    }
    SOCKET serverSock = WSASocket(PF_INET, SOCK_STREAM, 0, NULL, 0, WSA_FLAG_OVERLAPPED); // Overlapped I/O socket
    unsigned long mode = 1;
    ioctlsocket(serverSock, FIONBIO, &mode); // non-blocking socket
    struct sockaddr_in serverAddr;
    memset(&serverAddr, 0, sizeof(serverAddr));
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = htonl(INADDR_ANY);
    serverAddr.sin_port = htons(atoi(argv[1]));
    int option = 1;
    setsockopt(serverSock, SOL_SOCKET, SO_REUSEADDR, (char *)&option, sizeof(option));
    if (bind(serverSock, (struct sockaddr*)&serverAddr, sizeof(serverAddr)) == -1) {
        errorHandling("bind() error");
    }
    if (listen(serverSock, 5) == -1) {
        errorHandling("listen() error");
    }
    while (1) {
        /* Need to call functions like SleepEx() to get the thread into alertable wait status, and then the completion routine function
        can be called (windows' rule).
        A value of zero (dwMilliseconds) causes the thread to relinquish the remainder of its time slice to any other thread that is ready to run. */
        SleepEx(1, TRUE);
        int addr_size = sizeof(serverAddr);
        // serverSock is nonblocking, and then accept() is nonblocking, return nonblocking socket
        SOCKET clientSock = accept(serverSock, (struct sockaddr *)&serverAddr, &addr_size);
        if (clientSock == INVALID_SOCKET) {
            if (WSAGetLastError() == WSAEWOULDBLOCK) {
                continue;
            }
            else {
                errorHandling("accept() error");
            }
        }
        struct AioArgument *aioArgument = (struct AioArgument *)malloc(sizeof(struct AioArgument));
        memset(aioArgument, 0, sizeof(struct AioArgument));
        aioArgument->sock = clientSock;
        aioArgument->wsaBuf.buf = aioArgument->buf;
        aioArgument->wsaBuf.len = BUF_SIZE;
        LPWSAOVERLAPPED lpOverlapped = (LPWSAOVERLAPPED)malloc(sizeof(WSAOVERLAPPED));
        memset(lpOverlapped, 0, sizeof(WSAOVERLAPPED));
        // overlapped I/O, confirm completion by completion routine callback, lpOverlapped->hEvent unused, so use it to pass information for the callback function
        lpOverlapped->hEvent = aioArgument;
        DWORD bytesRecvd;
        DWORD flags = 0;
        /* WSARecv() and WSASend() is not always asynchronous, it may return immediately if the data transfer can be done immediately (for example, the data
        to be sent is little). In this situation, lpNumberOfBytesRecvd/lpNumberOfBytesSent points to byte number received/sent. */
        WSARecv(aioArgument->sock, &aioArgument->wsaBuf, 1, &bytesRecvd, &flags, lpOverlapped, readCompletionRoutine);
    }
}

void errorHandling(char *message) {
    fputs(message, stderr);
    fputc('\n', stderr);
    exit(1);
}

void CALLBACK readCompletionRoutine(DWORD dwError, DWORD cbTransferred, LPWSAOVERLAPPED lpOverlapped, DWORD dwFlags) {
    struct AioArgument *aioArgument = (struct AioArgument *)lpOverlapped->hEvent;
    if (cbTransferred == 0) { // close connection
        printf("close connection\n");
        closesocket(aioArgument->sock);
        free(aioArgument);
        free(lpOverlapped);
    }
    else {
        DWORD bytesSent;
        DWORD flags = 0;
        // received data length indicated by cbTransferred, assign it to wsaBuf.len and echo back
        aioArgument->wsaBuf.len = cbTransferred;
        WSASend(aioArgument->sock, &aioArgument->wsaBuf, 1, &bytesSent, flags, lpOverlapped, writeCompletionRoutine);
    }
}

void CALLBACK writeCompletionRoutine(DWORD dwError, DWORD cbTransferred, LPWSAOVERLAPPED lpOverlapped, DWORD dwFlags) {
    struct AioArgument *aioArgument = (struct AioArgument *)lpOverlapped->hEvent;
    DWORD bytesRecvd;
    DWORD flags = 0;
    WSARecv(aioArgument->sock, &aioArgument->wsaBuf, 1, &bytesRecvd, &flags, lpOverlapped, readCompletionRoutine);
}
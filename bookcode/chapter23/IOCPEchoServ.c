#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <winsock2.h>
#include <process.h>

#define BUF_SIZE 50
#define READ 1
#define	WRITE 2

struct AioArgument {
    OVERLAPPED overlapped;
    char buf[BUF_SIZE];
    WSABUF wsaBuf;
    // unlike CmplRouEchoServ_win.c using two callback, need to indentify whether the finish is obtained from WSASend()/WSARecv()
    int rwMode;
};

unsigned WINAPI echoThreadRun(void *completionPort);
void errorHandling(char *message);

int main(int argc, char *argv[]) {
    if (argc != 2) {
        argv[1] = "9190";
    }
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        errorHandling("WSAStartup() error!");
    }
    SOCKET serverSock = WSASocket(PF_INET, SOCK_STREAM, 0, NULL, 0, WSA_FLAG_OVERLAPPED);
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
    SYSTEM_INFO systemInfo;
    GetSystemInfo(&systemInfo);
    /* create completion port object. NumberOfConcurrentThreads indicates the maximum number of threads that the operating system
    can allow to concurrently process I/O completion packets for the I/O completion port.If NumberOfConcurrentThreads is zero, the
    system allows as many concurrently running threads as there are processors in the system. */
    HANDLE completionPort = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 0);
    for (int i = 0; i < systemInfo.dwNumberOfProcessors; i++) {
        _beginthreadex(NULL, 0, echoThreadRun, (void *)completionPort, 0, NULL);
    }
    while (1) {
        SOCKET clientSock = accept(serverSock, NULL, NULL);
        // The fourth argument will be ignored as long as the second argument is not NULL.
        SOCKET *clientSockPtr = (SOCKET *)malloc(sizeof(SOCKET));
        (*clientSockPtr) = clientSock;
        CreateIoCompletionPort((HANDLE)clientSock, completionPort, (ULONG_PTR)clientSockPtr, 0);
        struct AioArgument *aioArgument = (struct AioArgument *)malloc(sizeof(struct AioArgument));
        memset(aioArgument, 0, sizeof(struct AioArgument));
        aioArgument->wsaBuf.buf = aioArgument->buf;
        aioArgument->wsaBuf.len = BUF_SIZE;
        aioArgument->rwMode = READ;
        DWORD bytesRecvd;
        DWORD flags = 0;
        // The overlapped argument can also pass aioArgument, but &aioArgument->overlapped is more understandable.
        WSARecv(clientSock, &aioArgument->wsaBuf, 1, &bytesRecvd, &flags, &aioArgument->overlapped, NULL);
    }
    return 0;
}

unsigned WINAPI echoThreadRun(void *completionPort) {
    DWORD numberOfBytes;
    struct AioArgument *aioArgument;
    LPOVERLAPPED lpOverlapped;
    SOCKET *clientSockPtr;
    while (1) {
        /* The third argument will receive CreateIoCompletionPort(clientSock, completionPort, aioArgument, 0)'s third argument.
        The fourth argument will receive WSASend()/WSARecv()'s lpOverlapped argument. */
        GetQueuedCompletionStatus(completionPort, &numberOfBytes, (PULONG_PTR)&clientSockPtr, &lpOverlapped, INFINITE);
        SOCKET sock = *clientSockPtr;
        aioArgument = (struct AioArgument *)lpOverlapped;
        if (aioArgument->rwMode == READ) {
            if (numberOfBytes == 0) {
                struct sockaddr_in clientAddr;
                int addrSize;
                getpeername(sock, (struct sockaddr *)&clientAddr, &addrSize);
                printf("client : %s:%d disconnected\n", inet_ntoa(clientAddr.sin_addr), clientAddr.sin_port);
                closesocket(sock);
                free(aioArgument);
                free(clientSockPtr);
            }
            else {
                // send received data
                memset(lpOverlapped, 0, sizeof(OVERLAPPED));
                aioArgument->wsaBuf.len = numberOfBytes;
                aioArgument->rwMode = WRITE;
                DWORD bytesSent;
                WSASend(sock, &aioArgument->wsaBuf, 1, &bytesSent, 0, lpOverlapped, NULL);

                /* next asynchronous receive, the buffer can't be reused for this WSARecv(). Since WSASend() and WSARecv()
                are asynchronous, the buffer of WSASend() might be still in use when this WSARecv() is called.*/
                struct AioArgument *aioArgument = (struct AioArgument *)malloc(sizeof(struct AioArgument));
                memset(aioArgument, 0, sizeof(struct AioArgument));
                aioArgument->wsaBuf.buf = aioArgument->buf;
                aioArgument->wsaBuf.len = BUF_SIZE;
                aioArgument->rwMode = READ;
                DWORD bytesRecvd;
                DWORD flags = 0;
                // The overlapped argument can also pass aioArgument, but &aioArgument->overlapped is more understandable.
                WSARecv(sock, &aioArgument->wsaBuf, 1, &bytesRecvd, &flags, &aioArgument->overlapped, NULL);
            }
        }
        else {
            fprintf(stdout, "send over\n");
            free(aioArgument);
        }
    }
    return 0;
}

void errorHandling(char *message) {
    fputs(message, stderr);
    fputc('\n', stderr);
    exit(1);
}
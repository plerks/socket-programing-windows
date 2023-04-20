#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <winsock2.h>

#define BUF_SIZE 50
void errorHandling(char *message);
void compressSocks(SOCKET *socks, int sockCount, int index);
void compressEvents(WSAEVENT *socks, int sockCount, int index);

int main(int argc, char *argv[])
{
    if (argc != 2) {
        argv[1] = "9190";
    }
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        errorHandling("WSAStartup() error!");
    }
    SOCKET serverSock = socket(PF_INET, SOCK_STREAM, 0);
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
    int sockCount = 0;
    SOCKET socks[WSA_MAXIMUM_WAIT_EVENTS];
    WSAEVENT events[WSA_MAXIMUM_WAIT_EVENTS];
    socks[sockCount] = serverSock;
    events[sockCount] = WSACreateEvent(); // WSACreateEvent() creates manual-reset mode, non-signaled status event
    if (WSAEventSelect(serverSock, events[sockCount], FD_ACCEPT) == SOCKET_ERROR) {
        errorHandling("WSAEventSelect() error");
    }
    sockCount++;
    while (1) {
        // WSAWaitForMultipleEvents() with dwTimeout==infinite blocks and returns when signaled, thus similar to epoll_wait() in effect
        int startIndex = WSAWaitForMultipleEvents(sockCount, events, FALSE, WSA_INFINITE, FALSE) - WSA_WAIT_EVENT_0;
        for (int i = startIndex; i < sockCount; i++) {
            // dwTimeout==0, return immediately
            int status = WSAWaitForMultipleEvents(1, events, FALSE, 0, FALSE);
            if (status == WSA_WAIT_FAILED && status == WSA_WAIT_TIMEOUT) {
                continue;
            }
            else {
                WSANETWORKEVENTS networkEvents;
                WSAEnumNetworkEvents(socks[i], events[i], &networkEvents);
                if (networkEvents.lNetworkEvents & FD_ACCEPT) {
                    if (networkEvents.iErrorCode[FD_ACCEPT_BIT] != 0) {
                        errorHandling("accept error");
                        break;
                    }
                    int addrSize = sizeof(serverAddr);
                    SOCKET clientSock = accept(serverSock, (struct sockaddr *)&serverAddr, &addrSize);
                    socks[sockCount] = clientSock;
                    events[sockCount] = WSACreateEvent();
                    /* Microsoft's doc: The WSAEventSelect function automatically sets socket s to nonblocking mode,
                    regardless of the value of lNetworkEvents. */
                    // WSAEventSelect() is asynchronous, the actual socket change is checked by WSAWaitForMultipleEvents().
                    if (WSAEventSelect(clientSock, events[sockCount], FD_READ | FD_CLOSE) == SOCKET_ERROR) {
                        errorHandling("WSAEventSelect() error");
                    }
                    sockCount++;
                }
                if (networkEvents.lNetworkEvents & FD_READ) {
                    if (networkEvents.iErrorCode[FD_READ_BIT] != 0) {
                        errorHandling("read error");
                        break;
                    }
                    char buf[BUF_SIZE];
                    printf("triggered\n");
                    int len = recv(socks[i], buf, BUF_SIZE, 0);
                    send(socks[i], buf, len, 0);
                }
                if (networkEvents.lNetworkEvents & FD_CLOSE) {
                    if (networkEvents.iErrorCode[FD_CLOSE_BIT] != 0) {
                        errorHandling("close error");
                        break;
                    }
                    WSACloseEvent(events[i]);
                    closesocket(socks[i]);
                    compressSocks(socks, sockCount, i);
                    compressEvents(events, sockCount, i);
                    sockCount--;
                }
            }
        }
    }
}

void compressSocks(SOCKET *socks, int sockCount, int index) {
    for (int i = index; i + 1 < sockCount; i++) {
        socks[i] = socks[i + 1];
    }
}

void compressEvents(WSAEVENT *events, int sockCount, int index) {
    for (int i = index; i + 1 < sockCount; i++) {
        events[i] = events[i + 1];
    }
}

void errorHandling(char *message) {
    fputs(message, stderr);
    fputc('\n', stderr);
    exit(1);
}
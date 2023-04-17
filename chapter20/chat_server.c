#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <winsock2.h>
#include <windows.h>
#include <process.h>

#define MAX_CLIENT 50
#define BUF_SIZE 50
SOCKET clientSocks[MAX_CLIENT];
int clientCount = 0;
HANDLE mutex;
unsigned __stdcall handleConnection(void *arg);
void sendMsg(char *buf, int len);
void errorHandling(char *message);

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
    mutex = CreateMutex(NULL, FALSE, NULL);
    struct sockaddr_in clientAddr;
    while (1) {
        int addrSize = sizeof(serverAddr);
        SOCKET clientSock = accept(serverSock, (struct sockaddr *)&clientAddr, &addrSize);
        WaitForSingleObject(mutex, INFINITE);
        clientSocks[clientCount++] = clientSock;
        ReleaseMutex(mutex);
        _beginthreadex(NULL, 0, handleConnection, (void *)&clientSock, 0, NULL);
    }
    closesocket(serverSock);
    WSACleanup();
    return 0;
}

unsigned __stdcall handleConnection(void *arg) {
    SOCKET clientSock = *((SOCKET *)arg);
    char buf[BUF_SIZE];
    int len;
    while ((len = recv(clientSock, buf, BUF_SIZE, 0)) != 0) {
        sendMsg(buf, len);
    }
    WaitForSingleObject(mutex, INFINITE);
    // remove disconnected client, client_sock can be optimized to hashset, but no default hashset in c lib
    for (int i = 0; i < clientCount; i++) {
        if (clientSocks[i] == clientSock) {
            for (int j = i; j < clientCount - 1;j++) {
                clientSocks[j] = clientSocks[j + 1];
            }
        }
    }
    clientCount--;
    ReleaseMutex(mutex);
    closesocket(clientSock);
    return 0;
}

void sendMsg(char *buf, int len) {
    WaitForSingleObject(mutex, INFINITE);
    for (int i = 0; i < clientCount; i++) {
        send(clientSocks[i], buf, len, 0);
    }
    ReleaseMutex(mutex);
}

void errorHandling(char *message) {
    fputs(message, stderr);
    fputc('\n', stderr);
    exit(1);
}
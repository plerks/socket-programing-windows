#include <stdio.h>
#include <stdlib.h>
#include <winsock.h>

void errorHandling(char *message);

int main(int argc, char *argv[]) {
    char* argvCopy[2];
    if (argc != 2) {
        argv = argvCopy;
        argv[1] = "9190";
    }

    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        errorHandling("WSAStartup() error!");
    }

    SOCKET serverSock = socket(PF_INET, SOCK_STREAM, 0);
    if (serverSock == -1) {
        errorHandling("socket() error");
    }
    struct sockaddr_in serverAddr;
    memset(&serverAddr, 0, sizeof(serverAddr));
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = htonl(INADDR_ANY);
    serverAddr.sin_port = htons(atoi(argv[1]));

    if (bind(serverSock, (struct sockaddr*) &serverAddr, sizeof(serverAddr)) == -1) {
        errorHandling("bind() error");
    }

    if (listen(serverSock, 5) == -1) {
        errorHandling("listen() error");
    }
    struct sockaddr_in clientAddr;
    int clientAddrSize = sizeof(clientAddr);
    SOCKET clientSock = accept(serverSock, (struct sockaddr*) &clientAddr, &clientAddrSize);
    if (clientSock == -1) {
        errorHandling("accept() error");
    }
    char message[] = "Hello World!";
    send(clientSock, message, sizeof(message), 0);
    closesocket(clientSock);
    closesocket(serverSock);
    return 0;
}

void errorHandling(char *message) {
    fputs(message, stderr);
    fputc('\n', stderr);
    exit(1);
}
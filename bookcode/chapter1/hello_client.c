#include <stdio.h>
#include <stdlib.h>
#include <winsock2.h>
void errorHandling(char *message);

int main(int argc, char* argv[]) {
    char* argvCopy[3];
    if (argc != 3) {
        argv = argvCopy;
        argv[1] = "127.0.0.1";
        argv[2] = "9190";
    }

    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        errorHandling("WSAStartup() error!");
    }

    SOCKET sock = socket(PF_INET, SOCK_STREAM, 0);
    if (sock == -1) {
        errorHandling("socket() error");
    }
    struct sockaddr_in serverAddr;
    memset(&serverAddr, 0, sizeof(serverAddr));
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = inet_addr(argv[1]);
    serverAddr.sin_port = htons(atoi(argv[2]));

    if (connect(sock, (struct sockaddr*)&serverAddr, sizeof(serverAddr)) == -1) {
        errorHandling("connect() error");
    }
    char message[30];
    /* just a demo client, if the server message is very long and can't be transported in one time,
    read() in this way won't get the entire message */
    int strLen = recv(sock, message, sizeof(message) - 1, 0);
    if (strLen == -1) {
        errorHandling("read() error");
    }
    printf("Message from server : %s \n", message);
    closesocket(sock);
    return 0;
}

void errorHandling(char *message) {
    fputs(message, stderr);
    fputc('\n', stderr);
    exit(1);
}
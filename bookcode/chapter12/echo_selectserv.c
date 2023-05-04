#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <winsock2.h>

#define BUF_SIZE 50
void errorHandling(char *message);

int main(int argc, char *argv[]) {
    // if not given a port number argument, the server serves at port 9190 by default
    if (argc != 2) {
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
    int option = 1;
    setsockopt(serverSock, SOL_SOCKET, SO_REUSEADDR, (char *)&option, sizeof(option));
    if (bind(serverSock, (struct sockaddr*)&serverAddr, sizeof(serverAddr)) == -1) {
        errorHandling("bind() error");
    }
    if (listen(serverSock, 5) == -1) {
        errorHandling("listen() error");
    }
    /* select() requires fd_set to indicate the fds wanted to be monitored. But once return, the given argument fd_set
    will be set as the monitor result status. So the former want-to-monitor status needs to be reserved in advance. */
    fd_set readset, cpyReadset;
    FD_ZERO(&readset);
    FD_SET(serverSock, &readset);
    int selectNum = 0;
    int maxfd = serverSock;
    struct timeval timeout;
    char buf[BUF_SIZE];
    while (1) {
        cpyReadset = readset;
        /* select() may update the timeout argument to indicate how much time was left (referring to linux man page).
        So must initiate timeout variable inner the loop, otherwise timeout will continue to occur after the first timeout. */
        timeout.tv_sec = 5;
        timeout.tv_usec = 0;
        selectNum = select(maxfd + 1, &cpyReadset, 0, 0, &timeout);
        if (selectNum == -1) {
            break;
        }
        if (selectNum == 0) { // timeout
            continue;
        }
        /* maxfd monotonically increase. It can be optimized, but since changedQuantity is used to control
        the loop time, the loop does't execute too much. */
        for(int i = STDERR_FILENO + 1, changedQuantity = 0; i <= maxfd && changedQuantity <= selectNum; i++) {
            if (FD_ISSET(i, &cpyReadset)) {
                changedQuantity += 1;
                if (i == serverSock) { // extract connection from serverSock
                    struct sockaddr_in clientAddr;
                    int addr_size = sizeof(clientAddr);
                    int client_sock = accept(serverSock, (struct sockaddr *)&clientAddr, &addr_size);
                    if (client_sock > maxfd) {
                        maxfd = client_sock;
                    }
                    FD_SET(client_sock, &readset);
                }
                else {
                    int len = recv(i, buf, BUF_SIZE, 0);
                    if (len == 0) {
                        FD_CLR(i, &readset);
                        printf("client %d closed connection \n", i);
                        closesocket(i);
                    }
                    else if (len > 0) {
                        send(i, buf, len, 0);
                    }
                }
            }
        }
    }
    closesocket(serverSock);
    return 0;
}

void errorHandling(char *message) {
    fputs(message, stderr);
    fputc('\n', stderr);
    exit(1);
}
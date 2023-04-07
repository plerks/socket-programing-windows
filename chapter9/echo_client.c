#include <stdio.h>
#include <winsock2.h>

#define BUF_SIZE 100
void errorHandling(char *message);

int main(int argc, char *argv[]) {
    char* argvCopy[2];
    if (argc != 2) {
        argv = argvCopy;
        argvCopy[1] = "127.0.0.1";
        argvCopy[2] = "9190";
    }
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        errorHandling("WSAStartup() error!");
    }
    struct sockaddr_in serverAddr;
    memset(&serverAddr, 0, sizeof(serverAddr));
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = inet_addr(argv[1]);
    serverAddr.sin_port = htons(atoi(argv[2]));
    int sock = socket(PF_INET, SOCK_STREAM, 0);
    if (connect(sock, (struct sockaddr *)&serverAddr, sizeof(serverAddr)) != 0) {
        errorHandling("connect() error");
    }
    char buf[BUF_SIZE];
    fputs("Type to get the echo, Q/q to exit:\n", stdout);
    while (1) {
        fgets(buf, BUF_SIZE, stdin);
        if (!strcmp(buf, "Q\n") || !strcmp(buf, "q\n")) {
            break;
        }
        send(sock, buf, strlen(buf), 0);
        /* Read at most BUF_SIZE-1 chars and remove the last '\n'. The original book here is BUF_SIZE
        and set buf[len] = '\0', which may lead to out of range. */
        int len = recv(sock, buf, BUF_SIZE - 1, 0);
        buf[len] = '\0';
        if (len > 0 && buf[len - 1] == '\n') {
            buf[len - 1] = '\0';
        }
        if (!(len > 0)) {
            break;
        }
        /* If run the code with VSCode Microsoft C/C++ plugin, it opens a powershell terminal with UTF-8 encoding (`chcp` outputs 65001) and
        when echoing Chinese character, the echo gets grabled. Need to run `chcp 936` in advance to set encoding to GBK and run with command
        line. I guess the typing tool gives process GBK encoded bytes since windows is GBK by default, so it's needed to set terminal encoding
        to be GBK to display Chinese character.
        By the way, VSCode header -> Terminal -> New Terminal, run `chcp` and you can see terminal opened in this way is encoded with GBK,
        and git bash did't happen grabling without special operation. */
        printf("Message from server: %s\n", buf);
    }
    closesocket(sock);
    return 0;
}

void errorHandling(char *message) {
    fputs(message, stderr);
    fputc('\n', stderr);
    exit(1);
}
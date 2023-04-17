#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <winsock2.h>
#include <windows.h>
#include <process.h>
#include <time.h>

// undef max to avoid using the max(a, b) macro
#undef max

#define MAX_CLIENT 50
#define BUF_SIZE 50
#define NAME_ARR_SIZE 6
#define MAX_SENTENCE_HEADER_SIZE 50
char name[NAME_ARR_SIZE];
char *getRandomName(char *name);
unsigned __stdcall sendMsg(void *arg);
unsigned __stdcall recvMsg(void *arg);
void errorHandling(char *message);
int max(int a, int b);

int main(int argc, char *argv[])
{
    if (argc != 3) {
        argv[1] = "127.0.0.1";
        argv[2] = "9190";
    }
    getRandomName(name);
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        errorHandling("WSAStartup() error!");
    }
    struct sockaddr_in serverAddr;
    memset(&serverAddr, 0, sizeof(serverAddr));
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = inet_addr(argv[1]);
    serverAddr.sin_port = htons(atoi(argv[2]));
    SOCKET sock = socket(PF_INET, SOCK_STREAM, 0);
    if (connect(sock, (struct sockaddr *)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR) {
        errorHandling("connect() error");
    }
    fputs("enter message, Q/q to quit:\n", stdout);
    HANDLE sendThread = (HANDLE)_beginthreadex(NULL, 0, sendMsg, (void *)&sock, 0, NULL);
    HANDLE recvThread = (HANDLE)_beginthreadex(NULL, 0, recvMsg, (void *)&sock, 0, NULL);
    WaitForSingleObject(sendThread, INFINITE);
    WaitForSingleObject(recvThread, INFINITE);
    closesocket(sock);
    WSACleanup();
    return 0;
}

char *getRandomName(char *name) {
    srand((unsigned)time(NULL));
    int i;
    for (i = 0; i < max(3, rand() % NAME_ARR_SIZE); i++) {
        name[i] = (rand() % ('z' - 'a' + 1)) + 'a';
    }
    name[i] = '\0';
    return name;
}

/* The routine at start_address that's passed to _beginthreadex must use the __stdcall (for native code) or __clrcall (for managed code)
calling convention and must return a thread exit code.

see: https://learn.microsoft.com/en-us/cpp/c-runtime-library/reference/beginthread-beginthreadex?view=msvc-170#remarks */
unsigned __stdcall sendMsg(void *arg) {
    SOCKET sock = *((SOCKET *)arg);
    char msg[BUF_SIZE];
    while (1) {
        fgets(msg, BUF_SIZE, stdin);
        if (!strcmp(msg, "Q\n") || !strcmp(msg, "q\n")) {
            closesocket(sock);
            exit(0);
        }
        char nameMsg[MAX_SENTENCE_HEADER_SIZE + BUF_SIZE];
        sprintf(nameMsg, "[%s(random name)]'s message: %s", name, msg);
        int len = send(sock, nameMsg, strlen(nameMsg), 0);
    }
    return 0;
}

unsigned __stdcall recvMsg(void *arg) {
    int sock = *((SOCKET *)arg);
    char nameMsg[MAX_SENTENCE_HEADER_SIZE + BUF_SIZE];
    int len;
    while ((len = recv(sock, nameMsg, MAX_SENTENCE_HEADER_SIZE + BUF_SIZE - 1, 0)) > 0) {
        nameMsg[len] = '\0';
        fputs(nameMsg, stdout);
    }
    return 0;
}

int max(int a, int b) {
    return a > b ? a : b;
}

void errorHandling(char *message) {
    fputs(message, stderr);
    fputc('\n', stderr);
    exit(1);
}
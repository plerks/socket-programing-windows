#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <winsock2.h>
#include <process.h>
#include "Handlers.h"
#include "Server.h"

unsigned WINAPI threadRun(void *completionPort);
void errorHandling(char *message);
BOOL startWith(char *src, char *substr);
char *newString(char *buf, char *start, char *end);
int getContentLength(char *buf);
void *callProperHandler(struct Request *request, struct AioArgument *aioArgument);
void *defaultResonse(struct Request *request);
void *sendErrorPage(struct Request *request);
void getRequestMethodAndUrl(struct Request *request, char *requestMethod, char *url);
BOOL endsWith(char *src, char *subString);

void startServer(char *ip, int port) {
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        errorHandling("WSAStartup() error!");
    }
    SOCKET serverSock = WSASocket(PF_INET, SOCK_STREAM, 0, NULL, 0, WSA_FLAG_OVERLAPPED);
    struct sockaddr_in serverAddr;
    memset(&serverAddr, 0, sizeof(serverAddr));
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = htonl(inet_addr(ip));
    serverAddr.sin_port = htons(port);
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
        _beginthreadex(NULL, 0, threadRun, (void *)completionPort, 0, NULL);
    }
    while (1) {
        SOCKET clientSock = accept(serverSock, NULL, NULL);
        SOCKET *clientSockPtr = (SOCKET *)malloc(sizeof(SOCKET));
        (*clientSockPtr) = clientSock;
        // The fourth argument will be ignored as long as the second argument is not NULL.
        // associate the clientSock with the completionPort
        CreateIoCompletionPort((HANDLE)clientSock, completionPort, (ULONG_PTR)clientSockPtr, 0);
        struct AioArgument *aioArgument = (struct AioArgument *)malloc(sizeof(struct AioArgument));
        memset(aioArgument, 0, sizeof(struct AioArgument));
        aioArgument->wsaBuf.buf = aioArgument->buf;
        aioArgument->wsaBuf.len = BUF_SIZE;
        DWORD bytesRecvd;
        DWORD flags = 0;
        // The overlapped argument can also pass aioArgument, but &aioArgument->overlapped is more understandable.
        WSARecv(clientSock, &aioArgument->wsaBuf, 1, &bytesRecvd, &flags, &aioArgument->overlapped, NULL);
    }
}

unsigned WINAPI threadRun(void *completionPort) {
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
        aioArgument->wsaBuf.buf += numberOfBytes;
        aioArgument->wsaBuf.len -= numberOfBytes;
        aioArgument->totalBytes += numberOfBytes;
        if (strstr(aioArgument->buf, "\r\n\r\n") == NULL) { // The header part has not been fully received
            DWORD bytesRecvd;
            DWORD flags = 0;
            WSARecv(sock, &aioArgument->wsaBuf, 1, &bytesRecvd, &flags, &aioArgument->overlapped, NULL);
        }
        else { // The header part has been fully received
            char buf[MAX_HTTP_REQUEST_METHOD_LENGTH];
            struct Request request;
            memset(&request, 0, sizeof(request));
            request.sock = sock;
            request.requestLine = newString(aioArgument->buf, aioArgument->buf, strstr(aioArgument->buf, "\r\n") + strlen("\r\n"));
            request.headers = newString(aioArgument->buf, strstr(aioArgument->buf, "\r\n") + strlen("\r\n"), strstr(aioArgument->buf, "\r\n\r\n") + strlen("\r\n"));
            request.emptyLine = newString(aioArgument->buf, strstr(aioArgument->buf, "\r\n\r\n") + strlen("\r\n"), strstr(aioArgument->buf, "\r\n\r\n") + 2 * strlen("\r\n"));
            // If in callProperHandler() asynchronous I/O is called to recv/send data, need to add code to distinguish if the completion is from user code or server code.
            callProperHandler(&request, aioArgument);
            freeRequest(&request);
            closesocket(sock);
            /* `CreateIoCompletionPort((HANDLE)clientSock, completionPort, (ULONG_PTR)clientSockPtr, 0)` has associated the socket with the completion port object
            created by `CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 0)`. About whether it's needed to disassociate, see the Microsoft doc:
            <https://learn.microsoft.com/en-us/windows/win32/fileio/createiocompletionport>
            "A handle can be associated with only one I/O completion port, and after the association is made, the handle remains associated with that I/O completion port until it is closed."
            Inferring from the words, the association is removed when the socket is closed.

            And a similar question on stackoverflow <https://stackoverflow.com/questions/6573218/removing-a-handle-from-a-i-o-completion-port-and-other-questions-about-iocp>,
            An answer mentioned that: "It is not necessary to explicitly disassociate a handle from an I/O completion port; closing the handle is sufficient."
            */
            free(aioArgument);
            free(clientSockPtr);
        }
    }
    return 0;
}

void errorHandling(char *message) {
    fputs(message, stderr);
    fputc('\n', stderr);
    exit(1);
}

BOOL startWith(char *src, char *substr) {
    for (int i = 0; i < strlen(substr); i++) {
        if (src[i] != substr[i]) {
            return FALSE;
        }
    }
    return TRUE;
}

int getContentLength(char *buf) {
    char *s = strstr(buf, "Content-Length");
    if (s == NULL) {
        return 0;
    }
    s = strstr(s, ":");
    char numString[50] = {0};
    s = s + 1;
    for (int i = 0; s[i] != '\r'; i++) {
        numString[i] = s[i];
    }
    return atoi(numString);
}

// construct a heap String of buf[start, end)
char *newString(char *buf, char *start, char *end) {
    int length = end -start;
    char *s = (char *)malloc(length);
    memset(s, 0 , length);
    for (int i = 0; i < length; i++) {
        s[i] = *(start + i);
    }
    return s;
}

void *callProperHandler(struct Request *request, struct AioArgument *aioArgument) {
    char requestMethod[MAX_HTTP_REQUEST_METHOD_LENGTH];
    char url[URL_LENGTH];
    memset(requestMethod, 0, MAX_HTTP_REQUEST_METHOD_LENGTH);
    memset(url, 0, URL_LENGTH);
    getRequestMethodAndUrl(request, requestMethod, url);
    for (int i = 0; i < handler_num; i++) {
        if (!strcmp(requestMethod, handlers[i]->requestMethod) && !strcmp(url, handlers[i]->url)) {
            return handlers[i]->handle(request, aioArgument);
        }
    }
    return defaultResonse(request);
}

// no programer-defined logic found, try to match static files (could be optimized to cache the file content in memory for efficiency)
void *defaultResonse(struct Request *request) {
    char requestMethod[MAX_HTTP_REQUEST_METHOD_LENGTH];
    char url[URL_LENGTH];
    memset(requestMethod, 0, MAX_HTTP_REQUEST_METHOD_LENGTH);
    memset(url, 0, URL_LENGTH);
    getRequestMethodAndUrl(request, requestMethod, url);
    char path[URL_LENGTH] = {0};
    strcpy(path, "resources");
    strcat(path, url);
    FILE *file;
    if ((file = fopen(path, "r")) == NULL) {
        return sendErrorPage(request);
    }
    else {
        SOCKET sock = request->sock;
        char protocol[] = "HTTP/1.0 200 OK\r\n";
        char serverName[] = "Server: simple web server\r\n";
        char *contentType;
        if (endsWith(url, ".html")) {
            contentType = "Content-type: text/html\r\n";
        }
        if (endsWith(url, ".css")) {
            contentType = "Content-type: text/css\r\n";
        }
        if (endsWith(url, ".js")) {
            contentType = "Content-type: application/javascript\r\n";
        }
        char emptyLine[] = "\r\n";
        send(sock, protocol, strlen(protocol), 0);
        send(sock, serverName, strlen(serverName), 0);
        send(sock, contentType, strlen(contentType), 0);
        send(sock, emptyLine, strlen(emptyLine), 0);
        char buf[BUF_SIZE];
        while (fgets(buf, BUF_SIZE, file) != NULL) {
            send(sock, buf, strlen(buf), 0);
        }

    }
}

void *sendErrorPage(struct Request *request) {
    char requestMethod[MAX_HTTP_REQUEST_METHOD_LENGTH];
    char url[URL_LENGTH];
    memset(requestMethod, 0, MAX_HTTP_REQUEST_METHOD_LENGTH);
    memset(url, 0, URL_LENGTH);
    getRequestMethodAndUrl(request, requestMethod, url);
    char path[URL_LENGTH] = {"resources/errorpage.html"};
    FILE *file = fopen(path, "r");
    SOCKET sock = request->sock;
    char protocol[] = "HTTP/1.0 400 Bad Request\r\n";
    char serverName[] = "Server: simple web server\r\n";
    char contentType[] = "Content-type: text/html\r\n";
    char emptyLine[] = "\r\n";
    send(sock, protocol, strlen(protocol), 0);
    send(sock, serverName, strlen(serverName), 0);
    send(sock, contentType, strlen(contentType), 0);
    send(sock, emptyLine, strlen(emptyLine), 0);
    char buf[BUF_SIZE];
    while (fgets(buf, BUF_SIZE, file) != NULL) {
        send(sock, buf, strlen(buf), 0);
    }
}

void getRequestMethodAndUrl(struct Request *request, char *requestMethod, char *url) {
    char *s = request->requestLine;
    for (int i = 0; (*s) != ' ';i++) {
        requestMethod[i] = *s;
        s++;
    }
    s++;
    for (int i = 0; (*s) != ' ';i++) {
        url[i] = *s;
        s++;
    }
}

BOOL endsWith(char *src, char *subString) {
    if (strlen(src) < strlen(subString)) {
        return FALSE;
    }
    else {
        for (int i = 0; strlen(subString) - i > 0; i++) {
            if (src[strlen(src) - i] != subString[strlen(subString) -i]) {
                return FALSE;
            }
        }
        return TRUE;
    }
}
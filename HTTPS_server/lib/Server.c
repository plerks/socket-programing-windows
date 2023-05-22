#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <winsock2.h>
#include <process.h>
#include "Handlers.h"
#include "Server.h"
#include <openssl/ssl.h>
#include <openssl/err.h>

unsigned WINAPI threadRun(void *completionPort);
void errorHandling(char *message);
BOOL startWith(char *src, char *substr);
char *newString(char *buf, char *start, char *end);
int getContentLength(char *buf);
void *callProperHandler(struct Request *request, struct AioArgument *aioArgument);
void *defaultResonse(struct Request *request, struct AioArgument *aioArgument);
void *sendErrorPage(struct Request *request, struct AioArgument *aioArgument);
void getRequestMethodAndUrl(struct Request *request, char *requestMethod, char *url);
BOOL endsWith(char *src, char *subString);
SSL_CTX *create_context();
void configure_context(SSL_CTX *ctx);
struct AioArgument *newAioArgument(SSL *ssl, SOCKET sock);
void freeAioArgument(struct AioArgument *aioArgument);
void My_SSL_write_and_send(struct AioArgument *aioArgument, char *s, int len);

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
    /* create completion port object. If NumberOfConcurrentThreads is zero, the system allows as many concurrently running 
    threads as there are processors in the system. */
    HANDLE completionPort = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 0);
    for (int i = 0; i < systemInfo.dwNumberOfProcessors; i++) {
        _beginthreadex(NULL, 0, threadRun, (void *)completionPort, 0, NULL);
    }
    SSL_CTX *ctx;
    ctx = create_context();
    configure_context(ctx);
    while (1) {
        SOCKET clientSock = accept(serverSock, NULL, NULL);
        // treat SSL* as the effective equivalent of TCP socket file descriptor
        SSL *ssl = SSL_new(ctx);
        SSL_set_fd(ssl, clientSock);
        SSL_CTX_set_verify(ctx, SSL_VERIFY_NONE, NULL);
        /* <https://www.openssl.org/docs/man3.0/man3/SSL_accept.html>
        SSL_accept() waits for a TLS/SSL client to initiate the TLS/SSL handshake.
        The communication channel must already have been set and assigned to the ssl by setting an underlying BIO.
        */
        if (SSL_accept(ssl) <= 0) {
            // I'm using self-signed cert, so that's probably why SSL_accept() will error
            // printf("SSL_accept error\n");
        }
        SOCKET *clientSockPtr = (SOCKET *)malloc(sizeof(SOCKET));
        (*clientSockPtr) = clientSock;
        // The fourth argument will be ignored as long as the second argument is not NULL.
        // associate the clientSock with the completionPort
        CreateIoCompletionPort((HANDLE)clientSock, completionPort, (ULONG_PTR)clientSockPtr, 0);
        struct AioArgument *aioArgument = newAioArgument(ssl, clientSock);
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
    int n = 0;
    while (1) {
        /* The third argument will receive CreateIoCompletionPort(clientSock, completionPort, aioArgument, 0)'s third argument.
        The fourth argument will receive WSASend()/WSARecv()'s lpOverlapped argument. */
        GetQueuedCompletionStatus(completionPort, &numberOfBytes, (PULONG_PTR)&clientSockPtr, &lpOverlapped, INFINITE);
        SOCKET sock = *clientSockPtr;
        aioArgument = (struct AioArgument *)lpOverlapped;
        // WSARecv() get encrypted data, use BIO_write() to write into buf and use SSL_read() to get decrypted data
        BIO_write(aioArgument->rbio, aioArgument->wsaBuf.buf, numberOfBytes);
        aioArgument->decryptedTotalBytes += SSL_read(aioArgument->ssl, aioArgument->decryptedBuf + aioArgument->decryptedTotalBytes, BUF_SIZE);
        aioArgument->wsaBuf.buf += numberOfBytes;
        aioArgument->wsaBuf.len -= numberOfBytes;
        aioArgument->totalBytes += numberOfBytes;
        if (strstr(aioArgument->decryptedBuf, "\r\n\r\n") == NULL) { // The header part has not been fully received
            DWORD bytesRecvd;
            DWORD flags = 0;
            WSARecv(sock, &aioArgument->wsaBuf, 1, &bytesRecvd, &flags, &aioArgument->overlapped, NULL);
        }
        else { // The header part has been fully received
            char buf[MAX_HTTP_REQUEST_METHOD_LENGTH];
            struct Request request;
            memset(&request, 0, sizeof(request));
            request.sock = sock;
            request.requestLine = newString(aioArgument->decryptedBuf, aioArgument->decryptedBuf, strstr(aioArgument->decryptedBuf, "\r\n") + strlen("\r\n"));
            request.headers = newString(aioArgument->decryptedBuf, strstr(aioArgument->decryptedBuf, "\r\n") + strlen("\r\n"), strstr(aioArgument->decryptedBuf, "\r\n\r\n") + strlen("\r\n"));
            request.emptyLine = newString(aioArgument->decryptedBuf, strstr(aioArgument->decryptedBuf, "\r\n\r\n") + strlen("\r\n"), strstr(aioArgument->decryptedBuf, "\r\n\r\n") + 2 * strlen("\r\n"));
            // If in callProperHandler() asynchronous I/O is called to recv/send data, need to add code to distinguish if the completion is from user code or server code.
            callProperHandler(&request, aioArgument);
            freeRequest(&request);
            freeAioArgument(aioArgument);
            /* `CreateIoCompletionPort((HANDLE)clientSock, completionPort, (ULONG_PTR)clientSockPtr, 0)` has associated the socket with the completion port object
            created by `CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 0)`. About whether it's needed to disassociate, see the Microsoft doc:
            <https://learn.microsoft.com/en-us/windows/win32/fileio/createiocompletionport>
            "A handle can be associated with only one I/O completion port, and after the association is made, the handle remains associated with that I/O completion port until it is closed."
            Inferring from the words, the association is removed when the socket is closed.

            And a similar question on stackoverflow <https://stackoverflow.com/questions/6573218/removing-a-handle-from-a-i-o-completion-port-and-other-questions-about-iocp>,
            An answer mentioned that: "It is not necessary to explicitly disassociate a handle from an I/O completion port; closing the handle is sufficient."
            */
            closesocket(sock);
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
    int length = end - start;
    char *s = (char *)malloc(length);
    memset(s, 0, length);
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
    return defaultResonse(request, aioArgument);
}

// no programer-defined logic found, try to match static files (could be optimized to cache the file content in memory for efficiency)
void *defaultResonse(struct Request *request, struct AioArgument *aioArgument) {
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
        return sendErrorPage(request, aioArgument);
    }
    else {
        SSL *ssl = aioArgument->ssl;
        char protocol[] = "HTTP/1.0 200 OK\r\n";
        char serverName[] = "Server: simple web server\r\n";
        char *contentType;
        if (endsWith(url, ".html")) {
            contentType = "Content-type: text/html; charset=utf-8\r\n";
        }
        if (endsWith(url, ".css")) {
            contentType = "Content-type: text/css; charset=utf-8\r\n";
        }
        if (endsWith(url, ".js")) {
            contentType = "Content-type: application/javascript; charset=utf-8\r\n";
        }
        char emptyLine[] = "\r\n";
        My_SSL_write_and_send(aioArgument, protocol, strlen(protocol));
        My_SSL_write_and_send(aioArgument, serverName, strlen(serverName));
        My_SSL_write_and_send(aioArgument, contentType, strlen(contentType));
        My_SSL_write_and_send(aioArgument, emptyLine, strlen(emptyLine));
        char buf[BUF_SIZE];
        int len = 0;
        while (fgets(buf, BUF_SIZE, file) != NULL) {
            My_SSL_write_and_send(aioArgument, buf, strlen(buf));
        }

    }
}

void *sendErrorPage(struct Request *request, struct AioArgument *aioArgument) {
    char requestMethod[MAX_HTTP_REQUEST_METHOD_LENGTH];
    char url[URL_LENGTH];
    memset(requestMethod, 0, MAX_HTTP_REQUEST_METHOD_LENGTH);
    memset(url, 0, URL_LENGTH);
    getRequestMethodAndUrl(request, requestMethod, url);
    char path[URL_LENGTH] = {"resources/errorpage.html"};
    FILE *file = fopen(path, "r");
    SSL *ssl = aioArgument->ssl;
    char protocol[] = "HTTP/1.0 400 Bad Request\r\n";
    char serverName[] = "Server: simple web server\r\n";
    char contentType[] = "Content-type: text/html; charset=utf-8\r\n";
    char emptyLine[] = "\r\n";
    My_SSL_write_and_send(aioArgument, protocol, strlen(protocol));
    My_SSL_write_and_send(aioArgument, serverName, strlen(serverName));
    My_SSL_write_and_send(aioArgument, contentType, strlen(contentType));
    My_SSL_write_and_send(aioArgument, emptyLine, strlen(emptyLine));
    char buf[BUF_SIZE];
    while (fgets(buf, BUF_SIZE, file) != NULL) {
        My_SSL_write_and_send(aioArgument, buf, strlen(buf));
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

// from https://wiki.openssl.org/index.php/Simple_TLS_Server
SSL_CTX *create_context()
{
    const SSL_METHOD *method;
    SSL_CTX *ctx;

    method = TLS_server_method();

    ctx = SSL_CTX_new(method);
    if (!ctx) {
        perror("Unable to create SSL context");
        ERR_print_errors_fp(stderr);
        exit(EXIT_FAILURE);
    }

    return ctx;
}

// from https://wiki.openssl.org/index.php/Simple_TLS_Server
void configure_context(SSL_CTX *ctx)
{
    /* Set the key and cert */
    if (SSL_CTX_use_certificate_file(ctx, "tls_files/server.crt", SSL_FILETYPE_PEM) <= 0) {
        ERR_print_errors_fp(stderr);
        exit(EXIT_FAILURE);
    }

    if (SSL_CTX_use_PrivateKey_file(ctx, "tls_files/server.key", SSL_FILETYPE_PEM) <= 0 ) {
        ERR_print_errors_fp(stderr);
        exit(EXIT_FAILURE);
    }
}

struct AioArgument *newAioArgument(SSL *ssl, SOCKET sock) {
    struct AioArgument *aioArgument = (struct AioArgument *)malloc(sizeof(struct AioArgument));
    memset(aioArgument, 0, sizeof(struct AioArgument));
    aioArgument->sock = sock;
    aioArgument->ssl = ssl;
    aioArgument->rbio = BIO_new(BIO_s_mem());
    aioArgument->wbio = BIO_new(BIO_s_mem());
    SSL_set_bio(ssl, aioArgument->rbio, aioArgument->wbio);
    aioArgument->wsaBuf.buf = aioArgument->buf;
    aioArgument->wsaBuf.len = BUF_SIZE;
    return aioArgument;
}

void freeAioArgument(struct AioArgument *aioArgument) {
    SSL_shutdown(aioArgument->ssl);
    /* BIO_free(aioArgument->rbio);
    BIO_free(aioArgument->wbio); */
    /* see <https://www.openssl.org/docs/man1.1.1/man3/SSL_free.html>:
    SSL_free() also calls the free()ing procedures for indirectly affected items, if applicable: the buffering BIO, 
    the read and write BIOs, cipher lists specially created for this ssl, the SSL_SESSION. Do not explicitly free these 
    indirectly freed up items before or after calling SSL_free(), as trying to free things twice may lead to program failure.
    */
    SSL_free(aioArgument->ssl);
    free(aioArgument);
}

// write chars into the ssl's wbio and send out
void My_SSL_write_and_send(struct AioArgument *aioArgument, char *s, int len) {
    SSL_write(aioArgument->ssl, s, strlen(s));
    char buf[SMALL_BUF_SIZE];
    int temp;
    while ((temp = BIO_read(aioArgument->wbio, buf, SMALL_BUF_SIZE)) > 0) {
        send(aioArgument->sock, buf, temp, 0);
    }
}
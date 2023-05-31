#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <winsock2.h>
#include <process.h>

#define MAX_DOMAIN_NAME_LENGTH 50
#define MAX_URL_LENGTH 256
#define BUF_SIZE 2048
struct arg_get {
    char *url;
    void *(*then)(void *);
};

struct arg_post {
    char *url;
    char *data;
    int length;
    void *(*then)(void *);
};

unsigned WINAPI threadRun_get(void *argList);
unsigned WINAPI threadRun_post(void *argList);
void resolveDomainNameFromUrl(char *url, char *domainName);
void getRequestLineUrlFromUrl(char *Url, char *requestLineUrl);
int resolvePortFromUrl(char *url);

void *get(char *url, void *(*then)(void *)) {
    struct arg_get *argList = (struct arg_get *)malloc(sizeof(struct arg_get));
    argList->url = url;
    argList->then = then;
    HANDLE thread = (HANDLE)_beginthreadex(NULL, 0, threadRun_get, (void *)argList, 0, NULL);
    return thread;
}

// data may contains '\0', so need to know length by another argument
void *post(char *url, char *data, int length, void *(*then)(void *)) {
    struct arg_post *argList = (struct arg_post *)malloc(sizeof(struct arg_post));
    argList->url = url;
    argList->data = data;
    argList->length = length;
    argList->then = then;
    HANDLE thread = (HANDLE)_beginthreadex(NULL, 0, threadRun_post, (void *)argList, 0, NULL);
    return thread;
}

unsigned WINAPI threadRun_get(void *argList) {
    struct arg_get *arg = (struct arg_get *)argList;
    char domainName[MAX_DOMAIN_NAME_LENGTH] = {0};
    resolveDomainNameFromUrl(arg->url, domainName);
    /* see <https://learn.microsoft.com/en-us/windows/win32/api/winsock2/nf-winsock2-gethostbyname>:
    The gethostbyname function returns a pointer to a hostent structure—a structure allocated by Windows Sockets.
    */
    struct hostent *hostent = gethostbyname(domainName);
    // choose the first ipaddress
    if (hostent == NULL) {
        printf("error when resolving domain name\n");
        exit(1);
    }
    struct in_addr *ipAddr = (struct in_addr *)hostent->h_addr_list[0];
    struct sockaddr_in serverAddr;
    memset(&serverAddr, 0, sizeof(serverAddr));
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = (*ipAddr).s_addr;
    serverAddr.sin_port = htons(resolvePortFromUrl(arg->url));
    int sock = socket(PF_INET, SOCK_STREAM, 0);
    if (connect(sock, (struct sockaddr *)&serverAddr, sizeof(serverAddr)) != 0) {
        printf("connect() error\n");
        exit(1);
    }
    char requestLineUrl[MAX_URL_LENGTH] = {0};
    getRequestLineUrlFromUrl(arg->url, requestLineUrl);
    send(sock, "GET ", strlen("GET "), 0);
    send(sock, requestLineUrl, strlen(requestLineUrl), 0);
    send(sock, " HTTP/1.1\r\n", strlen(" HTTP/1.1\r\n"), 0);
    send(sock, "\r\n", strlen("\r\n"), 0);
    char buf[BUF_SIZE] = {0};
    int recvLen = 0;
    int len = 0;
    while ((len = recv(sock, buf + recvLen, BUF_SIZE - recvLen, 0)) != 0) {
        recvLen += len;
    }
    arg->then(strstr(buf, "\r\n\r\n") + strlen("\r\n\r\n"));
}

unsigned WINAPI threadRun_post(void *argList) {
    struct arg_post *arg = (struct arg_post *)argList;
    char domainName[MAX_DOMAIN_NAME_LENGTH] = {0};
    resolveDomainNameFromUrl(arg->url, domainName);
    struct hostent *hostent = gethostbyname(domainName);
    // choose the first ipaddress
    if (hostent->h_addr_list[0] == NULL) {
        printf("error when resolving domain name\n");
        exit(1);
    }
    struct in_addr *ipAddr = (struct in_addr *)hostent->h_addr_list[0];
    struct sockaddr_in serverAddr;
    memset(&serverAddr, 0, sizeof(serverAddr));
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = (*ipAddr).s_addr;
    serverAddr.sin_port = htons(resolvePortFromUrl(arg->url));
    int sock = socket(PF_INET, SOCK_STREAM, 0);
    if (connect(sock, (struct sockaddr *)&serverAddr, sizeof(serverAddr)) != 0) {
        printf("connect() error\n");
        exit(1);
    }
    char requestLineUrl[MAX_URL_LENGTH] = {0};
    getRequestLineUrlFromUrl(arg->url, requestLineUrl);
    send(sock, "POST ", strlen("POST "), 0);
    send(sock, requestLineUrl, strlen(requestLineUrl), 0);
    send(sock, " HTTP/1.1\r\n", strlen(" HTTP/1.1\r\n"), 0);
    char contentLength[30] = {0};
    sprintf(contentLength, "Content-Length: %d\r\n", arg->length);
    send(sock, contentLength, strlen(contentLength), 0);
    send(sock, "\r\n", strlen("\r\n"), 0);
    send(sock, arg->data, arg->length, 0);
    
    char buf[BUF_SIZE] = {0};
    int recvLen = 0;
    int len = 0;
    while ((len = recv(sock, buf + recvLen, BUF_SIZE - recvLen, 0)) != 0) {
        recvLen += len;
    }
    arg->then(strstr(buf, "\r\n\r\n") + strlen("\r\n\r\n"));
}

void resolveDomainNameFromUrl(char *url, char *domainName) {
    char *s = strstr(url, "//");
    s += 2;
    for (int i = 0; *(s + i) != ':' && *(s + i) != '/'; i++) {
        domainName[i] = *(s + i);
    }
}

void getRequestLineUrlFromUrl(char *url, char *requestLineUrl) {
    char *s;
    s = strstr(url, "//");
    s += 2;
    s = strstr(s, "/");
    int i = 0;
    while((*s) != '\0') {
        requestLineUrl[i++] = *s;
        s++;
    }
}

int resolvePortFromUrl(char *url) {
    char buf[6] = {0};
    char *s = strstr(strstr(url, "//"), ":");
    if (s == NULL) {
        return 80;
    }
    else {
        s += strlen(":");
        for (int i = 0; *(s + i) != '/'; i++) {
            buf[i] = *(s + i);
        }
        return atoi(buf);
    }
}
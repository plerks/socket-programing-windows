#include <stdio.h>
#include <winsock2.h>
#include "server.h"

void *getDemoHandleFunc(struct Request *request, struct AioArgument *aioArgument);
void *postDemoHandleFunc(struct Request *request, struct AioArgument *aioArgument);

int main(int argc, char *argv) {
    /* No need to concern much about addHandler() thread safety problems,
    only main thread writes it before startServer() and only worker threads read it after startServer(). */
    struct Handler *demoHandler_get = (struct Handler *)malloc(sizeof(struct Handler));
    memset(demoHandler_get, 0, sizeof(struct Handler));
    strcpy(demoHandler_get->requestMethod, "GET");
    strcpy(demoHandler_get->url, "/getdemo");
    demoHandler_get->handle = getDemoHandleFunc;
    addHandler(demoHandler_get);

    struct Handler *demoHandler_post = (struct Handler *)malloc(sizeof(struct Handler));
    memset(demoHandler_post, 0, sizeof(struct Handler));
    strcpy(demoHandler_post->requestMethod, "POST");
    strcpy(demoHandler_post->url, "/postdemo");
    demoHandler_post->handle = postDemoHandleFunc;
    addHandler(demoHandler_post);

    startServer("0.0.0.0", 80);
}

void *getDemoHandleFunc(struct Request *request, struct AioArgument *aioArgument) {
    SOCKET sock = request->sock;
    char protocol[] = "HTTP/1.0 200 OK\r\n";
    char serverName[] = "Server: simple web server\r\n";
    /* If set Content-type as text/plain, for this response body, Chrome will decide to download it.
    I expected that Chrome would just display it as text. See:
    <https://stackoverflow.com/questions/71345343/why-do-some-images-automatically-download-when-their-url-is-visited-and-others-n>
    <https://stackoverflow.com/questions/9660514/content-type-of-text-plain-causes-browser-to-download-of-file> */
    char contentType[] = "Content-type: text/html\r\n";
    char emptyLine[] = "\r\n";
    send(sock, protocol, strlen(protocol), 0);
    send(sock, serverName, strlen(serverName), 0);
    send(sock, contentType, strlen(contentType), 0);
    send(sock, emptyLine, strlen(emptyLine), 0);
    char buf[] = {"This is a GET request demo response."};
    send(sock, buf, strlen(buf), 0);
}

// return the post request body, assume that the post request has header: Content-Length to tell where the request body ends
void *postDemoHandleFunc(struct Request *request, struct AioArgument *aioArgument) {
    SOCKET sock = request->sock;
    char protocol[] = "HTTP/1.0 200 OK\r\n";
    char serverName[] = "Server: simple web server\r\n";
    char contentType[] = "Content-type: text/html\r\n";
    char emptyLine[] = "\r\n";
    send(sock, protocol, strlen(protocol), 0);
    send(sock, serverName, strlen(serverName), 0);
    send(sock, contentType, strlen(contentType), 0);
    send(sock, emptyLine, strlen(emptyLine), 0);
    int contentLength = getContentLength(request->headers);
    int len = 0;
    int recvLen = 0;
    int bufSize = 50;
    char buf[bufSize];
    // Now aioArgument buf may have received part of the post request body data.
    int aioReceivedBodyLength = aioArgument->totalBytes - (strstr(aioArgument->buf, "\r\n\r\n") - aioArgument->buf) - strlen("\r\n\r\n");
    send(sock, strstr(aioArgument->buf, "\r\n\r\n") + strlen("\r\n\r\n"), aioReceivedBodyLength, 0);
    len += aioReceivedBodyLength;
    while (len < contentLength) {
        recvLen += recv(sock, buf, bufSize, 0);
        len += recvLen;
        send(sock, buf, len, 0);
    }
}
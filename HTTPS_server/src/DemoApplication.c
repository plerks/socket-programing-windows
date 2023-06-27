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

    startServer("0.0.0.0", 443);
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
    My_SSL_write_and_send(aioArgument, protocol, strlen(protocol));
    My_SSL_write_and_send(aioArgument, serverName, strlen(serverName));
    My_SSL_write_and_send(aioArgument, contentType, strlen(contentType));
    My_SSL_write_and_send(aioArgument, emptyLine, strlen(emptyLine));
    char body[] = {"This is a GET request demo response."};
    My_SSL_write_and_send(aioArgument, body, strlen(body));
}

// return the post request body, assume that the post request has header: Content-Length to tell where the request body ends
void *postDemoHandleFunc(struct Request *request, struct AioArgument *aioArgument) {
    SOCKET sock = request->sock;
    char protocol[] = "HTTP/1.0 200 OK\r\n";
    char serverName[] = "Server: simple web server\r\n";
    char contentType[] = "Content-type: text/html\r\n";
    char emptyLine[] = "\r\n";
    My_SSL_write_and_send(aioArgument, protocol, strlen(protocol));
    My_SSL_write_and_send(aioArgument, serverName, strlen(serverName));
    My_SSL_write_and_send(aioArgument, contentType, strlen(contentType));
    My_SSL_write_and_send(aioArgument, emptyLine, strlen(emptyLine));
    int contentLength = getContentLength(request->headers);
    int len = 0;
    int recvLen = 0; // total decrypted body data received
    char buf[SMALL_BUF_SIZE] = {0};
    char decryptedBuf[SMALL_BUF_SIZE] = {0};
    // Now aioArgument buf may have received part of the post request body data.
    int aioReceivedBodyLength = aioArgument->decryptedTotalBytes - (strstr(aioArgument->decryptedBuf, "\r\n\r\n") - aioArgument->decryptedBuf) - strlen("\r\n\r\n");
    My_SSL_write_and_send(aioArgument, strstr(aioArgument->decryptedBuf, "\r\n\r\n") + strlen("\r\n\r\n"), aioReceivedBodyLength);
    recvLen += aioReceivedBodyLength;
    while (recvLen < contentLength) {
        /* In case there is data unread inner ssl, read all out first.(But this should't happen since in Server.c threadRun() I have
        called SSL_read() in a loop to read all decrypted data out)
        */
        while ((len = SSL_read(aioArgument->ssl, decryptedBuf, SMALL_BUF_SIZE)) > 0) {
            recvLen += len;
            SSL_write(aioArgument->ssl, decryptedBuf, len);
            while ((len = BIO_read(aioArgument->wbio, buf, SMALL_BUF_SIZE)) > 0) {
                send(sock, buf, len, 0);
            }
        }

        len = recv(sock, buf, SMALL_BUF_SIZE, 0);
        if (len < 0) {
            int err = WSAGetLastError();
            if (err == EWOULDBLOCK) {
                continue;
            }
            else {
                printf("recv() returned -1 with WSAGetLastError(): %d\n", err);
                exit(1);
            }
        }
        BIO_write(aioArgument->rbio, buf, len);
        while ((len = SSL_read(aioArgument->ssl, decryptedBuf, SMALL_BUF_SIZE)) > 0) {
            recvLen += len;
            SSL_write(aioArgument->ssl, decryptedBuf, len);
            // To read all the data out, need to call BIO_read() mutiply times since the buffer might be not big enough and can't read all the data in one call.
            while ((len = BIO_read(aioArgument->wbio, buf, SMALL_BUF_SIZE)) > 0) {
                send(sock, buf, len, 0);
            }
        }
    }
    shutdown(sock, SD_SEND);
}
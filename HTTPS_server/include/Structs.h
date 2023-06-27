#ifndef STRUCTS_H
#define STRUCTS_H
#include <openssl/ssl.h>
#include <openssl/err.h>
#include <winsock2.h>

/* This file is defined to resolve the problem that Handler.h and Server.h may have loop dependent relationship otherwise.
BUF_SIZE and AioArgument used to be defined in Server.h. Server.h needs to include Handlers.h for using struct Request, Handlers.h
needs to include Server.h for using BUF_SIZE and struct AioArgument. Thus loop depending. If Handlers.h does't include Server.h,
the program can run but have a warning since can't find the definition of struct AioArgument in Handlers.h.
*/
#define BUF_SIZE 20480
#define SMALL_BUF_SIZE 50

struct AioArgument {
    OVERLAPPED overlapped;
    char buf[BUF_SIZE];
    char decryptedBuf[BUF_SIZE];
    WSABUF wsaBuf;
    int totalBytes;
    int decryptedTotalBytes;
    SOCKET sock;
    SSL *ssl;
    BIO *rbio;
    BIO *wbio;
};

#endif
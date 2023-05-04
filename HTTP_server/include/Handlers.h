#ifndef HANDLERS_H
#define HANDLERS_H

#include <winsock2.h>
#include "Structs.h"

#define MAX_HANDLER_NUM 1024
#define MAX_HTTP_REQUEST_METHOD_LENGTH 10
#define URL_LENGTH 1024

typedef unsigned char byte;

// The Request implementation does't contain body here, the best implementation might be wrapping the body as an Inputstream
struct Request {
    SOCKET sock;
    char *requestLine;
    char *headers;
    char *emptyLine;
};

struct Handler {
    void *(*handle)(struct Request *, struct AioArgument *);
    char requestMethod[MAX_HTTP_REQUEST_METHOD_LENGTH];
    char url[URL_LENGTH];
};

extern struct Handler *handlers[MAX_HANDLER_NUM];

extern int handler_num;

void addHandler(struct Handler *handler);

void freeRequest(struct Request *request);
#endif
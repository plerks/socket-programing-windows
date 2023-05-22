#ifndef SERVER_H
#define SERVER_H

#include "Handlers.h"

void startServer(char *ip, int port);

void getRequestMethodAndUrl(struct Request *request, char *requestMethod, char *url);

int getContentLength(char *buf);

void My_SSL_write_and_send(struct AioArgument *aioArgument, char *s, int len);
#endif
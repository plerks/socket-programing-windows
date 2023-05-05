#include <stdio.h>
#include <winsock2.h>
#include "MyHttp.h"

void *callback_get(void *body);
void *callback_post(void *body);

int main(int argc, char *argv[]) {
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        printf("WSAStartup() error!\n");
        exit(1);
    }

    char url[] = {"http://localhost:80/index.html"};
    HANDLE thread = http.get(url, callback_get);
    printf("thread1 running\n");

    char url2[] = {"http://localhost:80/postdemo"};
    char data[] = {"This is data posted to the server api '/postdemo' and the response body will contain the same data."};
    HANDLE thread2 = http.post(url2, data, strlen(data), callback_post);
    printf("thread2 running\n");
    // inner get()/post(), a thread is created to perform request, wait until the thread ends
    WaitForSingleObject(thread, INFINITE);
    WaitForSingleObject(thread2, INFINITE);
}

// body points to response body
void *callback_get(void *body) {
    char *r = (char *)body;
    printf("%s\n", body);
}

void *callback_post(void *body) {
    char *r = (char *)body;
    printf("%s\n", body);
}
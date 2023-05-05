#ifndef HTTP_H
#define HTTP_H

struct Http {
    void *(*get)(char *url, void *(*then)(void *));
    void *(*post)(char *url, char *data, int length, void *(*then)(void *));
};

void *get(char *url, void *(*then)(void *));

void *post(char *url, char *data, int length, void *(*then)(void *));

struct Http http = {get, post};
#endif
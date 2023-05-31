#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <winsock2.h>
#include <process.h>
#include <openssl/ssl.h>
#include <openssl/err.h>
#include <openssl/x509v3.h>

#define MAX_DOMAIN_NAME_LENGTH 50
#define MAX_URL_LENGTH 256
#define MAX_DOMAIN_LENGTH 256
#define BUF_SIZE 20480
#define SMALL_BUF_SIZE 300
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
void My_SSL_write_and_send(SSL *ssl, SOCKET sock, char *s, int len);
void get_common_name_from_line(char common_name[], char *line);
BOOL checkCertificateDomainName(char *line, char *url, char *commonName);

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
    // choose the first ip address
    if (hostent == NULL) {
        printf("error when resolving domain name\n");
        exit(1);
    }
    struct in_addr *ipAddr = (struct in_addr *)hostent->h_addr_list[0];
    // printf("resolved ip address: %s\n", inet_ntoa(*ipAddr));
    struct sockaddr_in serverAddr;
    memset(&serverAddr, 0, sizeof(serverAddr));
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = (*ipAddr).s_addr;
    serverAddr.sin_port = htons(resolvePortFromUrl(arg->url));
    SOCKET sock = socket(PF_INET, SOCK_STREAM, 0);
    if (connect(sock, (struct sockaddr *)&serverAddr, sizeof(serverAddr)) != 0) {
        printf("connect() error\n");
        exit(1);
    }
    int opt_val = 1;
    setsockopt(sock, IPPROTO_TCP, TCP_NODELAY, (void *)&opt_val, sizeof(opt_val));

    const SSL_METHOD *method;
    SSL_CTX *ctx;
    method = TLS_client_method();
    ctx = SSL_CTX_new(method);
    // If `SSL_CTX_set_verify(ctx, SSL_VERIFY_PEER, NULL);`, for certificates not verified successfully, SSL_connect() will fail and the request can't continue.
    SSL_CTX_set_verify(ctx, SSL_VERIFY_NONE, NULL);
    SSL_CTX_set_verify_depth(ctx, 4);
    /* The 3rd argument CApath is not that convinent.
    see:
    <https://www.openssl.org/docs/manmaster/man1/openssl-verification-options.html>
    <https://www.openssl.org/docs/manmaster/man1/openssl-rehash.html>
    <https://stackoverflow.com/questions/28366384/troubles-with-ssl-ctx-load-verify-locations>
    "-CApath dir
    Use the specified directory as a collection of trusted certificates, i.e., a trust store. 
    Files should be named with the hash value of the X.509 SubjectName of each certificate. 
    This is so that the library can extract the IssuerName, hash it, and directly lookup the file to get the issuer certificate. 
    See openssl-rehash(1) for information on creating this type of directory."
    
    It seems that openssl don't automatically add the .crt files under CApath into trusted list. Need to run
    `openssl rehash CApath` to generate special files and then will openssl find trusted certificates.
    So add another file like tls_files/server2.crt won't make server2.crt as a trusted certificate.
    */
    SSL_CTX_load_verify_locations(ctx, "tls_files/server.crt", "tls_files/");
    SSL *ssl = SSL_new(ctx);
    SSL_set_fd(ssl, sock);
    int returnValue = SSL_connect(ssl);
    if (returnValue != 1) {
        printf("SSL_connect() error with return value: %d, process exiting\n", returnValue);
        exit(1);
    }
    /* see:
    <https://wiki.openssl.org/index.php/SSL/TLS_Client>
    <https://www.openssl.org/docs/man1.0.2/man3/SSL_get_verify_result.html>
    SSL_get_verify_result(): If no peer certificate was presented, the returned result code is X509_V_OK.
    This is because no verification error occurred, it does however not indicate success.
    So need to check if peer certificate is presented.
    */
    X509* cert = SSL_get_peer_certificate(ssl);
    if (!cert) {
        printf("No certificate presented by peer! Continue to request ignoring it.");
    }
    else {
        X509_NAME * name = X509_get_subject_name(cert);
        char *line = X509_NAME_oneline(X509_get_subject_name(cert), 0, 0);
        char commonName[MAX_DOMAIN_LENGTH] = {0};
        X509_free(cert);
        if (!checkCertificateDomainName(line, arg->url, commonName)) {
            printf("The server certificate has an inconsistent domain name! The domain name in certificate is \"%s\" while the requesting domain is \"%s\". Continue to request ignoring it.\n", commonName, domainName);
        }
        else if (SSL_get_verify_result(ssl) != X509_V_OK) { // Here should be checking the certificate hash failed.
            printf("Certificate digital signature check failed! Continue to request ignoring the failure.\n");
        }
        else {
            printf("Certificate verification succeeded!\n");
        }
    }
    BIO *rbio = BIO_new(BIO_s_mem());
    BIO *wbio = BIO_new(BIO_s_mem());
    SSL_set_bio(ssl, rbio, wbio);

    char requestLineUrl[MAX_URL_LENGTH] = {0};
    getRequestLineUrlFromUrl(arg->url, requestLineUrl);
    My_SSL_write_and_send(ssl, sock, "GET ", strlen("GET "));
    My_SSL_write_and_send(ssl, sock, requestLineUrl, strlen(requestLineUrl));
    My_SSL_write_and_send(ssl, sock, " HTTP/1.1\r\n", strlen(" HTTP/1.1\r\n"));
    My_SSL_write_and_send(ssl, sock, "\r\n", strlen("\r\n"));
    /* Need to call this. It's weird that I found that if I don't call this, the above sent-out data might not be really send out (probably
    cached in the socket, but the extreme weird thing is that windows socket did't decide to send the data out even after a period of time).
    And cause a result that at least for my implementation of HTTPS_server, the server did't receive the data until the emptyline,
    and it stucks at WSARecv() and don't respond.
    */
    shutdown(sock, SD_SEND);

    char buf[BUF_SIZE] = {0};
    int recvLen = 0;
    int len = 0;
    while ((len = recv(sock, buf + recvLen, BUF_SIZE - recvLen, 0)) > 0) {
        recvLen += len;
    }
    char decryptedBuf[BUF_SIZE] = {0};
    BIO_write(SSL_get_rbio(ssl), buf, recvLen);
    len = 0;
    recvLen = 0;
    while ((len = SSL_read(ssl, decryptedBuf + recvLen, BUF_SIZE - recvLen)) > 0) {
        recvLen += len;
    }
    if (strstr(decryptedBuf, "\r\n\r\n") == NULL) {
        arg->then("");
    }
    else {
        arg->then(strstr(decryptedBuf, "\r\n\r\n") + strlen("\r\n\r\n"));
    }
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
    SOCKET sock = socket(PF_INET, SOCK_STREAM, 0);
    if (connect(sock, (struct sockaddr *)&serverAddr, sizeof(serverAddr)) != 0) {
        printf("connect() error\n");
        exit(1);
    }
    /* int opt_val = 1;
    setsockopt(sock, IPPROTO_TCP, TCP_NODELAY, (void *)&opt_val, sizeof(opt_val)); */

    const SSL_METHOD *method;
    SSL_CTX *ctx;
    method = TLS_client_method();
    ctx = SSL_CTX_new(method);
    SSL_CTX_set_verify(ctx, SSL_VERIFY_NONE, NULL);
    SSL_CTX_set_verify_depth(ctx, 4);
    SSL_CTX_load_verify_locations(ctx, "tls_files/server.crt", "tls_files/");
    SSL *ssl = SSL_new(ctx);
    SSL_set_fd(ssl, sock);
    int returnValue = SSL_connect(ssl);
    if (returnValue != 1) {
        printf("SSL_connect() error with return value: %d, process exiting\n", returnValue);
        exit(1);
    }
    X509* cert = SSL_get_peer_certificate(ssl);
    if (!cert) {
        printf("No certificate presented by peer! Continue to request ignoring it.");
    }
    else {
        X509_NAME * name = X509_get_subject_name(cert);
        char *line = X509_NAME_oneline(X509_get_subject_name(cert), 0, 0);
        char commonName[MAX_DOMAIN_LENGTH] = {0};
        X509_free(cert);
        if (!checkCertificateDomainName(line, arg->url, commonName)) {
            printf("The server certificate has an inconsistent domain name! The domain name in certificate is \"%s\" while the requesting domain is \"%s\". Continue to request ignoring it.\n", commonName, domainName);
        }
        else if (SSL_get_verify_result(ssl) != X509_V_OK) { // Here should be checking the certificate hash failed.
            printf("Certificate digital signature check failed! Continue to request ignoring the failure.\n");
        }
        else {
            printf("Certificate verification succeeded!\n");
        }
    }
    BIO *rbio = BIO_new(BIO_s_mem());
    BIO *wbio = BIO_new(BIO_s_mem());
    SSL_set_bio(ssl, rbio, wbio);

    char requestLineUrl[MAX_URL_LENGTH] = {0};
    getRequestLineUrlFromUrl(arg->url, requestLineUrl);
    My_SSL_write_and_send(ssl, sock, "POST ", strlen("POST "));
    My_SSL_write_and_send(ssl, sock, requestLineUrl, strlen(requestLineUrl));
    My_SSL_write_and_send(ssl, sock, " HTTP/1.1\r\n", strlen(" HTTP/1.1\r\n"));
    char contentLength[30] = {0};
    sprintf(contentLength, "Content-Length: %d\r\n", arg->length);
    My_SSL_write_and_send(ssl, sock, contentLength, strlen(contentLength));
    My_SSL_write_and_send(ssl, sock, "\r\n", strlen("\r\n"));
    My_SSL_write_and_send(ssl, sock, arg->data, arg->length);
    
    shutdown(sock, SD_SEND);

    char buf[BUF_SIZE] = {0};
    int recvLen = 0;
    int len = 0;
    while ((len = recv(sock, buf + recvLen, BUF_SIZE - recvLen, 0)) > 0) {
        recvLen += len;
    }
    char decryptedBuf[BUF_SIZE] = {0};
    BIO_write(SSL_get_rbio(ssl), buf, recvLen);
    len = 0;
    recvLen = 0;
    while ((len = SSL_read(ssl, decryptedBuf + recvLen, BUF_SIZE - recvLen)) > 0) {
        recvLen += len;
    }
    if (strstr(decryptedBuf, "\r\n\r\n") == NULL) {
        arg->then("");
    }
    else {
        arg->then(strstr(decryptedBuf, "\r\n\r\n") + strlen("\r\n\r\n"));
    }
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
        return 443;
    }
    else {
        s += strlen(":");
        for (int i = 0; *(s + i) != '/'; i++) {
            buf[i] = *(s + i);
        }
        return atoi(buf);
    }
}

// write chars into ssl and send out
void My_SSL_write_and_send(SSL *ssl, SOCKET sock, char *s, int len) {
    SSL_write(ssl, s, len);
    char buf[SMALL_BUF_SIZE] = {0};
    int temp;
    while ((temp = BIO_read(SSL_get_wbio(ssl), buf, SMALL_BUF_SIZE)) > 0) {
        send(sock, buf, temp, 0);
    }
}

void get_common_name_from_line(char common_name[], char *line) {
    char *s = strstr(line, "CN") + strlen("CN");
    for (int i = 0; *(s + i) != '/'; i++) {
        common_name[i] = *(s + i);
    }
}

// once return, the domain name in certificate will be filled in commonName.
BOOL checkCertificateDomainName(char *line, char *url, char *commonName) {
    char *s;
    // Did't find the openssl function to get domain name from cert. Don't know the field key of domain name in cert, tried some websites and found the following.
    if (strstr(line, "Ltd/CN=") != NULL) {
        s = strstr(line, "Ltd/CN=") + strlen("Ltd/CN=");
    }
    else if (strstr(line, "Inc/CN=") != NULL) {
        s = strstr(line, "Inc./CN=") + strlen("Inc./CN=");
    }
    else {
        s = strstr(line, "/CN=") + strlen("/CN=");
    }
    for (int i = 0; *(s + i) != '/' && *(s + i) != '\0'; i++) {
        commonName[i] = *(s + i);
    }

    char domainName[MAX_DOMAIN_LENGTH] = {0};
    s = strstr(url, "//") + strlen("//");
    for (int i = 0; *(s + i) != '/' && *(s + i) != ':'; i++) {
        domainName[i] = *(s + i);
    }

    return !strcmp(commonName, domainName);
}
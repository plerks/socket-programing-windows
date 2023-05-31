Improve the HTTP_request to HTTPS_request using [openssl](https://www.openssl.org/) library. (folder include/openssl, libssl.a and libcrypto.a are openssl content)

The code is not mature enough, I did't write adequate error processing code and not suitable for handling extreme situation (for example, in threadRun_get(), when the server response data gets very long, the decryptedBuf may not be big enough).

For example, if in lib/MyHttps.c `#define BUF_SIZE 2048`, and `http.get("https://www.baidu.com/index.html", callback_get)`, then the www.baidu.com/index.html webpage won't be printed.

## How to run

First, need to ensure the server is running. You can run **HTTPS_server** or change the domain name in src/HttpsRequestDemo.c to some public domain name.

1.Open **HTTPS_request** folder with VSCode and open src/HttpsRequestDemo.c to run.

2.Or
```
HTTPS_request> gcc src/HttpsRequestDemo.c lib/MyHttps.c -I include -lssl -lcrypto -lws2_32 -o src/HttpsRequestDemo
HTTPS_request> ./src/HttpsRequestDemo
```

## About the HTTPS certificate verify.
By calling `SSL_CTX_load_verify_locations()` in lib/MyHttps.c, the certificate tls_files/server.crt that I created using openssl command line is trusted. But the domain name inner it is "gxytestserver.com", so the certificate check will fail, and as for requesting other public domain, the certificate check will also fail since I did't place their corresponding certificate files (see my comment in lib/MyHttps.c) in `SSL_CTX_load_verify_locations()` CApath.
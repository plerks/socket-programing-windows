Improve the HTTP_server to HTTPS_server using [openssl](https://www.openssl.org/) library. (folder include/openssl, libssl.a and libcrypto.a are openssl content)

The code is not mature enough, I did't write adequate error processing code and not suitable for handling extreme situation (for example, in postDemoHandleFunc(), when the client post data gets very long, the decryptedBuf may not be big enough).

I used asynchronous WSARecv() to receive data until all the data before emptyLine is received. However, then in callProperHandler() I assumed that in the user code in DemoApplication.c, no asynchronous I/O is called (WSARecv()/WSASend() function). If to support the user code to use asynchronous I/O, the solution would be like adding a "from" field in AioArgument to indicate if the I/O completion is from the user code or server code. And wrap WSARecv()/WSASend() with a user-provided callback function. In server.c, distinguish if the completion is from the server code or user code by the "from" field. If from user, call the user callback function (the function pointer is passed by aioArgument).

## How to run
1.Open HTTPS_server folder with VSCode and open src/DemoApplication.c to run.

2.Or
```
HTTPS_server> gcc src/DemoApplication.c lib/Handlers.c lib/Server.c -I include -lssl -lcrypto -lws2_32 -lGDI32 -lADVAPI32 -lCRYPT32 -lUSER32 -o src/DemoApplication
HTTPS_server> ./src/DemoApplication
```
See [openssl's description](https://github.com/openssl/openssl/blob/master/NOTES-WINDOWS.md#linking-native-applications): "If you link with static OpenSSL libraries, then you're expected to additionally link your application with WS2_32.LIB, GDI32.LIB, ADVAPI32.LIB, CRYPT32.LIB and USER32.LIB". (But in my test don't link `GDI32.LIB`, `ADVAPI32.LIB`, `CRYPT32.LIB` and `USER32.LIB` still works fine). Besides, `ws2_32` needs to be included after `libcrypto.a` and `libssl.a` else compiling will fail. About the reason, see [this](https://stackoverflow.com/questions/3363398/g-linking-order-dependency-when-linking-c-code-to-c-code) and [this](https://blog.csdn.net/zzd_zzd/article/details/105059952).

In my test,
```
HTTPS_server> gcc src/DemoApplication.c lib/Handlers.c lib/Server.c -I include -lssl -lcrypto -lws2_32 -o src/DemoApplication
HTTPS_server> ./src/DemoApplication
```
still works fine.

## How to see the effect

I have put some static files under resources and configured some urls' response.

* visit <https://localhost/index.html> for static file index.html.
* visit <https://localhost/getdemo> for self-configured GET response.

When visiting from the browser, the browser will give a warning since I'm using self-generated self-signed certificate. The certificate won't get acknowledged by the browser.

* POST <https://localhost:443/postdemo> for self-configured POST response. (With Postman or something else, only support when the request indicates its request body length by `Content-Length`)
* other requests for errorpage

## Build and link the openssl libraries
I have already built the libraries manually and put the artifacts in `lib` folder so you don't need to build them yourself.

By `printf("%s\n", SSLeay_version(SSLEAY_VERSION));`, my openssl version is `OpenSSL 3.1.0 14 Mar 2023`.

1. Need to download [MSYS2](https://www.msys2.org/) and run `pacman -S mingw-w64-ucrt-x86_64-gcc` and `pacman -S make` to have gcc (need to modify PATH environment) and make installed inner MSYS2's environment.
2. Use the MSYS2 shell to to git clone [openssl](https://github.com/openssl/openssl), my version's commit id is `43d5dac9d00ac486823d949f85ee3ad650b62af8`
3. Referring to [this](https://github.com/openssl/openssl/blob/master/NOTES-WINDOWS.md#native-builds-using-mingw), use the MSYS2 shell to run `./Configure mingw64` and `make` to build the libraries.
4. Find the `libcrypto.a`, `libssl.a` and `openssl` header files in `MSYS2's installation directory/home/${username}` directory and introduce them into the project (I have put the library files under lib folder and header files under include folder).

## Generate crt and key file
Use openssl to generate .crt and .key files. However, it seems that openssl did't provide official windows installer. I found an openssl installer [here](https://slproweb.com/products/Win32OpenSSL.html).

Referring to [this](https://ningyu1.github.io/site/post/51-ssl-cert/), run:

1. run `openssl genrsa -out server.key 1024` to generate private key
2. run `openssl req -new -key server.key -out server.csr` to generate the .csr file. Fill in your domain name in the common name field in the terminal. I tested that openssl permits to generate a certificate with domain name localhost.
3. run `openssl x509 -req -in server.csr -out server.crt -signkey server.key -days 36500` to generate the .crt file

I have placed the .crt and .key file at the tls_files folder.

## Use openssl under IOCP
reference:
* https://wiki.openssl.org/index.php/Simple_TLS_Server
* https://famellee.wordpress.com/2013/02/20/use-openssl-with-io-completion-port-and-certificate-signing/
* https://blog.csdn.net/xiaoqing_2014/article/details/79720913
* https://blog.csdn.net/liao20081228/article/details/77193729

Referred to [this](https://famellee.wordpress.com/2013/02/20/use-openssl-with-io-completion-port-and-certificate-signing/), for the way of how to encrypt/decrypt data because in IOCP need to use asynchronous I/O like WSASend()/WSARecv() to send data. The reference's original text:
```
OpenSSL data handling
When use OpenSSL with asynchronous sockets, we need to use 
memory BIOs instead of socket BIOs and we are responsible for 
data transmission between sockets and memory BIOs. For data 
received from sockets, we need to call BIO_write to write them 
to input BIO, then call SSL_read with input BIO to retrieve 
decrypted plain text; for data to be sent out, first call 
SSL_write to write them into output BIO, then call BIO_read on 
output BIO to retrieve data that needs to be sent to peer over 
socket.
```

See openssl's [SSL_set_fd()](https://www.openssl.org/docs/man3.0/man3/SSL_set_fd.html) doc:
```
SSL_set_fd() sets the file descriptor fd as the input/output 
facility for the TLS/SSL (encrypted) side of ssl. fd will 
typically be the socket file descriptor of a network 
connection.

When performing the operation, a socket BIO is automatically 
created to interface between the ssl and fd. The BIO and hence 
the SSL engine inherit the behaviour of fd. If fd is 
nonblocking, the ssl will also have nonblocking behaviour.

If there was already a BIO connected to ssl, BIO_free() will 
be called (for both the reading and writing side, if 
different).
```
There're two BIO in an SSL, rbio for the read operations of the ssl object and wbio for the write operations of the ssl object. The socket BIO can be set by `SSL_set_bio()` and got by `SSL_get_rbio()/SSL_get_wbio()` and in my test if you don't manually call `SSL_set_bio()`, there's default BIO inner SSL and rbio == wbio. See the descriptions of [SSL_set0_rbio()](https://www.openssl.org/docs/man3.1/man3/SSL_set_bio.html): "On calling this function, any existing rbio that was previously set will also be freed via a call to BIO_free_all". So probably there's no need to free the previous BIO when calling `SSL_set_bio()`.

In my view, openssl library's SSL * is quite like the equivalent of TCP socket file descriptor.

My summaries about openssl's vital functions used about encrypting/decrypting data:
* [BIO_write(BIO *b, const void *data, int dlen)](https://www.openssl.org/docs/man3.0/man3/BIO_write.html). Write the data into BIO *b. The function don't conduct data encrypting/decrypting, just copy the data into SSL's internal BIO.
* [BIO_read(BIO *b, void *buf, int len)](https://www.openssl.org/docs/man1.0.2/man3/BIO_read.html). Read the data from BIO *b. The function don't conduct data encrypting/decrypting, just read the data from SSL's internal BIO.
* [SSL_write(SSL *ssl, const void *buf, int num)](https://www.openssl.org/docs/man1.1.1/man3/SSL_write.html). **Encrypt** the buf data into ssl. Guessing from the code effect in HTTPS_server/src/DemoApplication.c/postDemoHandleFunc(), the function **don't** send the encrypted data out. Need to call BIO_read() to get the encrypted data and send it out by send().
* [SSL_read(SSL *ssl, void *buf, int num)](https://www.openssl.org/docs/man1.1.1/man3/SSL_read.html). **Decrypt** the data from ssl into buf. Guessing from the code effect in HTTPS_server/src/DemoApplication.c/postDemoHandleFunc(), the function can retrive data from socket, not only the data written into BIO using BIO_write().

BIO_write()/BIO_read() don't conduct encrypt/decrypt, just data moving. SSL_write()/SSL_read() conduct the data form change. To use IOCP, must use windows' asynchronous socket I/O functions like WSASend()/WSARecv() to send/recv data (These functions are probably already system calls, else the completion port can't capture the completion of I/O), so need to use the openssl's functions as a encrypt/decrypt tool and don't let openssl do the send/recv work.

When using IOCP, it's needed to use windows' WSASend()/WSARecv() function so that the completion port can capture the I/O completion. So it's needed to encrypt/decrypt the data. More specifically, SSL * has buffer inside, after WSARecv() has got the HTTPS encrypted data, use BIO_write() to write the encrypted data into the buffer, and then use SSL_read() to read decrypted data from the buffer. And as for sending data, after the data to be sent is determined, use SSL_write() to write the plain data into the buffer (now encrypted), and then use BIO_read() to get the encrypted data and send it out with WSASend().
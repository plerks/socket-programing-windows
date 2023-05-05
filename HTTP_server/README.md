An HTTP server. Able to serve static files and Implemented basic function that user can configure url with its corresponding response.

The function is limited and not easy to use, far less than formal framework.

I used asynchronous WSARecv() to receive data until all the data before emptyLine is received. However, then in callProperHandler() I have to assume that no asynchronous I/O is called else things would be much more complicated.

Default c lib does not have common data structures like String or Map. And I did't implement one myself. So I need to define buf length as an estimated upper bound or write some other compromised code.

The book's chapter24 demonstrated how to code a basic HTTP server. But it's quite basic, just accept, create new thread to read, resolve and write, without using any concurrent mode introduced in the book before. And the author mentioned that because for normal HTTP protocol, the server just close the connetion after responsing (short connection), no time for IOCP/epoll to make much effect, so using IOCP/epoll won't bring much improvement. Despite that, I used IOCP to implement the server (originate from chapter23/IOCPEchoServ.c).

## How to run

1.Open **HTTP_server** folder with VSCode and open src/DemoApplication.c to run.

2.Or
```
HTTP_server> gcc src/DemoApplication.c lib/Handlers.c lib/Server.c -I include -l ws2_32 -o src/DemoApplication
HTTP_server> ./src/DemoApplication
```

3.Or
```
HTTP_server\lib> gcc -c .\Handlers.c .\Server.c -I ../include
HTTP_server\lib> ar -cr libmyserver.a Handlers.o Server.o
HTTP_server> gcc ./src/DemoApplication.c -I include -L lib -lmyserver -lws2_32 -o src/DemoApplication
HTTP_server> ./src/DemoApplication
```

## How to see the effect

I have put some static files under resources and configured some urls' response.

* visit <http://localhost:80/index.html> for static file index.html.
* visit <http://localhost/getdemo> for self-configured GET response.
* POST <http://localhost:80/postdemo> for self-configured POST response. (With Postman or something else, only support when the request indicates its request body length by `Content-Length`)
* other requests for errorpage

## How the HTTP request/response body end is detected?

First of all, the server and client can both easily know the end of headers by detecting the emptyLine (detecting "\r\n\r\n").

HTTP protocol's typical process: client send -> server response -> server close the connection.

So the client can know that the server has sent all the response content by detect the close of connection (read() returns 0).

And as for how server detects the end of request body. If it's a GET request, then it's supposed to have no body (thus done). And if it's a POST request, see [this](https://stackoverflow.com/questions/4824451/detect-end-of-http-request-body), the request headers should indicate the body length by `Content-Length`(for fixed length) or `Transfer-Encoding: Chunked`(for uncertain length).
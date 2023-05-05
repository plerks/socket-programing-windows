A basic http request lib, able to send GET/POST request and call the corresponding callback after having received all the body data (assume that there's limited data).

Implemented by simply creating a new thread to perform the request and call the callback function.

Default c lib does not have common data structures like String or Map. And I did't implement one myself. So I need to define buf length as an estimated upper bound or write some other compromised code.

Asynchronous I/O suits the requirement best. However, windows asynchronous I/O finish confirm either need to call WSAWaitForMultipleEvents() (like chapter21/AsynNotiEchoServ.c, this function blocks so also need to create a new thread to prevent http.get() blocks), or use WSARecv()/WSASend() completion routine (like chapter23/CmplRouEchoServ.c, this does't need to create a new thread, but need to let the thread to enter alertable wait status by calling functions like SleepEx() so that the callback can be executed, which is not elegant. Besides, the thread needs to sleep enough until the asynchronous I/O finish).

## How to run

First, need to ensure the server is running. You can run **HTTP_server** or change the domain name in src/HttpRequestDemo.c to some public domain name.

1.Open **HTTP_request** folder with VSCode and open src/HttpRequestDemo.c to run.

2.Or
```
HTTP_request> gcc src/HttpRequestDemo.c lib/MyHttp.c -I include -lws2_32 -o src/HttpRequestDemo
HTTP_request> ./src/HttpRequestDemo
```
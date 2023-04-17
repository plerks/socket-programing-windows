Here is my learning on socket programing. Based on a book called *TCP/IP网络编程* (ISBN 9787115358851).

The code is in c and not cross-platform. Here is code for windows. See the code for linux [here](https://github.com/plerks/socket-programing-linux).

Most code is based on the book's chapter example code (Not all example code is contained). And I may have adjusted the code style based on my own habit or added some adjustment. I'm following camelCase naming convention for the windows code.

To run the code, typically you need to run the server and its corresponding client to see the effect. Take chapter1 for example. You need to:
```
cd chapter1
gcc hello_server.c -o hello_server -lwsock32 -Wall
gcc hello_client.c -o hello_client -lwsock32 -Wall
./hello_server
./hello_client
```
Else you can open the project with VSCode and run the code with my setting. Just to launch the target "${fileBasenameNoExtension}" is fine.

Server serves at port 9190 by default, you can configure it by running like `./hello_server 9190`.

Client connect to 127.0.0.1:9190 by default, you can configure it by running like `./hello_client 127.0.0.1 9190`.

`gcc --version`:
```
gcc (x86_64-win32-seh-rev0, Built by MinGW-W64 project) 8.1.0
Copyright (C) 2018 Free Software Foundation, Inc.
This is free software; see the source for copying conditions.  There is NO
warranty; not even for MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
```
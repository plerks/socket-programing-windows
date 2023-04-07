Echo server. Concurrent model based on select() function. Run along with chapter9/echo_client.c.

On [select() microsoft document](https://learn.microsoft.com/en-us/windows/win32/api/winsock2/nf-winsock2-select#remarks), they say:
```
The variable FD_SETSIZE determines the maximum number of descriptors in a set. (The default value of FD_SETSIZE is 64, which can be modified by defining FD_SETSIZE to another value before including Winsock2.h.)
```
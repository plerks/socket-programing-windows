CmplRouEchoServ.c and IOCPEchoServ.c are both echo server. Run along with chapter21/echo_client_compare_len.c.

### CmplRouEchoServ.c:

**Overlapped I/O**. Overlapped I/O is a concept in windows. But in my view it's exactly asynchronous I/O. And the book author indicates that there's no need to distinguish between overlapped I/O and asynchronous I/O.

One vital thing for overlapped I/O is confirm the finish of I/O. There're two ways - based on event object or completion routine(a callback function in fact). If use event object, it's needed to use WSAWaitForMultipleEvents() and the function introduced blocking. CmplRouEchoServ.c used completion routine.

Another thing weird is that if run CmplRouEchoServ.c along with chapter21/echo_client_compare_len.c, even if the client message is not long, the client recv() may take twice to get the full message (probably because WSASend() divided the the message into two packages, but there's no need to divide).

**About the alertable wait status**:

Need to call functions like SleepEx(DWORD dwMilliseconds, BOOL bAlertable) to get the thread into alertable wait status, and then the completion routine function can be called (windows' rule).

SleepEx(DWORD dwMilliseconds, BOOL bAlertable) microsoft [doc](https://learn.microsoft.com/en-us/windows/win32/api/synchapi/nf-synchapi-sleepex): 
```html
Suspends the current thread until the specified condition is met. Execution resumes when one of the following occurs:
An I/O completion callback function is called.
An asynchronous procedure call (APC) is queued to the thread.
The time-out interval elapses.
...
a value of zero (dwMilliseconds) causes the thread to relinquish the remainder of its time slice to any other thread that is ready to run (like Java's yield()).
```
I guess the mechanism works like this:

when calling functions like SleepEx(), the current thread gets into alertable wait status until return. In windows, only in alertable wait status can the completion function be called, so when windows found (SleepEx() is probably a system call) the thread entered alertable wait status (which means the current thread don't have things to do, which means execute callback functions now won't bother the procedure stream), it goes about to call the callback function, add the callback function frame to the callstack.

See [this](https://learn.microsoft.com/en-us/windows-hardware/drivers/kernel/waits-and-apcs):

```html
Typically, the thread remains in the wait state until the operating system can complete the operation that the caller requests. However, if the caller specifies WaitMode = UserMode, the operating system might interrupt the wait.
...
The exact situations in which the operating system interrupts the wait depends on the value of the Alertable parameter of the routine. If Alertable = TRUE, the wait is an alertable wait. Otherwise, the wait is a non-alertable wait. The operating system interrupts alertable waits only to deliver a user APC.
```
To know it better, need to learn about windows' APC mechanism.

The main point of CmplRouEchoServ.c is using asynchronous I/O and completion routine. But the need to call SleepEx() makes the code not elegant.

### IOCPEchoServ.c

**IOCP**. Thread pool + asynchronous I/O + multiple user program worker thread.

In my understanding, IOCP works like this: calling `CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 0)`, windows [creates an I/O completion port](https://learn.microsoft.com/en-us/windows/win32/fileio/createiocompletionport), which contains a thread pool to handle I/O. And the second CreateIoCompletionPort call: `CreateIoCompletionPort((HANDLE)clientSock, completionPort, (ULONG_PTR)clientSockPtr, 0)` tells the os that I care about the I/O finish on the file handle - clientSock. Then, when os finishes I/O on clientSock, the completion port object saves an object with related information into a message queue. And then the program can dequeue a finish information object by calling `GetQueuedCompletionStatus()`.

Usually set IOCP thread number == user program worker thread number == system processors number.

[I/O Completion Ports' Microsoft doc](https://learn.microsoft.com/en-us/windows/win32/fileio/i-o-completion-ports)
```html
When a process creates an I/O completion port, the system creates an associated queue object for threads whose sole purpose is to service these requests. Processes that handle many concurrent asynchronous I/O requests can do so more quickly and efficiently by using I/O completion ports in conjunction with a pre-allocated thread pool than by creating threads at the time they receive an I/O request.
```

[CreateIoCompletionPort() Microsoft doc](https://learn.microsoft.com/en-us/windows/win32/fileio/createiocompletionport#remarks)
```html
The I/O system can be instructed to send I/O completion notification packets to I/O completion ports, where they are queued. The CreateIoCompletionPort function provides this functionality.
```

And what information do we care when getting the finish information?

**The sock, and the data buffer** information.

Information is filled into GetQueuedCompletionStatus()'s two arguments when it returns. GetQueuedCompletionStatus()'s third argument gets the second type CreateIoCompletionPort() call's third argument. GetQueuedCompletionStatus()'s fourth argument gets the lpOverlapped argument when calling WSASend()/WSARecv().

So one point is how to pass the information needed to GetQueuedCompletionStatus() while satisfy the need to call WSASend()/WSARecv(). WSASend()/WSARecv() requires a lpOverlapped argument and it will be used by the os to accomplish the call. WSASend()/WSARecv() knows the **sock and buffer** information while lpOverlapped is passed into GetQueuedCompletionStatus(). CreateIoCompletionPort() has a CompletionKey argument. However, CompletionKey is not needed by CreateIoCompletionPort() to finish its function. Namely, it has no meaning and is customizable. CreateIoCompletionPort() knows the **sock** information.

Since CreateIoCompletionPort() sounds to have more similarity with **sock**'s life cycle. So lpCompletionKey is used to pass **sock** information. Another problem is, how to satisfy the need of WSASend()/WSARecv() of acquiring a lpOverlapped while using the lpOverlapped to pass the **buffer** information. The strategy is using the characteristic that **the struct has the same address as its first member**. So we design an extra struct to wrap an Overlapped as the first member and pass it to WSASend()/WSARecv() while the struct can contain buffer information with other members (In fact **sock** can also be passed by the struct).

Moreover, the buffer for the WSARecv() will next be echoed by WSASend(). But the buffer can't be reused by another WSARecv(). Since WSASend() and WSARecv() are asynchronous, the buffer of WSASend() might be still in use when another WSARecv() is called. (May be optimized by a buffer pool?)
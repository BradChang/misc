Behavior notes
==============
The application accepts new clients in the main thread. The accept callback runs in the main thread.
Henceforth a consistent worker thread handles all I/O on a given descriptor.
The application must not close the descriptor. Instead it should modify the flags: *flags |= TCPSRV_DO_CLOSE
Within the callback the application can modify the polling condition. Default is to poll readability.
There is a per-fd data slot for the callback's to use. They init, recycle it and fini it.

callback notes
==============

on_accept: TCPSRV_POLL_READ is set. 
           (You could OR in TCPSRV_POLL_WRITE if you want to immediately poll writability after accept).
           TCPSRV_DO_CLOSE if you want the server to close the the fd right immediately after the accept cb 
           TCPSRV_DO_EXIT may be set if you want to terminate the application gracefully
           TCPSRV_CAN_READ  is not tested at this point
           TCPSRV_CAN_WRITE is not tested at this point
on_data:
           TCPSRV_CAN_READ  is available
           TCPSRV_DO_CLOSE tells the server to run the upon_closure cb then close the fd 
           TCPSRV_DO_EXIT may be set if you want to terminate the application gracefully
           TCPSRV_CAN_WRITE is available
           TCPSRV_POLL_READ  may be set* (see note)
           TCPSRV_POLL_WRITE may be set* (see note)
           Note: omit TCPSRV_POLL_READ/WRITE bits to preserve current epoll test

Tests
=====
test1: simplest application, drain server, uses defaults
test2: sends greeting using accept cb, closes connection
test3: sends greeting, consumes data once, closes connection

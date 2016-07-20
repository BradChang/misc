Shared byte ring

This ring buffer resides in a memory mapped file allowing multiple processes to
share it.  The challenges of sharing include: how do processes rendezvous to
"find" the buffer (is it a pathname, etc); how is access synchronization or
locking done; does it persist only when a process is using it; does clean up
occur eventually; and how does it get initialized in the first place?

This implementation identifies a shared ring buffer by a pathname, because it
is a file.  But when a process is using it, it is mapped into memory. If the
file resides in a tmpfs then it is RAM resident without disk backing. To open
the same ring buffer, two processes need only know its pathname.

This implementation uses POSIX file locking as a global lock on the shared ring
buffer. This prevents concurrent access of any kind. 

Notification of readability is designed for select/poll/epoll compatibility.
This implementation creates a named pipe whose name is stored inside the
control area of the ring buffer.  When a process puts data into the ring buffer
it sends a byte to the named pipe listed there, if there is a blocked consumer
waiting for it.  This is compatible with clients based on select, poll or epoll.

Because this byte ring buffer is a file, it has filesystem persistence. Thus it
remains until deleted, or in the case of a tmpfs RAM filesystem, until the
system is rebooted.

The byte ring buffer must be initialized prior to use. This is done using an API call
that succeeds only if it creates the file exclusively. 

Because the ring buffer is a file, an ecosystem of command line utilities can be made
to operate on it. See the utilities directory.

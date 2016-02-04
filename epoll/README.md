This is a standard template for an epoll-based, signal handling,
event driven C program. It is minimal- it only waits for a signal.
Every few seconds it does background work. A real application
would put some application logic in and handling of additional
file descriptors.

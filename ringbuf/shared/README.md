Here's a link back to my [main GitHub page](http://troydhanson.github.io/).

# `shr_ring`: a multi-process ring buffer in C

This is a ring buffer in C for Linux. It is designed for use by multiple processes
communicating. The processes can be concurrent, or they can run at separate times.
In other words, the ring persists independently of the processes that use it. When
concurrent processes pass data through the ring, the data sharing occurs in memory.

The elements of the ring can be bytes or messages.  In message mode, it takes
or gives full messages (a buffer with a length) per read or write. It has a
mode to overwrite old data in the ring if you want that to happen
automatically, even if the data has not been read. It has blocking mode (the
default) and a non-blocking mode. In non-blocking mode, read/write returns
immediately rather than waiting for data or ring space.

The ring implementation is a memory-mapped file. (If you put the file in a
`tmpfs` filesystem like `/dev/shm` it resides in RAM without a disk backing).
Since the ring is a file, it has file persistence. POSIX file locking protects
the ring so that only one process can put data in it, or read data from it, at
a time.

A process can `select()` to wait for another process to place data in the ring.
The ring internally uses a pair of fifo's (in the same directory as the ring)
to signal I/O readiness. The fifo's get created automatically with the ring.

The source is in `shr_ring.h` and `shr_ring.c`. These are placed in the public
domain.  

Compatibility: `shr_ring` is written in C for Linux only. Whether it runs on
other POSIX platforms depends on whether they support opening a fifo in
read/write mode- see `fifo(7)`. 

Performance: no claims are made. On a laptop, rates between half a million
and two million reads and writes per second seemed typical.

## API

### Create 

Create a ring of a given size. The file is created a tiny bit larger for the
bookkeeping to reside in the file.

    int shr_init(char *file, size_t sz, int flags, ...);

The flags include:

    SHR_INIT_OVERWRITE
    SHR_INIT_KEEPEXIST
    SHR_INIT_MESSAGES
    SHR_INIT_LRU_STOMP

The first two describe how to handle an already-existing ring file. `SHR_INIT_OVERWRITE` 
clears an existing file and changes it to the given size. `SHR_INIT_KEEPEXIST`
uses the ring file if it exists already, leaving its content and size as is.

Message mode is enacted by setting `SHR_INIT_MESSAGES`. In this mode the read and write
operations operate on one full message at a time. Otherwise, the ring elements are bytes.

The `SHR_INIT_LRU_STOMP` mode enables automatic overwriting of the oldest ring data, when
data is being written to an already-full ring. (Normally the ring data must be consumed
by `shr_read` in order to be eligible for overwriting). It is possible to tell if data
was overwritten (meaning, data was dropped, before the reader could read it) using the
`shr_stat` function.

Initialization of the ring need only occur once, and can be done earlier and/or in a
separate process from those processes which eventually use it for read and write.

### Open

A process has to open the ring before it can read or write data to it.

    shr *shr_open(char *file, int flags);

This returns an opaque ring handle. The flags is 0 or a bitwise combination of:

    SHR_RDONLY
    SHR_WRONLY
    SHR_NONBLOCK
    SHR_SELECTFD

A reader uses `SHR_RDONLY` and a writer uses `SHR_WRONLY` - these are mutually exclusive.
The `SHR_NONBLOCK` flag causes read or write operations to return immediately, rather 
than block, when data or ring space is unavailable. (Technically, the read or write still
blocks to acquire the POSIX file lock on the ring, prior to determining data availability).
The `SHR_SELECTFD` mode, for readers only, enables a subsequent call to `shr_get_selectable_fd`
to get a file descriptor suitable for use with select, poll or epoll. That descriptor can
be used to tell when data in the ring is available. 

### Put data in

A process (that has the ring open for writing) can put data in this way:

    ssize_t shr_write(shr *s, char *buf, size_t len);

If the ring was initialized in message mode, then the buffer and length are considered
to comprise a single message; the message is eventually returned from `shr_read` intact.
In contrast, in the default byte mode, the data is considered an unstructured byte stream
that can be returned to later reads possibly in fragments.

Return value:

     > 0 (number of bytes copied into ring, always the full buffer)
     0   (insufficient space in ring, in non-blocking mode)
    -1   (error, such as the buffer exceeds the total ring capacity)
    -2   (signal arrived while blocked waiting for ring)

### Get data out

To read data from the ring, a process that has opened the ring for reading, can do:

    ssize_t shr_read(shr *s, char *buf, size_t len);

Return value:

     > 0 (number of bytes read from the ring)
     0   (ring empty, in non-blocking mode)
    -1   (error)
    -2   (signal arrived while blocked waiting for ring)
    -3   (buffer can't hold message; SHR_INIT_MESSAGE mode)

### Select on data availability 

A reader that has opened the ring in `SHR_RDONLY | SHR_SELECTFD | SHR_NONBLOCK` mode
can use this function to get a file descriptor suitable for use with select, poll or epoll.

    int shr_get_selectable_fd(shr *s);

The descriptor provides an edge triggered data availability notification. If the descriptor
becomes readable, the reader should call `shr_read` in a loop, until it returns 0, meaning 
the ring is empty. (Expect the possibility of it returning 0 immediately, without any data
being returned, because spurious notifications can occur. This can occur, for example, when
`SHR_INIT_LRU_STOMP` mode reclaims space for which a notification still remains pending).

### Close

To close the `shr_ring`, use:

    void shr_close(shr *s);

### Unlink

A process that intends to dispose of the ring can call this function prior to `shr_close`.
This function unlinks (removes) the ring file and the dual fifo's in the same directory.

    int shr_unlink(shr *s);

This function returns 0.

### Metrics

A set of numbers describing the ring in terms of total space, used space, and I/O counters,
can be obtained using this function. See the definition of `shr_stat` in `shr_ring.h`.

    int shr_stat(shr *s, struct shr_stat *stat, struct timeval *reset);

This function can, optionally, reset the stats and start a new period, by
passing a non-NULL pointer in the final argument.


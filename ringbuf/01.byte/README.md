Byte ring buffer

This ring buffer is a contiguous memory buffer of bytes- no content assumption is made.
The C functions permit data to be added to the ring, and consumed from the ring. The
ring buffer has behavior compatible with write(), namely, since write can accept fewer
bytes than it is given, this ring buffer allows fewer bytes than the full pending buffer
to be consumed. That is why there are functions to get the next chunk of pending data
and a separate function to mark it (or less than it) consumed. Note that if there are
data pending to consume, it may span two buffers -- the part leading up to the end of
the ring, and its subsequent part which has wrapped and starts at the ring's offset 0.
That is why the API function to get the next "chunk" is so named; it may require two
chunks to consume the whole pending buffer.

This ring buffer is meant for use in a single process. It is in heap memory. It has
no locks. Threads using it would need to mutex lock it themselves.

The ring size is specified when it is created. If the ring fills up because there
is inadequate consumption, attempts to put further data into the ring fail.

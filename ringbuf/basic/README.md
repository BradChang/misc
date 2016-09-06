Here's a link back to my [main GitHub page](http://troydhanson.github.io/).

# Byte ring buffer in C

This ring buffer is a contiguous memory buffer. The elements of the ring are
bytes. This ring buffer is meant for use in a single process. The source is in
`ringbuf.h` and `ringbuf.c`.  These are placed in the public domain.  

## API

### Create 

Create a ring of a given size:

    ringbuf *ringbuf_new(size_t sz);

### Put data in

The data is copied in, if it can fit. It returns 0 on success or -1 if there is
not enough room in the ring. It succeeds only if the whole length can fit.

    int ringbuf_put(ringbuf *r, const void *data, size_t len);

### Get data out

The function returns the size of the next available buffer to read, putting its
address in `data`.

    size_t ringbuf_get_next_chunk(ringbuf *r, char **data);

Note that you need to advance the read position afterward, to get a new chunk.

### Advance read position 

The read position advances when you mark a number of bytes as consumed. You can
consume fewer bytes than the whole chunk. When you consume bytes, it makes them
available for the ring to overwrite them.

    void ringbuf_mark_consumed(ringbuf *r, size_t len);

### Free

When you are done with the ring buffer, free it.

    void ringbuf_free(ringbuf *r);


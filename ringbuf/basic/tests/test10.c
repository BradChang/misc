#include <stdio.h>
#include "ringbuf.h"

char z[11] = {'a','b','c','d','e','f','g','h','i','j','k'};

int main() {
 ringbuf *r;
 size_t sz;
 int rc;
 char *d;
 char buf[32+11]; /* room for struct ring + 11 bytes of ring */

 r = ringbuf_take(buf,sizeof(buf));
 printf("gave buffer (ringbuf_take) expecting 11 free bytes\n");

 sz = ringbuf_get_freespace(r);
 printf("buffer has %lu free bytes\n", sz);

 printf("putting 7 bytes into buffer of sz 11\n");
 rc = ringbuf_put(r, z, 7);
 printf("put: %s\n", (rc == -1) ? "failed" : "ok");

 sz = ringbuf_get_freespace(r);
 printf("buffer has %lu free bytes\n", sz);

 printf("consuming 3\n");
 ringbuf_mark_consumed(r, 3);

 sz = ringbuf_get_freespace(r);
 printf("buffer has %lu free bytes\n", sz);

 sz = ringbuf_get_next_chunk(r, &d);
 printf("chunk sz %lu: sz, %.*s\n", sz, (int)sz, d);

 printf("marking clear\n");
 ringbuf_clear(r);

 sz = ringbuf_get_freespace(r);
 printf("buffer has %lu free bytes\n", sz);

 printf("nothing left\n");
 sz = ringbuf_get_pending_size(r);
 printf("pending size = %lu\n", sz);

 sz = ringbuf_get_freespace(r);
 printf("buffer has %lu free bytes\n", sz);

 /* do not ringbuf_free(r); since we used ringbuf_take */
 return 0;
}

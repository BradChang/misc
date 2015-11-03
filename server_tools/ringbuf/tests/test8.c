#include <stdio.h>
#include "ringbuf.h"

char z[11] = {'a','b','c','d','e','f','g','h','i','j','k'};

int main() {
 ringbuf *r;
 size_t sz;
 int rc;
 char *d;

 r = ringbuf_new(11);
 printf("putting 7 bytes into buffer of sz 11\n");
 rc = ringbuf_put(r, z, 7);
 printf("put: %s\n", (rc == -1) ? "failed" : "ok");
 printf("consuming 3\n");
 ringbuf_mark_consumed(r, 3);

 sz = ringbuf_get_next_chunk(r, &d);
 printf("chunk sz %lu: sz, %.*s\n", sz, (int)sz, d);

 printf("marking clear\n");
 ringbuf_clear(r);
 printf("nothing left\n");
 sz = ringbuf_get_pending_size(r);
 printf("size = %lu\n", sz);

 ringbuf_free(r);
 return 0;
}

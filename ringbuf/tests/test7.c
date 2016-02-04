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
 printf("consuming 7\n");
 ringbuf_mark_consumed(r, 7);
 printf("next put of > 4 bytes wraps buffer, writing 5\n");
 rc = ringbuf_put(r, z, 5); printf("put: %s\n", (rc == -1) ? "failed" : "ok");

 sz = ringbuf_get_next_chunk(r, &d);
 printf("chunk sz %lu: sz, %.*s\n", sz, (int)sz, d);
 printf("reading 3, of 5 available\n");
 ringbuf_mark_consumed(r, 3);

 printf("next read is last byte before wrapping\n");
 sz = ringbuf_get_next_chunk(r, &d);
 printf("chunk sz %lu: sz, %.*s\n", sz, (int)sz, d);
 ringbuf_mark_consumed(r, 1);

 printf("next read starts reading from wrapped part\n");
 sz = ringbuf_get_next_chunk(r, &d);
 printf("chunk sz %lu: sz, %.*s\n", sz, (int)sz, d);
 ringbuf_mark_consumed(r, 1);

 printf("nothing left\n");
 sz = ringbuf_get_pending_size(r);
 printf("size = %lu\n", sz);
 ringbuf_free(r);
 return 0;
}

#include <stdio.h>
#include "ringbuf.h"

char z[10] = {'a','b','c','d','e','f','g','h','i','j'};

int main() {
 ringbuf *r;
 size_t sz;
 int rc;
 char *d;

 r = ringbuf_new(11);
 printf("putting 7 bytes into buffer of sz 11\n");
 rc = ringbuf_put(r, z, 7);
 printf("put: %s\n", (rc == -1) ? "failed" : "ok");
 ringbuf_mark_consumed(r, 2);
 printf("next put of > 4 bytes wraps buffer, writing 6\n");
 rc = ringbuf_put(r, z, 6);
 printf("put 6: %s\n", (rc == -1) ? "failed" : "ok");
 printf("put fails now, til some bytes consumed\n");
 rc = ringbuf_put(r, z, 1);
 printf("put 1: %s\n", (rc == -1) ? "failed" : "ok");

 printf("get produces a chunk of 9, of 11 available\n");
 printf("the first contiguous buffer is 9, the next is 11\n");
 sz = ringbuf_get_next_chunk(r, &d);
 printf("chunk sz %lu: sz, %.*s\n", sz, (int)sz, d);
 ringbuf_mark_consumed(r,9);
 sz = ringbuf_get_next_chunk(r, &d);
 printf("chunk sz %lu: sz, %.*s\n", sz, (int)sz, d);

 ringbuf_free(r);
 return 0;
}

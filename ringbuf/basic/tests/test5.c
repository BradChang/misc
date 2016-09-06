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
 printf("consuming 2\n");
 ringbuf_mark_consumed(r, 2);
 printf("next put of > 4 bytes wraps buffer, writing 1\n");
 rc = ringbuf_put(r, z, 1); printf("put: %s\n", (rc == -1) ? "failed" : "ok");
 rc = ringbuf_put(r, z, 1); printf("put: %s\n", (rc == -1) ? "failed" : "ok");
 rc = ringbuf_put(r, z, 1); printf("put: %s\n", (rc == -1) ? "failed" : "ok");
 rc = ringbuf_put(r, z, 1); printf("put: %s\n", (rc == -1) ? "failed" : "ok");
 printf("about to wrap\n");
 rc = ringbuf_put(r, z, 1); printf("put: %s\n", (rc == -1) ? "failed" : "ok");
 rc = ringbuf_put(r, z, 1); printf("put: %s\n", (rc == -1) ? "failed" : "ok");
 printf("expect next put to fail, buffer full\n");
 rc = ringbuf_put(r, z, 1);
 printf("put 1: %s\n", (rc == -1) ? "failed" : "ok");

 printf("get a contiguous chunk of 9, of 11 available\n");
 sz = ringbuf_get_next_chunk(r, &d);
 printf("chunk sz %lu: sz, %.*s\n", sz, (int)sz, d);
 ringbuf_mark_consumed(r,9);
 printf("get a contiguous chunk of 2, of 2 available\n");
 sz = ringbuf_get_next_chunk(r, &d);
 printf("chunk sz %lu: sz, %.*s\n", sz, (int)sz, d);

 ringbuf_free(r);
 return 0;
}

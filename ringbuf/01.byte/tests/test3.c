#include <stdio.h>
#include "ringbuf.h"

char z[10] = {'a','b','c','d','e','f','g','h','i','j'};

int main() {
 ringbuf *r;
 size_t sz;
 int rc;
 char *d;

 r = ringbuf_new(20);
 rc = ringbuf_put(r, z, sizeof(z));
 printf("put: %s\n", (rc == -1) ? "failed" : "ok");

 sz = ringbuf_get_pending_size(r);
 printf("pending size %lu\n", sz);
 sz = ringbuf_get_next_chunk(r, &d);
 printf("next chunk size %lu\n", sz);

 printf("adding more data\n");
 rc = ringbuf_put(r, z, sizeof(z));
 printf("put: %s\n", (rc == -1) ? "failed" : "ok");

 sz = ringbuf_get_pending_size(r);
 printf("pending size %lu\n", sz);
 sz = ringbuf_get_next_chunk(r, &d);
 printf("next chunk size %lu\n", sz);
 printf("chunk: %.*s\n", (int)sz, d);

 printf("adding too much data\n");
 rc = ringbuf_put(r, z, sizeof(z));
 printf("put: %s\n", (rc == -1) ? "failed" : "ok");

 sz = ringbuf_get_pending_size(r);
 printf("pending size %lu\n", sz);
 sz = ringbuf_get_next_chunk(r, &d);
 printf("next chunk size %lu\n", sz);
 printf("chunk: %.*s\n", (int)sz, d);


 ringbuf_free(r);
 return 0;
}

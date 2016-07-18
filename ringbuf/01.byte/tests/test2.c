#include <stdio.h>
#include "ringbuf.h"

char z[10] = {'a','b','c','d','e','f','g','h','i','j'};

int main() {
 ringbuf *r;
 size_t sz;
 char *d;

 r = ringbuf_new(20);

 sz = ringbuf_get_pending_size(r);
 printf("pending size %lu\n", sz);
 sz = ringbuf_get_next_chunk(r, &d);
 printf("next chunk size %lu\n", sz);

 ringbuf_put(r, z, sizeof(z));
 sz = ringbuf_get_pending_size(r);
 printf("pending size %lu\n", sz);
 sz = ringbuf_get_next_chunk(r, &d);
 printf("next chunk size %lu\n", sz);
 printf("chunk: %.*s\n", (int)sz, d);

 printf("unconsumed; leaves pending\n");
 sz = ringbuf_get_pending_size(r);
 printf("pending size %lu\n", sz);
 sz = ringbuf_get_next_chunk(r, &d);
 printf("next chunk size %lu\n", sz);
 printf("chunk: %.*s\n", (int)sz, d);

 ringbuf_mark_consumed(r, 5);
 printf("marked 5 bytes consumed\n");

 sz = ringbuf_get_pending_size(r);
 printf("pending size %lu\n", sz);
 sz = ringbuf_get_next_chunk(r, &d);
 printf("next chunk size %lu\n", sz);
 printf("chunk: %.*s\n", (int)sz, d);

 ringbuf_mark_consumed(r, 5);
 printf("marked 5 bytes consumed\n");

 sz = ringbuf_get_pending_size(r);
 printf("pending size %lu\n", sz);
 sz = ringbuf_get_next_chunk(r, &d);
 printf("next chunk size %lu\n", sz);

 ringbuf_free(r);
 return 0;
}

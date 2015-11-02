#include "ringbuf.h"

ringbuf *ringbuf_new(size_t sz) {
  ringbuf *r = calloc(1, sizeof(*r) + sz);
  if (r == NULL) {
    fprintf(stderr,"out of memory\n");
    goto done;
  }

  r->n = sz;

 done:
  return r;
}

void ringbuf_free(ringbuf* r) {
  free(r);
}

/*
int ringbuf_put(ringbuf *, char *data, size_t len);
size_t ringbuf_get_pending_size(ringbuf *);
size_t ringbuf_get_next_chunk(ringbuf *, char **data);
void ringbuf_mark_consumed(ringbuf *, size_t);
*/


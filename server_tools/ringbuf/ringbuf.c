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

/* copy data in. fails if ringbuf has insuff space. */
#define MIN(a,b) (((a) < (b)) ? (a) : (b))
int ringbuf_put(ringbuf *r, char *data, size_t len) {
  size_t a,b,c;
  if (r->i < r->o) {  // available space is a contiguous buffer
    a = r->o - r->i; 
    if (len > a) return -1;
    memcpy(&r->d[r->i], data, len);
  } else {            // available space wraps; it's two buffers
    b = r->n - r->i;  // in-head to eob (receives leading input)
    c = r->o;         // out-head to in-head (receives trailing input)
    a = b + c;
    if (len > a) return -1;
    memcpy(&r->d[r->i], data, MIN(b, len));
    if (len > b) memcpy(r->d, &data[b], len-b);
  }
  r->i = (r->i + len) % r->n;
  return 0;
}

/*
size_t ringbuf_get_pending_size(ringbuf *);
size_t ringbuf_get_next_chunk(ringbuf *, char **data);
void ringbuf_mark_consumed(ringbuf *, size_t);
*/


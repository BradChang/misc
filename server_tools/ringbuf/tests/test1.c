#include <stdio.h>
#include "ringbuf.h"
int main() {
 ringbuf *r;
 r = ringbuf_new(100);
 ringbuf_free(r);
 return 0;
}

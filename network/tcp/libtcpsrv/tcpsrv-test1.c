#include <stdio.h>
#include "libtcpsrv.h"

typedef struct {
  int fd;
} slot_t;

tcpsrv_init_t parms = {
  .nthread=2,
  .maxfd = 20,
  .timeout = 10,
  .port = 1099,
  .sz = sizeof(slot_t),
};

int main() {
  int rc=-1;
  void *t;

  t=tcpsrv_init(&parms);

  if (t==NULL) {
    fprintf(stderr,"tcpsrv_init failed\n");
    goto done;
  }

  tcpsrv_fini(t);

 done:
  return rc;
}

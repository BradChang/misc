#include <stdio.h>
#include "libtcpsrv.h"

typedef struct {
  int fd;
} slot_t;

int rainy_day(int seconds, void *data) {
  //fprintf(stderr,"periodic function running\n",seconds);
  return 0;
}

tcpsrv_init_t parms = {
  //.verbose=1,
  .nthread=2,
  .maxfd = 13,
  .timeout = 10,
  .port = 1099,
  .slot_sz = sizeof(slot_t),
  .periodic_seconds = 1,
  .periodic = rainy_day,
  .cp_path = "./socket",
};

int main() {
  int rc=-1;
  void *t;

  t=tcpsrv_init(&parms); if (!t) goto done;
  tcpsrv_run(t);
  tcpsrv_fini(t);

 done:
  return rc;
}

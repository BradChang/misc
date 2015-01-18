#include <stdio.h>
#include "libtcpsrv.h"

typedef struct {
  int i; // unused
} slot_t;

void greet(tcpsrv_client_t *c, void *data, int *flags) {
  char buf[] = "welcome (" __FILE__ ")\n";
  slot_t *slot = (slot_t*)c->slot;
  fprintf(stderr,"accepting fd %d in main thread from %s\n", c->fd, c->ip_str);
  write(c->fd, buf, sizeof(buf));
  *flags |= TCPSRV_DO_CLOSE;
}

tcpsrv_init_t parms = {
  .verbose=1,
  .nthread=2,
  .maxfd = 13,
  .timeout = 10,
  .port = 1099,
  .slot_sz = sizeof(slot_t),
  .on_accept = greet, 
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

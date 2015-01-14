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
}

void data(tcpsrv_client_t *c, void *data, int *flags) {
  slot_t *slot = (slot_t*)c->slot;
  char buf[255];
  size_t nc=0;
  fprintf(stderr,"fd %d in worker thread\n", c->fd);
  fprintf(stderr,"fd %d readable: %c writable: %c\n", c->fd,
    (*flags & TCPSRV_CAN_READ) ? 'y' : 'n',
    (*flags & TCPSRV_CAN_WRITE) ? 'y' : 'n');
  if (*flags & TCPSRV_CAN_READ) nc=read(c->fd,buf,sizeof(buf));
  fprintf(stderr,"fd %d read %d bytes [%.*s]\n", c->fd, (int)nc, (int)nc, buf);
  *flags |= TCPSRV_DO_CLOSE;
}

tcpsrv_init_t parms = {
  .verbose=1,
  .nthread=2,
  .maxfd = 13,
  .timeout = 10,
  .port = 1099,
  .sz = sizeof(slot_t),
  .on_accept = greet, 
  .on_data = data, 
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

#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include "libtcpsrv.h"

typedef struct {
  int fd;
} slot_t;

#define NTHREAD 2

int fds[NTHREAD][2];

/* executed inside each thread for each of its fd-slots */
void invoke_cb(tcpsrv_client_t *client, void *ptr, void *data, int *flags) {
  char buf[1000];
  if (*flags & TCPSRV_OP_COMPLETE) {
    close(fds[client->thread_idx][1]); // close write end
    return;
  }
  snprintf(buf,sizeof(buf),"thread %d fdslot %d remote %s+%u\n", 
    client->thread_idx, client->fd, client->ip_str, client->port);
  write(fds[client->thread_idx][1],buf,strlen(buf));
  /* test the closure ability by closing odd-fd clients */
  if (client->fd & 1) *flags |= TCPSRV_DO_CLOSE;
}

int ask_cmd(void *cp, int argc, char **argv, void *t) {
  int n,rc;
  char buf[100];
  cp_printf(cp, "querying threads.\n");
  for(n=0; n<NTHREAD; n++) pipe(fds[n]);
  tcpsrv_invoke(t, fds);
  // read them all til eof
  for(n=0; n<NTHREAD; n++) {
    do {
      rc = read(fds[n][0],buf,sizeof(buf));
      if (rc >  0) cp_printf(cp,"%.*s",rc,buf);
      if (rc == 0) close(fds[n][0]); // eof
      if (rc < 0) cp_printf(cp,"\nread: %s\n",strerror(errno));
    } while (rc > 0);
  }
  return CP_OK;
}

void close_cb(tcpsrv_client_t *client, void *data) {
  fprintf(stderr,"fd %d thread %d closing\n", client->fd, client->thread_idx);
}

tcpsrv_init_t parms = {
  //.verbose=1,
  .nthread=NTHREAD,
  .maxfd = 20,
  .port = 1099,
  .sz = sizeof(slot_t),
  .on_invoke = invoke_cb,
  .on_close = close_cb,
  .cp_path = "/tmp/test.socket",
};

int main() {
  int rc=-1;
  void *t;

  t=tcpsrv_init(&parms); if (!t) goto done;
  cp_add_cmd(parms.cp, "ask", ask_cmd, "invoke thread callback", t);
  tcpsrv_run(t);
  tcpsrv_fini(t);

  rc = 0;

 done:
  return rc;
}

#include <stdio.h>
#include "libtcpsrv.h"

typedef struct {
  int fd;
} slot_t;

int rainy_day(int seconds, void *data) {
  //fprintf(stderr,"periodic function running\n",seconds);
  return 0;
}

int stats_cmd(void *cp, int argc, char **argv, void *_data) {
  int *count = (int*)_data;
  cp_printf(cp, "invocation %u", (*count)++);
  return CP_OK;
}
int halt_cmd(void *cp, int argc, char **argv, void *_data) {
  cp_printf(cp, "halting server\n");
  cp_printf(cp, "TODO\n");
  // FIXME need way to set t->shutdown
  return CP_OK;
}

tcpsrv_init_t parms = {
  //.verbose=1,
  .nthread=2,
  .maxfd = 13,
  .timeout = 10,
  .port = 1099,
  .sz = sizeof(slot_t),
  .periodic_seconds = 1,
  .periodic = rainy_day,
  .cp_path = "./socket",
};

int count=0;

int main() {
  int rc=-1;
  void *t;

  parms.data = &count;
  t=tcpsrv_init(&parms); if (!t) goto done;
  cp_add_cmd(parms.cp, "stats", stats_cmd, "show stats", &count);
  cp_add_cmd(parms.cp, "halt",  halt_cmd,  "terminate", NULL);
  tcpsrv_run(t);
  tcpsrv_fini(t);

 done:
  return rc;
}

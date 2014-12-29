#include "libtcpsrv.h"
#include "internal.h"

static int shutdown_cmd(void *cp, int argc, char *argv[], void *_t) {
  tcpsrv_t *t = (tcpsrv_t*)_t;
  t->shutdown=1;
}

void register_cp_cmds(tcpsrv_t *t) {
   cp_add_cmd(t->cp, "shutdown", shutdown_cmd, "shutdown server", t);
}


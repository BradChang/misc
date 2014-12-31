#include "libtcpsrv.h"
#include "internal.h"

/* thread status command */
static int th_cmd(void *cp, int argc, char *argv[], void *_t) {
  tcpsrv_t *t = (tcpsrv_t*)_t;
  int i,j;
  cp_printf(cp, "Number of threads: %u\n", t->p.nthread);
  for(i=0; i < t->p.nthread; i++) {
    time_t last = t->now - t->tc[i].pong;
    cp_printf(cp," %d: watchdog:%ds fds[", i, (int)last);
    for(j=0; j <= t->p.maxfd; j++) {
      if (BIT_TEST(t->tc[i].fdmask, j)) cp_printf(cp, "%d ", j);
    }
    cp_printf(cp," ]\n");
  }
  return CP_OK;
}

/* shutdown command */
static int shutdown_cmd(void *cp, int argc, char *argv[], void *_t) {
  tcpsrv_t *t = (tcpsrv_t*)_t;
  t->shutdown=1;
  return CP_OK;
}

void register_cp_cmds(tcpsrv_t *t) {
   cp_add_cmd(t->cp, "shutdown", shutdown_cmd, "stop server", t);
   cp_add_cmd(t->cp, "th", th_cmd, "thread status", t);
}


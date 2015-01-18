#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include "libtcpsrv.h"
#include "internal.h"

/* internally implemented control port commands */

/* TODO status: gives local IP's, uptime, #accepts, #active */
/* TODO histos: performance histograms */

/******************************************************************************
 * who: list active connections
 *      This control port command uses a trick to query the threads for all
 *      the fds they service. (We don't just walk those data structures from 
 *      the main thread because they are in flux as the threads manage them).
 *      The trick to the 'who' command is that it creates a temporary pipe(2)
 *      for every thread. It then reads data til-EOF on each of those pipes. 
 *      The data comes from the threads walking their active fd slots. The 
 *      signal for them to do this work, and how they find the pipe descriptors, 
 *      utilizes the internal send_workers_ptr capability. 
 *****************************************************************************/
/* helper to get the pointer to the pipe fd (p=0|1) for thread n */
#define thp(fds,n,p) ((char*)fds + ((n*2*sizeof(int))+(p*sizeof(int))))
/* executed inside each thread for each of its fd-slots */
void who_invoke_cb(tcpsrv_client_t *client, void *fds, void *data, int *flags) {
  char buf[1000];
  int fd = *(int*)thp(fds,client->thread_idx,1); // write end of pipe 
  if (*flags & TCPSRV_OP_COMPLETE) {
    close(fd); 
    return;
  }
  snprintf(buf,sizeof(buf),"thread %d fdslot %d remote %s+%u\n", 
    client->thread_idx, client->fd, client->ip_str, client->port);
  write(fd,buf,strlen(buf));
}
static int who_cmd(void *cp, int argc, char *argv[], void *_t) {
  tcpsrv_t *t = (tcpsrv_t*)_t;
  char buf[1000];
  int n,e,fd,rc=CP_CLOSE, *fds=NULL;

  fds = calloc(t->p.nthread, 2*sizeof(int));
  if (fds == NULL) {t->shutdown=1; goto done;}
  for(n=0; n<t->p.nthread; n++) { 
    /* we could hit a fd limit so handle that */
    if (pipe(thp(fds,n,0)) == -1) {
      e = errno;
      fprintf(stderr, "pipe: %s\n", strerror(e));
      cp_printf(cp, "pipe: %s\n", strerror(e));
      goto done;
    }
  }
  send_workers_ptr(t, WORKER_INVOKE, who_invoke_cb, fds);
  for(n=0; n<t->p.nthread; n++) { 
    fd = *(int*)thp(fds,n,0); // read end of pipe
    do {
      rc = read(fd,buf,sizeof(buf));
      if (rc >  0) cp_printf(cp,"%.*s",rc,buf);
      if (rc == 0) close(fd); // eof. close read end of pipe.
      if (rc < 0) cp_printf(cp,"\nread: %s\n",strerror(errno));
    } while (rc > 0);
  }
  rc = CP_OK;

 done:
  if (fds) free(fds);
  return rc;
}

/* thread status command. TODO improve this */
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

/* schedule imminent shutdown causing tcpsrv_run to return */
static int halt_cmd(void *cp, int argc, char *argv[], void *_t) {
  tcpsrv_t *t = (tcpsrv_t*)_t;
  t->shutdown=1;
  return CP_OK;
}

void register_cp_cmds(tcpsrv_t *t) {
   cp_add_cmd(t->cp, "who", who_cmd, "list connections", t);
   cp_add_cmd(t->cp, "halt", halt_cmd, "stop server", t);
   cp_add_cmd(t->cp, "th", th_cmd, "thread status", t);
}


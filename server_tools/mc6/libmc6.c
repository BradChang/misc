#include <assert.h>
#include <sys/un.h>
#include <sys/socket.h>
#include <errno.h>
#include "libmc6.h"
#include "utstring.h"
#include "utarray.h"
#include "uthash.h"
#include "tpl.h"

typedef struct {  // wraps a command structure 
  cp_cmd_t cmd;
  UT_hash_handle hh;
} cp_cmd_w;

typedef struct {
  struct sockaddr_un addr;
  cp_cmd_w *cmds; // hash table of commands
  int fd;         // listener descriptor
  void *data;     // opaque data pointer
  UT_array *fds;  // descriptors of connected clients
  // used in executing command callbacks 
  UT_string *s;
  UT_array *argv;
  int argc;
} cp_t;

/*******************************************************************************
 * default commands 
 ******************************************************************************/
static int quit_cmd(void *_cp, int argc, char **argv, void *data) {
  return CP_CLOSE;
}

static int help_cmd(void *_cp, int argc, char **argv, void *data) {
  cp_t *cp = (cp_t*)_cp;
  cp_cmd_w *cw, *tmp;
  HASH_ITER(hh, cp->cmds, cw, tmp) {
    cp_printf(_cp, "%-20s %s\n", cw->cmd.name, cw->cmd.help ? cw->cmd.help : "");
  }
  return CP_OK; 
}

static int unknown_cmd(void *_cp, int argc, char **argv, void *data) {
  if (argc > 0) cp_printf(_cp, "command not found\n");
  return CP_OK;
}

/*******************************************************************************
 * library API
 ******************************************************************************/
void *cp_init(char *path, cp_cmd_t *cmds, void *data, int *fd) {
  cp_cmd_t *cmd;
  int rc=-1;
  cp_t *cp;
  
  if ( (cp=malloc(sizeof(cp_t))) == NULL) goto done;
  memset(cp, 0, sizeof(*cp));

  cp->data = data;
  utstring_new(cp->s);
  utarray_new(cp->fds, &ut_int_icd);
  utarray_new(cp->argv,&ut_ptr_icd);

  cp->addr.sun_family = AF_UNIX;
  strncpy(cp->addr.sun_path, path, sizeof(cp->addr.sun_path)-1);
  if (*path != '\0') unlink(path);

  if ( (cp->fd = socket(AF_UNIX, SOCK_STREAM, 0)) == -1) {
    fprintf(stderr,"socket: %s\n",strerror(errno)); goto done;
  }

  if (bind(cp->fd, (struct sockaddr*)&cp->addr, sizeof(cp->addr)) == -1) {
    fprintf(stderr,"bind: %s\n",strerror(errno)); goto done;
  }

  if (listen(cp->fd, 5) == -1) {
    fprintf(stderr,"listen: %s\n",strerror(errno)); goto done;
  }

  for(cmd=cmds; cmd && cmd->name; cmd++) {
    cp_add_cmd(cp,cmd->name,cmd->cmdf,cmd->help);
  }
  cp_add_cmd(cp, "help", help_cmd, "this text");
  cp_add_cmd(cp, "quit", quit_cmd, "close session");

  *fd = cp->fd;  // app should poll on it
  rc= 0;

 done:
  if (rc < 0) {
    if (cp) {
      if (cp->s) utstring_free(cp->s);
      if (cp->fd > 0) close(cp->fd);
      free(cp);
    }
    cp=NULL;
  }
  return cp;
}

void cp_add_cmd(void *_cp, char *name, cp_cmd_f *cmd, char *help) {
  cp_t *cp = (cp_t*)_cp;
  cp_cmd_w *cw;

  /* create new command if it isn't in the hash; else update in place */
  HASH_FIND(hh, cp->cmds, name, strlen(name), cw);
  if (cw == NULL) {
    cw = calloc(1,sizeof(*cw));
    if (cw == NULL) { fprintf(stderr,"out of memory\n"); exit(-1); }
    cw->cmd.name = strdup(name);
    HASH_ADD_KEYPTR(hh, cp->cmds, cw->cmd.name, strlen(cw->cmd.name), cw);
  }
  cw->cmd.cmdf = cmd;
  if (cw->cmd.help) free(cw->cmd.help);
  cw->cmd.help = help ? strdup(help) : NULL;
}

static int do_cmd(cp_t *cp, int fd, int pos) {
  char *arg,**v=NULL,**argv;
  cp_cmd_f *cmd;
  cp_cmd_w *cw;
  tpl_node *tn;
  tpl_bin bin;
  int rc,tc;

  /* dequeue the client argv buffer */
  tn = tpl_map("A(s)", &arg);
  if (tpl_load(tn, TPL_FD, fd) < 0) {
    // client closed, or protocol error
    goto close_client;
  }

  /* prepare the argc/argv array */ 
  cp->argc = tpl_Alen(tn,1);
  utarray_clear(cp->argv);
  while (tpl_unpack(tn,1) > 0) utarray_push_back(cp->argv, &arg);
  tpl_free(tn);

  /* clear response buffer, lookup callback, run it, cleanup */
  utstring_clear(cp->s);
  argv = (char**)utarray_front(cp->argv);
  if (argv) HASH_FIND_STR(cp->cmds, *argv, cw);
  cmd = (argv && cw) ?  cw->cmd.cmdf : unknown_cmd;
  rc = cmd(cp, cp->argc, argv, cp->data);
  while ( (v=(char**)utarray_next(cp->argv,v))) free(*v);

  /* send response buffer */
  bin.addr = utstring_body(cp->s);
  bin.sz   = utstring_len(cp->s);
  tn = tpl_map("B", &bin);
  tpl_pack(tn,0);
  tc = tpl_dump(tn, TPL_FD, fd);
  tpl_free(tn);
  if (tc < 0) goto close_client;
  if (rc == CP_OK) return 0;

 close_client: // erase record of client descriptor. close client.
  utarray_erase(cp->fds, pos, 1);
  close(fd);
  return -fd; // notify app to STOP polling fd 
}

/*******************************************************************************
* cp_service
*  called by app when 'fd' is ready
*  fd is either the listening fd or a connected fd (previously returned from us)
*  this function returns x:
*    x=0 on 'normal' 
*    x<0 to tell app "stop polling fd x, I closed it"
*    x>0 to tell app "start polling fd x, I accepted it"
*  any other communication from callbacks to the app can use the opaque *data
*******************************************************************************/
int cp_service(void*_cp, int fd) {
  cp_t *cp = (cp_t*)_cp;
  int rc,*cd,nd,pos;
  cp_cmd_f *cmd;

  /* if the ready-fd is the _listening_ socket, we need to accept the
   * new client. then tell app to poll the new descriptor nd */
  if (fd == cp->fd) { 
    if ( (nd = accept(fd, NULL, NULL)) < 0) {
      fprintf(stderr,"accept: %s\n",strerror(errno));
      return 0;
    }
    utarray_push_back(cp->fds, &nd);    // record new descriptor 
    return nd;                           // notify app to poll nd
  }

  /* if we're here, confirm fd is one of our connected clients. */
  cd=NULL;
  while( (cd=(int*)utarray_next(cp->fds,cd))) if (*cd == fd) break;
  if (cd == NULL) {
    fprintf(stderr,"fd %d: unknown control port client\n", fd);
    assert(0);
  }

  pos = utarray_eltidx(cp->fds,cd);
  rc = do_cmd(cp, fd, pos);
  return rc;
}

void cp_fini(void *_cp) {
  cp_t *cp = (cp_t*)_cp;

  /* free up command structures */
  cp_cmd_w *cw, *tmp;
  HASH_ITER(hh, cp->cmds, cw, tmp) {
    HASH_DEL(cp->cmds, cw);
    free(cw->cmd.name);
    if (cw->cmd.help) free(cw->cmd.help);
    free(cw);
  }

  /* terminate connected clients */
  int *cd=NULL;
  while( (cd=(int*)utarray_next(cp->fds,cd))) close(*cd);

  /* free everything, stop listening */
  utarray_free(cp->fds);
  utarray_free(cp->argv);
  utstring_free(cp->s);
  close(cp->fd);
  free(cp);
}

void cp_printf(void *_cp, char *fmt, ...) { 
  cp_t *cp = (cp_t*)_cp;
  va_list ap;
  va_start(ap, fmt);
  utstring_printf_va(cp->s, fmt, ap);
  va_end(ap);
}


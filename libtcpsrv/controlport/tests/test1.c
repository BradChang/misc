#include <stdio.h>
#include <string.h>
#include <sys/epoll.h>
#include "libcontrolport.h"

int shutdown;

int halt_fcn(void *cp, int argc, char **argv, void *data) {
  shutdown=1;
  return CP_OK;
}
int count_fcn(void *cp, int argc, char **argv, void *data) {
  int *count = (int*)data;
  cp_printf(cp, "invocations: %d\n", (*count)++);
  return CP_OK;
}


int count=0;

int main(int argc, char *argv[]) {
  void *cp;
  int fd,epoll_fd, rc;
  struct epoll_event ev;
  cp = cp_init("/tmp/cp", &fd);
  cp_add_cmd(cp, "halt", halt_fcn, "halts the server", NULL);
  cp_add_cmd(cp, "count", count_fcn, "counts invocations", &count);

  epoll_fd = epoll_create(1);
  memset(&ev,0,sizeof(ev));
  ev.events = EPOLLIN;
  ev.data.fd = fd;
  epoll_ctl(epoll_fd, EPOLL_CTL_ADD, fd, &ev);

  while (epoll_wait(epoll_fd, &ev, 1, -1) > 0) {
    rc = cp_service(cp, ev.data.fd);
    if (rc > 0) {
      ev.events = EPOLLIN;
      ev.data.fd = rc;
      epoll_ctl(epoll_fd, EPOLL_CTL_ADD, rc, &ev);
    } 
    // if rc < 0, since client closed, epoll automatically deleted!
    // if (rc < 0) epoll_ctl(epoll_fd, EPOLL_CTL_DEL, -rc, &ev);
    if (shutdown) break;
  }

  cp_fini(cp);
  return 0;
}

#include <stdio.h>
#include <sys/epoll.h>
#include "libcontrolport.h"

int shutdown;

int main(int argc, char *argv[]) {
  void *cp;
  int fd,epoll_fd, rc;
  struct epoll_event ev;
  cp = cp_init("/tmp/cp", NULL, NULL, &fd);

  epoll_fd = epoll_create(1);
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

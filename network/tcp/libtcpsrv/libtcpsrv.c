#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <assert.h>
#include <errno.h>
#include <sys/signalfd.h>
#include <sys/epoll.h>
#include "libtcpsrv.h"

/* signals that we'll accept synchronously via signalfd */
static int sigs[] = {SIGHUP,SIGTERM,SIGINT,SIGQUIT,SIGALRM};

static int setup_listener(tcpsrv_t *t) {
  int rc = -1, one=1;

  int fd = socket(AF_INET, SOCK_STREAM, 0);
  if (fd == -1) {
    fprintf(stderr,"socket: %s\n", strerror(errno));
    goto done;
  }

  /**********************************************************
   * internet socket address structure: our address and port
   *********************************************************/
  struct sockaddr_in sin;
  sin.sin_family = AF_INET;
  sin.sin_addr.s_addr = t->p.addr;
  sin.sin_port = htons(t->p.port);

  /**********************************************************
   * bind socket to address and port 
   *********************************************************/
  setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one));
  if (bind(fd, (struct sockaddr*)&sin, sizeof(sin)) == -1) {
    fprintf(stderr,"bind: %s\n", strerror(errno));
    goto done;
  }

  /**********************************************************
   * put socket into listening state
   *********************************************************/
  if (listen(fd,1) == -1) {
    fprintf(stderr,"listen: %s\n", strerror(errno));
    goto done;
  }

  t->fd = fd;
  rc=0;

 done:
  if ((rc < 0) && (fd != -1)) close(fd);
  return rc;
}

static int add_epoll(int epoll_fd, int events, int fd) {
  int rc;
  struct epoll_event ev;
  memset(&ev,0,sizeof(ev)); // placate valgrind
  ev.events = events;
  ev.data.fd= fd;
  //fprintf(stderr,"adding fd %d to epoll\n", fd);
  rc = epoll_ctl(epoll_fd, EPOLL_CTL_ADD, fd, &ev);
  if (rc == -1) {
    fprintf(stderr,"epoll_ctl: %s\n", strerror(errno));
  }
  return rc;
}

static int del_epoll(int epoll_fd, int fd) {
  int rc;
  struct epoll_event ev;
  rc = epoll_ctl(epoll_fd, EPOLL_CTL_DEL, fd, &ev);
  if (rc == -1) {
    fprintf(stderr,"epoll_ctl: %s\n", strerror(errno));
  }
  return rc;
}

static void periodic() {
}

void *tcpsrv_init(tcpsrv_init_t *p) {
  int rc=-1,n;

  tcpsrv_t *t = calloc(1,sizeof(*t)); if (!t) goto done;
  t->p = *p; // struct copy
  t->slots = calloc(p->maxfd+1, p->sz); 
  if (t->slots == NULL) goto done;
  if (p->slot_init) p->slot_init(t->slots, p->maxfd+1, p->data);
  t->th = calloc(p->nthread,sizeof(pthread_t));
  if (t->th == NULL) goto done;

  sigfillset(&t->all);  /* set of all signals */
  sigemptyset(&t->few); /* set of signals we accept */
  for(n=0; n < sizeof(sigs)/sizeof(*sigs); n++) sigaddset(&t->few, sigs[n]);

  /* create the signalfd for receiving signals */
  t->signal_fd = signalfd(-1, &t->few, 0);
  if (t->signal_fd == -1) {
    fprintf(stderr,"signalfd: %s\n", strerror(errno));
    goto done;
  }

  /* set up the epoll instance */
  t->epoll_fd = epoll_create(1); 
  if (t->epoll_fd == -1) {
    fprintf(stderr,"epoll: %s\n", strerror(errno));
    goto done;
  }

  /* create thread areas */
  t->tc = calloc(t->p.nthread,sizeof(tcpsrv_thread_t));
  if (t->tc == NULL) goto done;
  for(n=0; n < t->p.nthread; n++) {
    t->tc[n].epoll_fd = epoll_create(1);
    if (t->tc[n].epoll_fd == -1) {
      fprintf(stderr,"epoll: %s\n", strerror(errno));
      goto done;
    }
  }

  rc=0;

 done:
  if (rc < 0) {
    fprintf(stderr, "tcpsrv_init failed\n");
    if (t) {
      if (t->slots) free(t->slots);
      if (t->th) free(t->th);
      if (t->signal_fd > 0) close(t->signal_fd);
      if (t->epoll_fd > 0) close(t->epoll_fd);
      if (t->tc) {
        for(n=0; n < t->p.nthread; n++) {
          if (t->tc[n].epoll_fd > 0) close(t->tc[n].epoll_fd);
        }
        free(t->tc);
      }
      free(t);
      t=NULL;
    }
  }
  return t;
}

int tcpsrv_run(void *_t) {
  tcpsrv_t *t = (tcpsrv_t*)_t;
  struct signalfd_siginfo info;
  struct epoll_event ev;
  int rc=-1,n;

  pthread_sigmask(SIG_SETMASK, &t->all, NULL);                  // block all signals 
  if (setup_listener(t)) goto done; 
  if (add_epoll(t->epoll_fd, EPOLLIN, t->fd))        goto done; // listening socket
  if (add_epoll(t->epoll_fd, EPOLLIN, t->signal_fd)) goto done; // signal socket


  // TODO start threads
  //for(i=0; i < p->nthread; i++) pthread_create(&t->th[i],NULL,worker,(void*)i);
  // TODO epoll on accept fd or active conns
  //      each thread has a maxfd-sized array that tells it which fd's to service
  //      and a vector version 
  //      on ready, enqueue work to appropriate thread, remove from epoll interest

  alarm(1);
  while (epoll_wait(t->epoll_fd, &ev, 1, -1) > 0) {

    /* if a signal was sent to us, read its signalfd_siginfo */
    if (ev.data.fd == t->signal_fd) { 
      if (read(t->signal_fd, &info, sizeof(info)) != sizeof(info)) {
        fprintf(stderr,"failed to read signal fd buffer\n");
        continue;
      }
      switch (info.ssi_signo) {
        case SIGALRM: 
          if ((++t->ticks % 10) == 0) periodic(); 
          alarm(1); 
          continue;
        default:  /* exit */
          fprintf(stderr,"got signal %d\n", info.ssi_signo);  
          goto done;
      }
    }

#if 0
    /* regular POLLIN. handle the particular descriptor that's ready */
    assert(ev.events & EPOLLIN);
    if (t->verbose) fprintf(stderr,"handle POLLIN on fd %d\n", ev.data.fd);
    if (ev.data.fd == t->fd) accept_client();
    else drain_client(ev.data.fd);
#endif
  }

 done:
  return rc;
}

// does each thread epoll on its established clients.
// and main thread does a sneaky mod epoll on the threads' epoll object
// each thread has one epoll instance open to pipe to parent

void tcpsrv_fini(void *_t) {
  int n;
  tcpsrv_t *t = (tcpsrv_t*)_t;
  free(t->slots);
  free(t->th);
  close(t->signal_fd);
  close(t->epoll_fd);
  for(n=0; n < t->p.nthread; n++) {
    close(t->tc[n].epoll_fd);
  }
  free(t->tc);
  free(t);
}

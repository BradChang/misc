#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <assert.h>
#include <errno.h>
#include <sys/signalfd.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "libtcpsrv.h"
#include "internal.h"

/* signals that we'll accept synchronously via signalfd */
static int sigs[] = {SIGHUP,SIGTERM,SIGINT,SIGQUIT,SIGALRM};

static int setup_listener(tcpsrv_t *t) {
  int rc = -1, one=1;

  int fd = socket(AF_INET6, SOCK_STREAM, 0);
  if (fd == -1) {
    fprintf(stderr,"socket: %s\n", strerror(errno));
    goto done;
  }

  /**********************************************************
   * internet socket address structure: our address and port
   *********************************************************/
  struct sockaddr_in6 sin;
  memset(&sin,0,sizeof(sin));
  sin.sin6_family = AF_INET6;
  sin.sin6_port = htons(t->p.port);
  // sin.sin6_addr.s6_addr = t->p.addr; TODO

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
  rc = epoll_ctl(epoll_fd, EPOLL_CTL_ADD, fd, &ev);
  if (rc == -1) {
    fprintf(stderr,"epoll_ctl: %s\n", strerror(errno));
  }
  return rc;
}

static int mod_epoll(int epoll_fd, int events, int fd) {
  int rc;
  struct epoll_event ev;
  memset(&ev,0,sizeof(ev)); // placate valgrind
  ev.events = events;
  ev.data.fd= fd;
  rc = epoll_ctl(epoll_fd, EPOLL_CTL_MOD, fd, &ev);
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

/* send a byte to each worker on the control pipe */
static void send_workers(tcpsrv_t *t, char op) {
  int n;
  for(n=0; n < t->p.nthread; n++) {
    if (write(t->tc[n].pipe_fd[1], &op, sizeof(op)) == -1) {
      fprintf(stderr,"control pipe write: %s\n", strerror(errno));
      t->shutdown=1;
    }
  }
}

void send_workers_ptr(tcpsrv_t *t, char op, 
   void (*on_invoke)(tcpsrv_client_t *client, void *ptr, void *data, int *flags),
   void *ptr) {
  int n,rc;

  char buf[sizeof(op)+sizeof(on_invoke)+sizeof(ptr)];
  buf[0] = op; 
  memcpy(&buf[sizeof(op)], &on_invoke, sizeof(on_invoke));
  memcpy(&buf[sizeof(op)+sizeof(on_invoke)], &ptr, sizeof(ptr));

  for(n=0; n < t->p.nthread; n++) {
    rc = write(t->tc[n].pipe_fd[1], buf, sizeof(buf));
    if (rc != sizeof(buf)) {
      fprintf(stderr,"control pipe: %s\n", (rc<0) ? strerror(errno) : "full");
      t->shutdown=1;
    }
  }
}


static void periodic(tcpsrv_t *t) {
  /* at 1hz we we send a byte on the control pipe to each worker.
   * if dead worker, pipe eventually fills, inducing shutdown. */
  send_workers(t,WORKER_PING);
  /* invoke the low-frequency app periodic callback, if due */
  if (t->p.periodic == NULL) return;
  if (t->p.periodic_seconds == 0) return;
  if (t->ticks % t->p.periodic_seconds) return;
  if (t->p.periodic(t->ticks, t->p.data) == -1) t->shutdown=1;
}

/* drain is a built-in default used if app has no on_data callback */
static void drain(tcpsrv_client_t *client, void *data, int *flags) {
  int rc, pos, *fp;
  char buf[1024];

  rc = read(client->fd, buf, sizeof(buf));
  switch(rc) { 
    default: fprintf(stderr,"received %d bytes\n", rc);         break;
    case  0: fprintf(stderr,"fd %d closed\n", client->fd);      break;
    case -1: fprintf(stderr, "recv: %s\n", strerror(errno));    break;
  }

  if (rc == 0) {
    *flags |= TCPSRV_DO_CLOSE;
  }
}

static void accept_client(tcpsrv_t *t) { // always in main thread
  int fd;
  void *p;
  struct sockaddr_in6 in;
  socklen_t sz = sizeof(in);

  fd = accept(t->fd,(struct sockaddr*)&in, &sz);
  if (fd == -1) {
    fprintf(stderr,"accept: %s\n", strerror(errno)); 
    goto done;
  }

  if (fd > t->p.maxfd) {
    if (t->p.verbose) fprintf(stderr,"overload fd %d > %d\n", fd, t->p.maxfd);
    t->num_overloads++;
    close(fd);
    goto done;
  }

  if (fd > t->high_watermark) t->high_watermark = fd;
  int thread_idx = fd % t->p.nthread;
  t->num_accepts++;

  /* fill the client structure that we expose to the application */
  tcpsrv_client_t *c = &t->si[fd].client;
  c->thread_idx = thread_idx;
  c->fd = fd;
  c->slot = fd_slot(t,fd);
  memcpy(&c->sa, &in, sz);
  if (!inet_ntop(AF_INET6, &in.sin6_addr, c->ip_str, sizeof(c->ip_str))) {
    fprintf(stderr,"inet_ntop: %s\n",strerror(errno)); 
    t->shutdown=1;
  }
  c->port = ntohs(in.sin6_port);
  c->accept_ts = t->now;

  /* if the app has an on-accept callback, invoke it. */ 
  int flags = TCPSRV_POLL_READ;
  if (t->p.on_accept) {
    t->p.on_accept(&t->si[fd].client, t->p.data, &flags);
    if (flags & TCPSRV_DO_EXIT) t->shutdown=1;
    if (flags & TCPSRV_DO_CLOSE) {
      if (t->p.on_close) t->p.on_close(&t->si[fd].client, t->p.data);
      close(fd);
    }
    if (flags & (TCPSRV_DO_EXIT | TCPSRV_DO_CLOSE)) goto done;
  }

  /* hand all further I/O on this fd to the worker. */
  BIT_SET(t->tc[thread_idx].fdmask, fd);
  int events = 0;
  if (flags & TCPSRV_POLL_READ)  events |= EPOLLIN;
  if (flags & TCPSRV_POLL_WRITE) events |= EPOLLOUT;
  if (add_epoll(t->tc[thread_idx].epoll_fd, events, fd) == -1) { 
    fprintf(stderr,"can't give accepted connection to thread %d\n", thread_idx);
    // close not needed; fd closed in fdmask sweep at exit
    t->shutdown=1;
  }

 done:
  return;
}

void *tcpsrv_init(tcpsrv_init_t *p) {
  int rc=-1,n;

  /* alloc. the internal data structure that keeps whole state */
  tcpsrv_t *t = calloc(1,sizeof(*t)); if (!t) goto done;
  t->p = *p; // struct copy

  /* set defaults for unspecified parameters in tcpsrv_init_t */
  if (t->p.on_data == NULL) t->p.on_data = drain;
  if (p->nthread == 0) p->nthread=1;
  if (p->maxfd == 0) p->maxfd=100; /* TODO query ulimit */

  /* initialize slots and internal slot_info */
  t->slots = calloc(p->maxfd+1, p->slot_sz); 
  if ((p->slot_sz > 0) && (t->slots == NULL)) goto done;
  if (p->slot_init) p->slot_init(t->slots, p->maxfd+1, p->data);
  t->si = calloc(p->maxfd+1, sizeof(tcpsrv_slotinfo_t)); 
  if (t->si == NULL) goto done;

  /* create thread structures. they get started later. */
  t->th = calloc(p->nthread,sizeof(pthread_t));
  if (t->th == NULL) goto done;
  t->tc = calloc(t->p.nthread,sizeof(tcpsrv_thread_t));
  if (t->tc == NULL) goto done;
  for(n=0; n < t->p.nthread; n++) {
    t->tc[n].thread_idx = n;
    t->tc[n].t = t;
    t->tc[n].fdmask = calloc(1,bytes_nbits(p->maxfd+1));
    if (t->tc[n].fdmask == NULL) goto done;
    if (pipe(t->tc[n].pipe_fd) == -1) goto done;
    t->tc[n].epoll_fd = epoll_create(1);
    if (t->tc[n].epoll_fd == -1) {
      fprintf(stderr,"epoll: %s\n", strerror(errno));
      goto done;
    }
  }

  /* signal handling setup. use signalfd to take signals in epoll */
  sigfillset(&t->all);  /* all signals */
  sigemptyset(&t->few); /* signals we accept */
  for(n=0; n < sizeof(sigs)/sizeof(*sigs); n++) sigaddset(&t->few, sigs[n]);
  t->signal_fd = signalfd(-1, &t->few, 0);
  if (t->signal_fd == -1) {
    fprintf(stderr,"signalfd: %s\n", strerror(errno));
    goto done;
  }

  /* set up the primary epoll instance, servicing the main thread */
  t->epoll_fd = epoll_create(1); 
  if (t->epoll_fd == -1) {
    fprintf(stderr,"epoll: %s\n", strerror(errno));
    goto done;
  }

  /* set up the control port, if configured to have one */
  if (t->p.cp_path) {
    if ( (t->cp = cp_init(t->p.cp_path, &t->cp_fd)) == NULL) {
      fprintf(stderr,"cp_init: failed\n");
      goto done;
    }
    p->cp = t->cp; /* expose the control port handle */
    t->cp_clients = calloc(1, bytes_nbits(p->maxfd+1));
    if (t->cp_clients == NULL) goto done;

    register_cp_cmds(t);
  }


  rc=0;

 done:
  if (rc < 0) {
    fprintf(stderr, "tcpsrv_init failed\n");
    if (t) {
      if (t->slots) free(t->slots);
      if (t->th) free(t->th);
      if (t->si) free(t->si);
      if (t->signal_fd > 0) close(t->signal_fd);
      if (t->epoll_fd > 0) close(t->epoll_fd);
      if (t->cp) cp_fini(t->cp);
      if (t->cp_clients) free(t->cp_clients);
      if (t->tc) {
        for(n=0; n < t->p.nthread; n++) {
          if (t->tc[n].pipe_fd[0] > 0) close(t->tc[n].pipe_fd[0]);
          if (t->tc[n].pipe_fd[1] > 0) close(t->tc[n].pipe_fd[1]);
          if (t->tc[n].epoll_fd > 0) close(t->tc[n].epoll_fd);
          if (t->tc[n].fdmask) free(t->tc[n].fdmask);
        }
        free(t->tc);
      }
      free(t);
      t=NULL;
    }
  }
  return t;
}

/* helper to handle DO_EXIT/DO_CLOSE flags from on_data or on_invoke cb */
static int do_flags(tcpsrv_t *t, tcpsrv_thread_t *tc, int fd, int flags) {
  if ((flags & (TCPSRV_DO_EXIT | TCPSRV_DO_CLOSE)) == 0) return 0;
  if (flags & TCPSRV_DO_EXIT) t->shutdown=1; // main checks at @1hz
  if (flags & TCPSRV_DO_CLOSE) {
    if (t->p.on_close) t->p.on_close(&t->si[fd].client, t->p.data);
    BIT_CLEAR(tc->fdmask, fd);
    close(fd); /* at this instant, fd could be re-used- don't use */
  }
  return 1;
}

static void *worker(void *_tc) {
  tcpsrv_thread_t *tc = (tcpsrv_thread_t*)_tc;
  int thread_idx = tc->thread_idx, i, flags;
  struct epoll_event ev;
  tcpsrv_t *t = tc->t;
  void *rv=NULL, *ptr;
  char op;
  /* fcn pointer */
  void (*invoke_cb)(tcpsrv_client_t *client, void *ptr, void *data, int *flags);

  if (t->p.verbose) fprintf(stderr,"thread %d starting\n", thread_idx);

  /* block all signals */
  sigset_t all;
  sigfillset(&all);
  pthread_sigmask(SIG_BLOCK, &all, NULL);

  /* listen to parent */
  if (add_epoll(tc->epoll_fd, EPOLLIN, tc->pipe_fd[0])) goto done;

  /* event loop */
  while (epoll_wait(tc->epoll_fd, &ev, 1, -1) > 0) {

    if (t->p.verbose) {
      fprintf(stderr,"thread %d %s %s fd %d\n", thread_idx, 
        (ev.events & EPOLLIN ) ? "IN " : "   ", 
        (ev.events & EPOLLOUT) ? "OUT" : "   ", ev.data.fd);
    }

    /* is I/O from the from main thread on the control pipe? */ 
    if (ev.data.fd == tc->pipe_fd[0]) { 
      if (read(tc->pipe_fd[0],&op,sizeof(op)) != sizeof(op)) goto done;
      switch(op) {
        case WORKER_PING: tc->pong = t->now; break;
        case WORKER_SHUTDOWN: goto done; break;
        case WORKER_INVOKE:
          if (read(tc->pipe_fd[0],&invoke_cb,sizeof(invoke_cb)) != sizeof(invoke_cb)) goto done;
          if (read(tc->pipe_fd[0],&ptr,sizeof(ptr)) != sizeof(ptr)) goto done;
          /* run "on_invoke" cb on each of this thread's active slots */
          for(i=0; i <= t->p.maxfd; i++) {
            if (BIT_TEST(tc->fdmask,i) == 0) continue;
            if (invoke_cb) {
              flags = 0;
              invoke_cb(&t->si[i].client, ptr, t->p.data, &flags);
              do_flags(t,tc,i,flags);
            }
          }
          /* invoke cb one last time on a faux slot to indicate iteration done*/
          tcpsrv_client_t end = { .slot=NULL, .thread_idx=thread_idx };
          flags = TCPSRV_OP_COMPLETE;
          if (invoke_cb) invoke_cb(&end, ptr, t->p.data, &flags);
          break;
      }
      continue;
    }

    /* regular I/O. */
    char *slot = fd_slot(t,ev.data.fd);
    flags = 0;
    if (ev.events & EPOLLIN)  flags |= TCPSRV_CAN_READ;
    if (ev.events & EPOLLOUT) flags |= TCPSRV_CAN_WRITE;
    t->p.on_data(&t->si[ev.data.fd].client, t->p.data, &flags); 

    /* did app set terminal condition or close fd? */
    if (do_flags(t,tc,ev.data.fd,flags)) continue;

    /* did app modify poll condition? (set neither bit to keep current poll) */
    if (flags & (TCPSRV_POLL_READ | TCPSRV_POLL_WRITE)) {
      int events = 0;
      if (flags & TCPSRV_POLL_READ)  events |= EPOLLIN;
      if (flags & TCPSRV_POLL_WRITE) events |= EPOLLOUT;
      if (mod_epoll(tc->epoll_fd, events, ev.data.fd)) goto done;
    }
  }

 done:
  /* thread exit is induced at impending program termination.
   * close any open descriptors still in service by this thread. */
  if (t->p.verbose) fprintf(stderr,"thread %d exiting\n", thread_idx);
  for(i=0; i <= t->p.maxfd; i++) {
    if (BIT_TEST(tc->fdmask,i) == 0) continue;
    if (t->p.on_close) t->p.on_close(&t->si[i].client, t->p.data);
    BIT_CLEAR(tc->fdmask,i); 
    close(i); 
  }
  return rv;
}

/* here is a lot going on under the hood. cp_service handles any control 
 * port i/o. If the ready-fd is the listening descriptor, it accepts the
 * new connection, and returns (rc > 0) a new fd that we should poll. If
 * rc<0 its an fd that we should stop polling; libcontrolport closed it.
 * BUT that's done for us; kernel removed it from epoll when fd closed. */
static void handle_control(tcpsrv_t *t, int fd) {
  int rc;

  rc = cp_service(t->cp, fd);
  if (rc < 0) BIT_CLEAR(t->cp_clients, -rc);
  if (rc <=0) return;

  BIT_SET(t->cp_clients, rc);
  if (add_epoll(t->epoll_fd, EPOLLIN, rc)) t->shutdown=1;
}

static void handle_signal(tcpsrv_t *t) {
  struct signalfd_siginfo info;
  int rc = -1;

  if (read(t->signal_fd, &info, sizeof(info)) != sizeof(info)) {
    fprintf(stderr,"failed to read signal fd buffer\n");
    goto done;
  }

  switch (info.ssi_signo) {
    case SIGALRM: 
      t->now = time(NULL);
      t->ticks++;
      periodic(t);
      alarm(1); 
      break;
    default:  /* exit */
      fprintf(stderr,"got signal %d\n", info.ssi_signo);  
      goto done;
  }
  rc = 0;

 done:
  if (rc < 0) t->shutdown=1;
}

int tcpsrv_run(void *_t) {
  tcpsrv_t *t = (tcpsrv_t*)_t;
  struct epoll_event ev;
  int rc=-1,n;

  pthread_sigmask(SIG_SETMASK, &t->all, NULL);                  // block all signals 
  if (setup_listener(t)) goto done; 
  if (add_epoll(t->epoll_fd, EPOLLIN, t->fd))        goto done; // listening socket
  if (add_epoll(t->epoll_fd, EPOLLIN, t->signal_fd)) goto done; // signal socket
  if (t->cp) {
    if (add_epoll(t->epoll_fd, EPOLLIN, t->cp_fd))   goto done; // control port 
  }


  for(n=0; n < t->p.nthread; n++) {
    void *data = &t->tc[n];
    pthread_create(&t->th[n],NULL,worker,data);
  }

  alarm(1);
  while (epoll_wait(t->epoll_fd, &ev, 1, -1) > 0) {

    assert(ev.events & EPOLLIN);
    if (t->p.verbose) fprintf(stderr,"POLLIN fd %d\n", ev.data.fd);

    if      (ev.data.fd == t->fd)                 accept_client(t);
    else if (ev.data.fd == t->signal_fd)          handle_signal(t);
    else if (ev.data.fd == t->cp_fd)              handle_control(t,t->cp_fd);
    else if (BIT_TEST(t->cp_clients, ev.data.fd)) handle_control(t,ev.data.fd);
    else {
      fprintf(stderr,"epoll fd %d unrecognized\n", ev.data.fd);
      t->shutdown=1;
    }

    if (t->shutdown) {
      send_workers(t,WORKER_SHUTDOWN);
      rc = 0;
      goto done;
    }
  }

 done:
  return rc;
}


void tcpsrv_fini(void *_t) {
  int n,rc;
  struct timespec max_wait = {.tv_sec=10,.tv_nsec=0};
  tcpsrv_t *t = (tcpsrv_t*)_t;
  for(n=0; n < t->p.nthread; n++) { // wait for thread term 
    rc=pthread_timedjoin_np(t->th[n],NULL,&max_wait);
    if (rc == -1) fprintf(stderr,"pthread_join %d: %s\n",n,strerror(errno));
    else if (t->p.verbose) fprintf(stderr,"thread %d exited\n",n);
  }
  if (t->p.slot_fini) t->p.slot_fini(t->slots, t->p.maxfd+1, t->p.data);
  if (t->slots) free(t->slots);
  close(t->signal_fd);
  close(t->epoll_fd);
  if (t->cp) cp_fini(t->cp);
  if (t->cp_clients) free(t->cp_clients);
  for(n=0; n < t->p.nthread; n++) {
    close(t->tc[n].pipe_fd[0]);
    close(t->tc[n].pipe_fd[1]);
    close(t->tc[n].epoll_fd);
    free(t->tc[n].fdmask);
  }
  free(t->th);
  free(t->si);
  free(t->tc);
  free(t);
}

/****************************************************************************** 
 * special API - for use within control port callbacks 
 ******************************************************************************/
// start shutdown, causing tcpsrv_run to return eventually 
void tcpsrv_shutdown(void *_t) { 
   tcpsrv_t *t = (tcpsrv_t*)_t;
   t->shutdown=1;
}
// queue each thread to run on_invoke cb on each of their active slots
void tcpsrv_invoke(void *_t, void *ptr) { 
  tcpsrv_t *t = (tcpsrv_t*)_t;
  send_workers_ptr(t,WORKER_INVOKE,t->p.on_invoke,ptr);
}


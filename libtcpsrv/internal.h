#ifndef __TCPSRV_INTERNAL_H__
#define __TCPSRV_INTERNAL_H__

#include <pthread.h>
#include <signal.h>
#include "libtcpsrv.h"

/* a few macros used to implement a mask of n bits. */
/* test/set/clear the bit at index i in bitmask c */
#define BIT_TEST(c,i)  (c[i/8] &   (1 << (i % 8)))
#define BIT_SET(c,i)   (c[i/8] |=  (1 << (i % 8)))
#define BIT_CLEAR(c,i) (c[i/8] &= ~(1 << (i % 8)))
#define bytes_nbits(n) ((n/8) + ((n % 8) ? 1 : 0))

#define fd_slot(t,fd) (t->slots + (fd * t->p.slot_sz))

typedef struct {
  int thread_idx;
  int epoll_fd;
#define WORKER_PING     'p'
#define WORKER_SHUTDOWN 's'
#define WORKER_INVOKE   'i'
  int pipe_fd[2];  // to main thread; [0]=child read end, [1]=parent write end 
  time_t pong;     // timestamp of last thread ping-reply
  struct _tcpsrv_t *t;
  char *fdmask;    // bit mask of fds owned by this thread
} tcpsrv_thread_t;

typedef struct {
  tcpsrv_client_t client;  /* this structure is exposed to the application */ 
  /* management stuff that should stay internal to libtcpsrv can go here */
} tcpsrv_slotinfo_t;

typedef struct _tcpsrv_t {
  tcpsrv_init_t p;
  time_t now;       /* incremented @ 1hz */
  int signal_fd;    /* how we accept our signals */
  int epoll_fd;     /* for main thread, signalfd, listener etc */
  int fd;           /* listener fd */
  int ticks;        /* global time ticker */
  int shutdown;     /* can be set in any thread to induce global shutdown */
  /* we use these to integrate the external control port library */
  int cp_fd;        /* control port listener descriptor */
  void *cp;         /* control port handle */
  char *cp_clients; /* bit mask of connected client descriptors */
  /* the set of all signals, and the smaller set of signals we accept. */
  sigset_t all;
  sigset_t few;
  /* there are 'n' threads spawned to service I/O. here we have an array
   * of the threads themselves, and an array of our thread management struct */
  pthread_t *th;
  tcpsrv_thread_t *tc; 
  /* some statistics */
  int num_accepts;   
  int num_overloads; /* rejects due to max fd exceeded */
  int num_rejects;   /* TODO rejects due to ACL */
  int high_watermark;/* max fd that has been accepted */
  /* Each connection gets its own slot for app data. It is entirely handled
   * by the application through callbacks. There is also a lib counterpart,
   * slotinfo, where this library stores its own info about the connection.*/
  tcpsrv_slotinfo_t *si;
  char *slots;
} tcpsrv_t;

/* prototypes */
void register_cp_cmds(tcpsrv_t *t);

#endif //__TCPSRV_INTERNAL_H__

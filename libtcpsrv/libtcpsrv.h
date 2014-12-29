#include <sys/socket.h>
#include <netinet/in.h>
#include <pthread.h>
#include <signal.h>
#include "libcontrolport.h"
/*****************************************************************************
 * libtcpsrv                                                                 *
 * Provides a threaded TCP server. Interface to application is via callbacks *
 ****************************************************************************/
typedef struct {
  int verbose;
  int nthread;          /* how many threads to create */
  int maxfd;            /* max file descriptor number we can service */
  int timeout;          /* shutdown silent connections after seconds TODO */
  int port;             /* to listen on */
  in_addr_t addr;       /* IP address to listen on */ // TODO or interface
  int sz;               /* size of structure for each active descriptor */
  void *data;           /* opaque */
  int periodic_seconds; /* how often to invoke periodic cb, if any */
  /* control port */
  char *cp_path;        /* path - unix domain socket, may be null */
  void *cp;             /* output: exposes control port handle for cp_add_cmd */
  /* callbacks into the application. may be NULL.  */
  void (*slot_init)(void *slot, int nslots, void *data);           // at program startup
  void (*on_accept)(void *slot, int fd, struct sockaddr_in6 *sa, void *data, int *flags);   // app should renew the slot
  void (*on_data)(void *slot, int fd, void *data, int *flags);     // app should consume/emit data
  void (*on_close)(void *slot, int fd, void *data);                // cleanup slot at fd closure
  void (*slot_fini)(void *slot, int nslots, void *data);           // at program termination
  int  (*periodic)(int uptime, void *data);                        // app periodic callback 
} tcpsrv_init_t;

/* these are values for flags in the callbacks */
#define TCPSRV_DO_CLOSE     (1 << 0)
#define TCPSRV_DO_CLOSE_RST (1 << 1)  // TODO
#define TCPSRV_POLL_READ    (1 << 2)
#define TCPSRV_POLL_WRITE   (1 << 3)
#define TCPSRV_CAN_READ     (1 << 4)
#define TCPSRV_CAN_WRITE    (1 << 5)
#define TCPSRV_DO_EXIT      (1 << 6)

typedef struct {
  int thread_idx;
  int epoll_fd;
#define WORKER_PING     'p'
#define WORKER_SHUTDOWN 's'
  int pipe_fd[2];  // to main thread; [0]=child read end, [1]=parent write end 
  time_t pong;     // timestamp of last thread ping-reply
  struct _tcpsrv_t *t;
  char *fdmask;    // bit mask of fds owned by this thread
} tcpsrv_thread_t;

typedef struct {
  struct sockaddr_in6 sa; /* describes the remote endpoint */
  time_t accept_ts;       /* unix time of socket acceptance*/
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

void *tcpsrv_init(tcpsrv_init_t *p);
int tcpsrv_run(void *_t);
void tcpsrv_fini(void *_t);


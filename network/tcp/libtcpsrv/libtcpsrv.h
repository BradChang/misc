#include <sys/socket.h>
#include <netinet/in.h>
#include <pthread.h>
#include <signal.h>
/*****************************************************************************
 * libtcpsrv                                                                 *
 * Provides a threaded TCP server. Interface to application is via callbacks *
 * into the init routine.                                                    *
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
  /* callbacks into the application. may be NULL.  THREADS CALL THESE- 
     callbacks should confine their r/w to the slot! */
  void (*slot_init)(void *slot, int nslots, void *data);
  void (*on_accept)(void *slot, int fd, void *data, int *flags);   // app should clean the slot
  void (*on_data)(void *slot, int fd, void *data, int *flags);     // app should consume/emit data
  void (*upon_close)(void *slot, int fd, void *data);              // cleanup slot at fd closure
  void (*slot_fini)(void *slot, int nslots, void *data);           // at program termination
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
  struct _tcpsrv_t *t;
} tcpsrv_thread_t;

typedef struct _tcpsrv_t {
  tcpsrv_init_t p;
  int signal_fd;
  int epoll_fd;     /* for main thread, signalfd, listener etc */
  int fd;           /* listener fd */
  int ticks;
  int shutdown;     /* can be set in any thread to induce global shutdown */
  char *slots;      /* app I/O context; a slot belongs to just one thread */
  sigset_t all;     /* the set of all signals */
  sigset_t few;     /* just the signals we accept */
  pthread_t *th;
  tcpsrv_thread_t *tc; /* per-thread control */
  int num_accepts;  /* cumulative counter */
  int num_overloads;/* rejects due to max fd exceeded, TODO rejects due to ACL */
} tcpsrv_t;

void *tcpsrv_init(tcpsrv_init_t *p);
int tcpsrv_run(void *_t);
void tcpsrv_fini(void *_t);


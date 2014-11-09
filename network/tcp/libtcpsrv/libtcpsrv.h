#include <sys/socket.h>
#include <netinet/in.h>
#include <pthread.h>
#include <signal.h>
/*****************************************************************************
 * libtcpsrv                                                                 *
 * Provides a threaded TCP server. Interface to application is via callbacks *
 * into the init routine.                                                    *
 *   tcpsrv_init                                                          *
 *   tcpsrv_run                                                           *
 ****************************************************************************/
typedef struct {
  int verbose;
  int nthread;          /* how many threads to create */
  int maxfd;            /* max file descriptor number we can service */
  int timeout;          /* shutdown silent connections after seconds */
  int port;             /* to listen on */
  in_addr_t addr;       /* IP address to listen on */ // TODO or interface
  int sz;               /* size of structure for each active descriptor */
  void *data;           /* opaque */
  void (*slot_init)(void *slot, int nslots, void *data);
  void (*on_accept)(void *slot, int fd, void *data);
  void (*on_data)(void *slot, int fd, void *data);
  void (*after_close)(void *slot, int fd, void *data);
} tcpsrv_init_t;

typedef struct {
  int thread_idx;
  int epoll_fd;
  struct _tcpsrv_t *t;
} tcpsrv_thread_t;

typedef struct _tcpsrv_t {
  tcpsrv_init_t p;
  int signal_fd;
  int epoll_fd;     /* for main thread, signalfd, listener etc */
  int fd;           /* listener fd */
  int ticks;
  int shutdown;     /* can be set in any thread to induce global shutdown */
  char *slots;
  sigset_t all;     /* the set of all signals */
  sigset_t few;     /* just the signals we accept */
  pthread_t *th;
  tcpsrv_thread_t *tc; /* per-thread control */
  /* TODO num_accepts, overloads (max fd exceeded), rejects (access list) */
} tcpsrv_t;

void *tcpsrv_init(tcpsrv_init_t *p);
int tcpsrv_run(void *_t);
void tcpsrv_fini(void *_t);

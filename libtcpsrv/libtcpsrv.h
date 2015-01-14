#ifndef __TCPSRV_H__
#define __TCPSRV_H__

#include <sys/socket.h>
#include <netinet/in.h>
#include "libcontrolport.h"

/* we expose this structure to application describing the connected client */
typedef struct {
  int thread_idx;                     /* thread servicing this client */
  int fd;                             /* descriptor to client */
  void *slot;                         /* app slot for client */
  struct sockaddr_in6 sa;             /* remote addr, port, */
  char ip_str[INET6_ADDRSTRLEN];      /* presentation form */
  int port;                           /* in host order     */
  time_t accept_ts;                   /* time of socket acceptance */
} tcpsrv_client_t;

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
  void (*slot_fini)(void *slot, int nslots, void *data);           // at program termination
  void (*on_accept)(tcpsrv_client_t *client, void *data, int *flags); // app should renew the slot
  void (*on_data)(tcpsrv_client_t *client, void *data, int *flags);   // app should consume/emit data
  void (*on_close)(tcpsrv_client_t *client, void *data);              // cleanup slot at fd closure
  int  (*periodic)(int uptime, void *data);                           // app periodic callback 
  void (*on_invoke)(tcpsrv_client_t *client, void *ptr, void *data, int *flags); // special purpose
} tcpsrv_init_t;

/* these are values for flags in the callbacks */
#define TCPSRV_DO_CLOSE     (1 << 0)
#define TCPSRV_DO_CLOSE_RST (1 << 1)  // TODO
#define TCPSRV_POLL_READ    (1 << 2)
#define TCPSRV_POLL_WRITE   (1 << 3)
#define TCPSRV_CAN_READ     (1 << 4)
#define TCPSRV_CAN_WRITE    (1 << 5)
#define TCPSRV_DO_EXIT      (1 << 6)
#define TCPSRV_OP_COMPLETE  (1 << 7)  // used in "on_invoke" cb to signal slot iteration done */

/*******************************************************************************
 * API
 ******************************************************************************/
void *tcpsrv_init(tcpsrv_init_t *p);
int tcpsrv_run(void *_t);
void tcpsrv_fini(void *_t);
/* initiate shutdown of the tcp server. shortly afterward tcpsrv_run should 
 * return. this function is meant for use only in control port callbacks.
 * but it could be called from a signal handler or another thread and be
 * expected to work since it just sets a flag in the tcpserver state. */
void tcpsrv_shutdown(void *_t);
/* used within a control port callback, this causes each io-thread to invoke
 * the on_invoke cb for each active fd slot. this function exists because
 * the slots belong to each thread. we don't inspect them from the main thread,
 * since that would require locking. the ptr parameter can be anything, such as
 * to a memory buffer that the thread should populate (by indexing into by 
 * thread_idx), or a file descriptor array that the threads should respond on.*/
void tcpsrv_invoke(void *_t, void *ptr);

#endif //__TCPSRV_H__

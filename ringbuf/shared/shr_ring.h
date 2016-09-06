#ifndef _SHARED_RING_H_
#define _SHARED_RING_H_

#include <sys/time.h> /* struct timeval (for stats) */

#if defined __cplusplus
extern "C" {
#endif

/* opaque data structure */
struct shr;
typedef struct shr shr;

/* stats structure */
struct shr_stat {

  /* this set of stats reflects the current period. when the caller resets the
   * the period (by calling shr_start with a reset value) these stats get zeroed. 
   */
  struct timeval start; /* start of the stats period (last reset) */
  size_t bw, br;        /* cumulative bytes written to/read from ring in period */
  size_t mw, mr;        /* cumulative messages written to/read from ring in period */
  size_t md, bd;        /* in lru stomp mode: messages dropped/bytes dropped */

  /* this set of numbers reflect the ring state at the moment shr_stat is called,
   * in other words, resetting the current period has no bearing on these numbers. 
   */
  size_t bn;            /* ring size in bytes */
  size_t bu;            /* current unread bytes (ready to read) in ring */
  size_t mu;            /* current unread messages (ready to read) in ring */
};

int shr_init(char *file, size_t sz, int flags, ...);
shr *shr_open(char *file, int flags);
int shr_get_selectable_fd(shr *s);
ssize_t shr_read(shr *s, char *buf, size_t len);
ssize_t shr_write(shr *s, char *buf, size_t len);
void shr_close(shr *s);
int shr_unlink(shr *s);
int shr_stat(shr *s, struct shr_stat *stat, struct timeval *reset);

/* shr_init flags - global to the ring */
#define SHR_INIT_OVERWRITE (1U << 0)
#define SHR_INIT_KEEPEXIST (1U << 1)
#define SHR_INIT_MESSAGES  (1U << 2)
#define SHR_INIT_LRU_STOMP (1U << 3)

/* shr_open flags */
#define SHR_RDONLY   (1U << 0)
#define SHR_WRONLY   (1U << 1)
#define SHR_NONBLOCK (1U << 2)
#define SHR_SELECTFD (1U << 3)

#if defined __cplusplus
}
#endif

#endif

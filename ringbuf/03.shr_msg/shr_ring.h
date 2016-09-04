#ifndef _SHARED_RING_H_
#define _SHARED_RING_H_

#if defined __cplusplus
extern "C" {
#endif

/* opaque data structure */
struct shr;
typedef struct shr shr;

int shr_init(char *file, size_t sz, int flags, ...);
shr *shr_open(char *file, int flags);
int shr_get_selectable_fd(shr *s);
ssize_t shr_read(shr *s, char *buf, size_t len);
ssize_t shr_write(shr *s, char *buf, size_t len);
void shr_close(shr *s);
int shr_unlink(shr *s);

/* shr_init flags */
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

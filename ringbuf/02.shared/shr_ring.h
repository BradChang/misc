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
ssize_t shr_read(shr *r, char *buf, size_t len);
ssize_t shr_write(shr *r, char *buf, size_t len);
void shr_close(shr *r);

/* shr_open flags */
#define SHR_RDONLY   (1U << 0)
#define SHR_WRONLY   (1U << 1)
#define SHR_NONBLOCK (1U << 2)

#if defined __cplusplus
}
#endif

#endif

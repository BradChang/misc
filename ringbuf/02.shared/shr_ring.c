#include <sys/types.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/file.h>
#include <unistd.h>
#include <limits.h>
#include <stdlib.h>
#include <assert.h>
#include <dirent.h>
#include <errno.h>
#include <stdio.h>
#include <libgen.h>
#include "shared_ring.h"

/* preamble region of ring file is the control block.  fixed length so we can
 * easily know where the data region starts even if we version the structure
 * that sits at offset 0. note the min size is a bit arbitrary; it has to be
 * at least a byte bigger than the control block. might as well be n pages.
 */
#define CONTROL_BLOCK_SZ 4096
#define MIN_RING_SZ (2*CONTROL_BLOCK_SZ)
#define CREAT_MODE 0644

static char ring_magic[] = "aredringsh";

/* this struct is mapped to the beginning of the mmap'd file. 
 * the control block is a fixed length block and this is the
 * structure we put inside it. this is shared state among the
 * processes using the ring. it is only to be used under lock.
 */
#define SR_WAITER (1 << 0)
typedef struct {
  char magic[sizeof(ring_magic)];
  int version;
  int flags;
} shr_ctrl;

/* this handle is given to each shr_open caller */
struct shr {
  char *name;    /* file name */
  struct stat s; /* file stat */
  int fd;        /* descriptor */
  union {
    char *buf;   /* mmap'd area */
    shr_ctrl *ctrl;
  };
  ringbuf *ring;
};

static void oom_exit(void) {
  fprintf(stderr, "out of memory\n");
  abort();
}

/* get the lock on the ring file. we use one lock for any read or write, 
 * because even the reader adjusts the position in the ring buffer. note,
 * we use a blocking wait (F_SETLKW) for the lock. this should be obtainable
 * quickly because a locked reader should read and release. however, if a
 * signal comes in while we await the lock, fcntl can return EINTR. since we
 * are a library, we do not alter the application's signal handling behavior.
 * rather, we propagate the "error" up to the application to deal with. 
 *
 * also note, since this is a POSIX file lock, anything that closes the 
 * descriptor (such as killing the application holding the lock) releases it.
 */
static int lock(int fd) {
  int rc = -1;
  const struct flock f = { .l_type = F_WRLCK, .l_whence = SEEK_SET, };

  if (fcntl(fd, F_SETLKW, &f) < 0) {
    fprintf(stderr, "fcntl (lock acquisiton failed): %s\n", strerror(errno));
    goto done;
  }

  rc = 0;
  
 done:
  return rc;
}

static int unlock(int fd) {
  int rc = -1;
  const struct flock f = { .l_type = F_UNLCK, .l_whence = SEEK_SET, };

  if (fcntl(fd, F_SETLK, &f) < 0) {
    fprintf(stderr, "fcntl (lock release failed): %s\n", strerror(errno));
    goto done;
  }

  rc = 0;

 done:
  return rc;
}

/* dirname and basename are destructive and return volatile memory. wrap them up
 */
static int dirbasename(char *file, char *out, size_t outlen, int dir) {
  char tmp[PATH_MAX], *c;
  int rc = -1;

  size_t l = strlen(file) + 1;
  if (l > sizeof(tmp)) {
    fprintf(stderr, "file name too long\n");
    goto done;
  }

  strncpy(tmp, file, sizeof(tmp));
  c = dir ? dirname(tmp) : basename(tmp);
  l = strlen(c) + 1;
  if (l > outlen) goto done;
  memcpy(out, c, l);

  rc = 0;

 done:
  return rc;
}

/* a ring file has a corresponding 'bell'. this is a notification mechanism
 * to wake up a blocked reader (or writer) when blocking is in effect. the
 * bell is a byte transmitted through a fifo; the data itself is in the ring.
 *
 * the name of the bell is stored in the in the control area in the ring. 
 * it is setup in this function which occurs in the initialization phase.
 * we keep the name there so other ring-using processes can open it easily. 
 *
 */
int make_bell(char *file, shr_ctrl *ctrl) {
  int rc = -1;

  rc = 0;

 done:
  return rc;
}

/*
 * shr_init creates a ring file
 *
 * The current implementation succeeds only if the file is created new.
 * Attempts to resize an existing file or init an existing file, even 
 * of the same size, fail.
 *
 */
int shr_init(const char *file, size_t file_sz, int flags, ...) {
  char *buf = NULL;
  int rc = -1;

  if (file_sz < MIN_RING_SZ) {
    fprintf(stderr,"shr_init: too small; min size: %ld\n", (long)MIN_RING_SZ);
    goto done;
  }

  size_t ring_sz = file_sz - CONTROL_BLOCK_SZ;

  int fd = open(file, O_RDWR|O_CREAT|O_EXCL, CREAT_MODE);
  if (fd == -1) {
    fprintf(stderr,"open %s: %s\n", file, strerror(errno));
    goto done;
  }

  if (lock(fd) < 0) goto done; /* close() below releases lock */

  if (ftruncate(fd, file_sz) < 0) {
    fprintf(stderr,"ftruncate %s: %s\n", file, strerror(errno));
    goto done;
  }

  buf = mmap(0, file_sz, PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0);
  if (buf == MAP_FAILED) {
    fprintf(stderr, "mmap %s: %s\n", file, strerror(errno));
    goto done;
  }

  char *ring = buf + CONTROL_BLOCK_SZ;
  if (ringbuf_take(ring, ring_sz) == NULL) {
    fprintf(stderr, "ringbuf_take: error\n");
    goto done;
  }

  /* init control block */
  shr_ctrl *ctrl = (shr_ctrl *)buf; 
  memcpy(ctrl->magic, magic, sizeof(magic));
  ctrl->version = 1;

  /* the 'bell' notifies blocked waiters */
  if (make_bell(file, ctrl->pname) < 0) goto done;

  rc = 0;

 done:
  if ((rc < 0) && (fd != -1)) unlink(file);
  if (fd != -1) close(fd);
  if (buf && (buf != MAP_FAILED)) munmap(buf, file_sz);
  return rc;
}


static int validate_ring(struct shr *r) {
  int rc = -1;

  if (r->s.st_size < MIN_RING_SZ) goto done;
  if (memcmp(r->ctrl->magic,magic,sizeof(magic))) goto done;

  /* check evident ring size vs internally stored size, and validate offsets */
  size_t ring_sz = r->s.st_size - (CONTROL_BLOCK_SZ + sizeof(ringbuf));
  if (ring_sz       != r->ring->n) goto done; /* sz != file_sz - overhead */
  if (r->ring->u >  r->ring->n) goto done; /* used > size */
  if (r->ring->i >= r->ring->n) goto done; /* input position >= size */
  if (r->ring->o >= r->ring->n) goto done; /* output position >= size */

  /* TODO verify existince of fifo, sanity check control block, etc. */

  rc = 0;

 done:
  if (rc < 0) fprintf(stderr,"invalid ring: %s\n", r->name);
  return rc;
}

struct shr *shr_open(const char *file) {
  int rc = -1;

  struct shr *r = malloc( sizeof(struct shr) );
  if (shr == NULL) oom_exit();
  memset(r, 0, sizeof(*r));

  r->name = strdup(file);
  r->fd = open(file, O_RDWR);
  if (r->fd == -1) {
    fprintf(stderr,"open %s: %s\n", file, strerror(errno));
    goto done;
  }

  if (fstat(r->fd, &r->s) == -1) {
    fprintf(stderr,"stat %s: %s\n", file, strerror(errno));
    goto done;
  }

  r->buf = mmap(0, r->s.st_size, PROT_READ|PROT_WRITE, MAP_SHARED, r->fd, 0);
  if (r->buf == MAP_FAILED) {
    fprintf(stderr, "mmap %s: %s\n", file, strerror(errno));
    goto done;
  }

  r->ring = (ringbuf*)(r->buf + CONTROL_BLOCK_SZ);
  if (validate_ring(r) < 0) goto done;

  rc = 0;

 done:
  if (rc < 0) {
    if (r->name) free(r->name);
    if (r->fd != -1) close(r->fd);
    if (r->buf && (r->buf != MAP_FAILED)) munmap(r->buf, r->s.st_size);
    free(r);
    r = NULL;
  }
  return r;
}

ssize_t shr_write(struct shr *r, char *buf, size_t len) {
  int rc = -1;

  if (lock(r->fd) < 0) goto done;
  
  /* TODO */

  if (unlock(r->fd) < 0) goto done;

  rc = 0;

 done:
  return rc;
}

/*
 * shr_read
 *
 * If blocking, wait until data is available, read it, return 0.
 * If non-blocking, if data available, handle as above, else return 1.
 * On error, return -1.
 * On signal, return -2.
 */
ssize_t shr_read(struct shr *r, char *buf, size_t len) {
  int rc = -1;

  if (lock(r->fd) < 0) goto done;
  /* TODO read */
  if (unlock(r->fd) < 0) goto done;

  rc = 0;

 done:
  return rc;
}

void shr_close(struct shr *r) {
  if (r->name) free(r->name);
  if (r->fd != -1) close(r->fd);
  if (r->buf && (r->buf != MAP_FAILED)) munmap(r->buf, r->s.st_size);
  free(r);
}


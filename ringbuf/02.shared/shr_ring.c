#include <sys/types.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/file.h>
#include <unistd.h>
#include <limits.h>
#include <stdlib.h>
#include <assert.h>
#include <dirent.h>
#include <string.h>
#include <errno.h>
#include <stdio.h>
#include <libgen.h>
#include "shr_ring.h"

#define CREAT_MODE 0644
#define MIN_RING_SZ (sizeof(shr_ctrl) + 1)
#define SHR_PATH_MAX 128

static char magic[] = "aredringsh";

/* this struct is mapped to the beginning of the mmap'd file. 
 */
typedef struct {
  char magic[sizeof(magic)];
  int version;
  int gflags;
  char bell[SHR_PATH_MAX]; /* fifo path */
  size_t n; /* allocd size */
  size_t u; /* used space */
  size_t i; /* input pos */
  size_t o; /* output pos */
  char d[]; /* C99 flexible array member */
} shr_ctrl;

/* the handle is given to each shr_open caller */
struct shr {
  char name[SHR_PATH_MAX]; /* ring file */
  struct stat s;
  int fd;
  int fifo_fd;
  int flags;
  union {
    char *buf;   /* mmap'd area */
    shr_ctrl *r;
  };
};

#define BLOCKING(m) (((m) & SHR_NONBLOCK) == 0)

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

/* TODO confirm behavior on releasing lock multiply and obtaining multiply */
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
 */
int make_bell(char *file, shr_ctrl *r) {
  int rc = -1;
  char dir[PATH_MAX], base[PATH_MAX], bell[PATH_MAX];
  size_t l;

  if (dirbasename(file, dir,  sizeof(dir),  1) < 0) goto done;
  if (dirbasename(file, base, sizeof(base), 0) < 0) goto done;
  snprintf(bell, sizeof(bell), "%s/.%s.fifo", dir, base);
  l = strlen(bell) + 1;

  if (l > sizeof(r->bell)) {
    fprintf(stderr, "file name too long: %s\n", bell);
    goto done;
  }

  memcpy(r->bell, bell, l);

  if (unlink(r->bell) < 0) {
    if (errno != ENOENT) {
      fprintf(stderr, "unlink %s: %s\n", r->bell, strerror(errno));
      goto done;
    }
  }

  if (mkfifo(r->bell, CREAT_MODE) < 0) {
    fprintf(stderr, "mkfifo: %s\n", bell);
    goto done;
  }

  rc = 0;

 done:
  return rc;
}

/*
 * shr_init creates a ring file
 *
 * succeeds only if the file is created new.  Attempts to resize an existing
 * file or init an existing file, even of the same size, fail.
 * TODO flags for resize, ok-if-exists, ..
 *
 */
int shr_init(char *file, size_t sz, int flags, ...) {
  char *buf = NULL;
  int rc = -1;

  size_t file_sz = sizeof(shr_ctrl) + sz;

  if (file_sz < MIN_RING_SZ) {
    fprintf(stderr,"shr_init: too small; min size: %ld\n", (long)MIN_RING_SZ);
    goto done;
  }

  assert(flags == 0);

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

  shr_ctrl *r = (shr_ctrl *)buf; 
  memcpy(r->magic, magic, sizeof(magic));
  r->version = 1;
  r->u = r->i = r->o = 0;
  r->n = file_sz - sizeof(*r);

  if (make_bell(file, r) < 0) goto done;

  rc = 0;

 done:
  if ((rc < 0) && (fd != -1)) unlink(file);
  if (fd != -1) close(fd);
  if (buf && (buf != MAP_FAILED)) munmap(buf, file_sz);
  return rc;
}


static int validate_ring(struct shr *s) {
  int rc = -1;

  if (s->s.st_size < (off_t)MIN_RING_SZ) goto done;
  if (memcmp(s->r->magic,magic,sizeof(magic))) goto done;

  shr_ctrl *r = s->r;
  size_t sz = s->s.st_size - sizeof(shr_ctrl);

  if (sz != r->n) goto done;   /* file_sz - overhead != data size */
  if (r->u >  r->n) goto done; /* used > size */
  if (r->i >= r->n) goto done; /* input position >= size */
  if (r->o >= r->n) goto done; /* output position >= size */

  /* check bell exists and is fifo */
  struct stat f;
  if (stat(r->bell, &f) < 0) goto done;
  if (S_ISFIFO(f.st_mode) == 0) goto done;

  rc = 0;

 done:
  if (rc < 0) fprintf(stderr,"invalid ring: %s\n", s->name);
  return rc;
}

struct shr *shr_open(char *file, int flags) {
  struct shr *s = NULL;
  int rc = -1;

  if ((flags & (SHR_RDONLY | SHR_WRONLY)) == 0) {
    fprintf(stderr,"shr_open: invalid mode\n");
    goto done;
  }

  s = malloc( sizeof(struct shr) );
  if (s == NULL) oom_exit();
  memset(s, 0, sizeof(*s));
  s->fd = -1;
  s->fifo_fd = -1;

  size_t l = strlen(file) + 1;
  if (l > sizeof(s->name)) {
    fprintf(stderr,"shr_open: path too long: %s\n", file);
    goto done;
  }
  memcpy(s->name, strdup(file), l);

  s->fd = open(file, O_RDWR);
  if (s->fd == -1) {
    fprintf(stderr,"open %s: %s\n", file, strerror(errno));
    goto done;
  }

  if (fstat(s->fd, &s->s) == -1) {
    fprintf(stderr,"stat %s: %s\n", file, strerror(errno));
    goto done;
  }

  s->buf = mmap(0, s->s.st_size, PROT_READ|PROT_WRITE, MAP_SHARED, s->fd, 0);
  if (s->buf == MAP_FAILED) {
    fprintf(stderr, "mmap %s: %s\n", file, strerror(errno));
    goto done;
  }

  if (validate_ring(s) < 0) goto done;

  int mode = 0;
  mode |= (flags & SHR_RDONLY) ? O_RDONLY : 0;
  mode |= (flags & SHR_WRONLY) ? O_WRONLY : 0;
  s->fifo_fd = open(s->r->bell, mode);
  if (s->fifo_fd < 0) {
    fprintf(stderr, "open %s: %s\n", s->r->bell, strerror(errno));
    goto done;
  }
  
  rc = 0;

 done:
  if ((rc < 0) && s) {
    if (s->fd != -1) close(s->fd);
    if (s->fifo_fd != -1) close(s->fifo_fd);
    if (s->buf && (s->buf != MAP_FAILED)) munmap(s->buf, s->s.st_size);
    free(s);
    s = NULL;
  }
  return s;
}

static int ring_bell(shr_ctrl *r) {
  int rc = -1;

  rc = 0;

 done:
  return rc;
}

/* copy data in. fails if ringbuf has insuff space. */
#define MIN(a,b) (((a) < (b)) ? (a) : (b))
ssize_t shr_write(struct shr *s, char *buf, size_t len) {
  int rc = -1;
  shr_ctrl *r = s->r;

  /* since this function returns signed, cap len */
  if (len > SSIZE_MAX) goto done;

  if (lock(s->fd) < 0) goto done;
  
  size_t a,b,c;
  if (r->i < r->o) {  // available space is a contiguous buffer
    a = r->o - r->i; 
    assert(a == r->n - r->u);
    if (len > a) goto done;
    memcpy(&r->d[r->i], buf, len);
  } else {            // available space wraps; it's two buffers
    b = r->n - r->i;  // in-head to eob (receives leading input)
    c = r->o;         // out-head to in-head (receives trailing input)
    a = b + c;        // available space
    // the only ambiguous case is i==o, that's why u is needed
    if (r->i == r->o) a = r->n - r->u; 
    assert(a == r->n - r->u);
    if (len > a) goto done;
    memcpy(&r->d[r->i], buf, MIN(b, len));
    if (len > b) memcpy(r->d, &buf[b], len-b);
  }
  r->i = (r->i + len) % r->n;
  r->u += len;

  ring_bell(r);
  rc = 0;

 done:

  unlock(s->fd);
  return (rc == 0) ? (ssize_t)len : -1;
}

/*
 * shr_read
 *
 * If blocking, wait until data is available, read it, return 0.
 * If non-blocking, if data available, handle as above, else return 1.
 * On error, return -1.
 * On signal, return -2.
 * TODO implications for bell; multi reads reqd? (fill caller buf from both chunks; req caller to recall if buf filled)
 */
ssize_t shr_read(struct shr *s, char *buf, size_t len) {
  int rc = -1;
  shr_ctrl *r = s->r;
  size_t nr;
  char *from;

  /* since this function returns signed, cap len */
  if (len > SSIZE_MAX) len = SSIZE_MAX;
  if (len == 0) goto done;

  if (lock(s->fd) < 0) goto done;

  if (r->o < r->i) { // next chunk is the whole pending buffer
    assert(r->u == r->i - r->o);
    from = &r->d[r->o];
    nr = r->u;
    if (len < nr) nr = len;
    memcpy(buf, from, nr);
    /* mark consumed */
    r->o = (r->o + nr ) % r->n;
    r->u -= nr;
  } else if ((r->o == r->i) && (r->u == 0)) {
    nr = 0;
  } else {
    // if we're here, that means r->o > r->i. the pending
    // output is wrapped around the buffer. this function 
    // returns the chunk prior to eob. caller's has to call
    // again to get the next chunk wrapped around the buffer.
    size_t b,c;
    b = r->n - r->o; // length of the part we're returning
    c = r->i;        // wrapped part length- a sanity check
    assert(r->u == b + c);
    from = &r->d[r->o];
    nr = b;
    if (len < nr) nr = len;
    memcpy(buf, from, nr);
    /* mark consumed */
    r->o = (r->o + nr ) % r->n;
    r->u -= nr;
  }

  if ((nr == 0) && BLOCKING(s->flags)) {
    /* TODO block */
    unlock(s->fd);
    /* TODO read bell */
  }

  rc = 0;

 done:

  unlock(s->fd);
  return (rc == 0) ? (ssize_t)nr : -1;
}

void shr_close(struct shr *s) {
  if (s->fd != -1) close(s->fd);
  if (s->fifo_fd != -1) close(s->fifo_fd);
  if (s->buf && (s->buf != MAP_FAILED)) munmap(s->buf, s->s.st_size);
  free(s);
}


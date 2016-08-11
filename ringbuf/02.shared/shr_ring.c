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
#define MIN(a,b) (((a) < (b)) ? (a) : (b))

/* gflags */
#define W_WANT_SPACE (1U << 0)
#define R_WANT_DATA  (1U << 1)

static char magic[] = "aredringsh";

/* this struct is mapped to the beginning of the mmap'd file. 
 */
typedef struct {
  char magic[sizeof(magic)];
  int version;
  unsigned gflags;
  char w2r[SHR_PATH_MAX]; /* w->r fifo */
  char r2w[SHR_PATH_MAX]; /* r->w fifo */
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
  int ring_fd;
  int r2w;
  int w2r;
  unsigned flags;
  union {
    char *buf;   /* mmap'd area */
    shr_ctrl *r;
  };
};

static void oom_exit(void) {
  fprintf(stderr, "out of memory\n");
  abort();
}

/* get the lock on the ring file. we use a file lock for any read or write, 
 * because even the reader adjusts the position in the ring buffer. note,
 * we use a blocking wait (F_SETLKW) for the lock. this should be obtainable
 * quickly because a locked reader should read and release in bounded time.
 * if a signal comes in while we await the lock, fcntl can return EINTR. since
 * we are a library, we do not alter the application's signal handling.
 * rather, we propagate the condition up to the application to deal with. 
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
  char dir[PATH_MAX], base[PATH_MAX], w2r[PATH_MAX], r2w[PATH_MAX];
  size_t l;

  if (dirbasename(file, dir,  sizeof(dir),  1) < 0) goto done;
  if (dirbasename(file, base, sizeof(base), 0) < 0) goto done;
  snprintf(w2r, sizeof(w2r), "%s/.%s.w2r", dir, base);
  snprintf(r2w, sizeof(r2w), "%s/.%s.r2w", dir, base);
  l = strlen(w2r) + 1;
  assert(strlen(w2r) == strlen(r2w));

  if (l > sizeof(r->w2r)) {
    fprintf(stderr, "file name too long: %s\n", w2r);
    goto done;
  }

  memcpy(r->w2r, w2r, l);
  memcpy(r->r2w, r2w, l);

  if (unlink(r->w2r) < 0) {
    if (errno != ENOENT) {
      fprintf(stderr, "unlink %s: %s\n", r->w2r, strerror(errno));
      goto done;
    }
  }

  if (unlink(r->r2w) < 0) {
    if (errno != ENOENT) {
      fprintf(stderr, "unlink %s: %s\n", r->r2w, strerror(errno));
      goto done;
    }
  }

  if (mkfifo(r->w2r, CREAT_MODE) < 0) {
    fprintf(stderr, "mkfifo: %s\n", w2r);
    goto done;
  }

  if (mkfifo(r->r2w, CREAT_MODE) < 0) {
    fprintf(stderr, "mkfifo: %s\n", r2w);
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

/* this can be used as a precursor to shr_close to delete the ring and fifos */
int shr_unlink(struct shr *s) {
  unlink(s->name);
  unlink(s->r->w2r);
  unlink(s->r->r2w);
  return 0;
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
  if (stat(r->w2r, &f) < 0) goto done;
  if (S_ISFIFO(f.st_mode) == 0) goto done;
  if (stat(r->r2w, &f) < 0) goto done;
  if (S_ISFIFO(f.st_mode) == 0) goto done;

  rc = 0;

 done:
  if (rc < 0) fprintf(stderr,"invalid ring: %s\n", s->name);
  return rc;
}

static int make_nonblock(int fd) {
	int fl, unused = 0, rc = -1;

	fl = fcntl(fd, F_GETFL, unused);
	if (fl < 0) {
		fprintf(stderr, "fcntl: %s\n", strerror(errno));
		goto done;
	}

	if (fcntl(fd, F_SETFL, fl | O_NONBLOCK) < 0) {
		fprintf(stderr, "fcntl: %s\n", strerror(errno));
		goto done;
	}

  rc = 0;

 done:
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
  s->ring_fd = -1;
  s->w2r = -1;
  s->r2w = -1;
  s->flags = flags;

  size_t l = strlen(file) + 1;
  if (l > sizeof(s->name)) {
    fprintf(stderr,"shr_open: path too long: %s\n", file);
    goto done;
  }

  memcpy(s->name, file, l);
  s->ring_fd = open(file, O_RDWR);
  if (s->ring_fd == -1) {
    fprintf(stderr,"open %s: %s\n", file, strerror(errno));
    goto done;
  }

  if (fstat(s->ring_fd, &s->s) == -1) {
    fprintf(stderr,"stat %s: %s\n", file, strerror(errno));
    goto done;
  }

  s->buf = mmap(0, s->s.st_size, PROT_READ|PROT_WRITE, MAP_SHARED, s->ring_fd, 0);
  if (s->buf == MAP_FAILED) {
    fprintf(stderr, "mmap %s: %s\n", file, strerror(errno));
    goto done;
  }

  if (validate_ring(s) < 0) goto done;

  /* the bell is a pair of fifo's. open'd in O_RDWR. this is ok on Linux,
   * see fifo(7). we use O_RDWR because the peer (reader, or writer) may or 
   * may not exist at the same time as us. the bell notifies the peer if it
   * exists, and is a no-op if the peer is absent.  
   */
  s->w2r = open(s->r->w2r, O_RDWR); 
  if (s->w2r < 0) {
    fprintf(stderr, "open %s: %s\n", s->r->w2r, strerror(errno));
    goto done;
  }

  s->r2w = open(s->r->r2w, O_RDWR);
  if (s->r2w < 0) {
    fprintf(stderr, "open %s: %s\n", s->r->r2w, strerror(errno));
    goto done;
  }

  /*
   *           NB ------r2w-----> B
   *   reader                         writer
   *            B <-----w2r------ NB
   *
   */
  if (flags & SHR_RDONLY) {  /* this process is a reader */
     if (make_nonblock(s->r2w) < 0) goto done;  /* w may be absent, nonblock */
     /* s->w2r is blocking */
  }

  if (flags & SHR_WRONLY) {  /* this process is a writer */
     if (make_nonblock(s->w2r) < 0) goto done;  /* r may be absent, nonblock */
     /* s->r2w is blocking */
  }

  rc = 0;

 done:
  if ((rc < 0) && s) {
    if (s->ring_fd != -1) close(s->ring_fd);
    if (s->w2r != -1) close(s->w2r);
    if (s->r2w != -1) close(s->r2w);
    if (s->buf && (s->buf != MAP_FAILED)) munmap(s->buf, s->s.st_size);
    free(s);
    s = NULL;
  }
  return s;
}

/* 
 * to ring the bell we write a byte (non-blocking) to a fifo. this notifies
 * the peer if it exists, and is harmless otherwise. also if the fifo is full
 * we consider this to have succeeded, because the point of the fifo is to be
 * readable when data is available; and a full fifo already satisfies that need.
 *
 * we only ring the bell when the peer R_WANTS_DATA or W_WANTS_SPACE. though,
 * it is harmless to ring it superfluously, as the peer reblocks as needed.
*/
static int ring_bell(struct shr *s) {
  int rc = -1;
  ssize_t nr;
  char b = 0;

  int fd = -1;
  if ((s->flags & SHR_WRONLY) && (s->r->gflags & R_WANT_DATA))  fd = s->w2r;
  if ((s->flags & SHR_RDONLY) && (s->r->gflags & W_WANT_SPACE)) fd = s->r2w;

	if (fd == -1) { /* no peer awaits bell right now */
    rc = 0;
    goto done;
	}

  nr = write(fd, &b, sizeof(b));
  if ((nr < 0) && !((errno == EWOULDBLOCK) || (errno == EAGAIN))) {
    fprintf(stderr, "write: %s\n", strerror(errno));
    goto done;
  }

  rc = 0;

 done:
  return rc;
}

/* read the fifo, causing us to block, awaiting notification from the peer.
 * for a reader process, the notification (fifo readability) means that we
 * should check the ring for new data. for a writer process, the notification
 * means that space has become available in the ring. both of these only
 * occur if we have told the peer to notify us by setting s->r->gflags
 *
 * returns 0 on normal wakeup, 
 *        -1 on error, 
 *        -2 on signal while blocked
 */
static int block(struct shr *s, int condition) {
  int rc = -1, fd;
  ssize_t nr;
  char b[4096]; /* any big buffer to reduce reads */

  if      (condition == R_WANT_DATA)  fd = s->w2r;
  else if (condition == W_WANT_SPACE) fd = s->r2w;
  else assert(0);

  nr = read(fd, &b, sizeof(b));
  if (nr < 0) {
    if (errno == EINTR) rc = -2;
    else fprintf(stderr, "read: %s\n", strerror(errno));
    goto done;
  }

  assert(nr > 0);

  rc = 0;

 done:
  return rc;
}

/* 
 * write data into ring
 *
 * if there is sufficient space in the ring - copy the whole buffer in.
 * if there is insufficient free space in the ring- wait for space, or
 * return 0 immediately in non-blocking mode. only writes all or nothing.
 *
 * returns:
 *   > 0 (number of bytes copied into ring, always the full buffer)
 *   0   (insufficient space in ring, in non-blocking mode)
 *  -1   (error, such as the buffer exceeds the total ring capacity)
 *  -2   (signal arrived while blocked waiting for ring)
 *
 */
ssize_t shr_write(struct shr *s, char *buf, size_t len) {
  int rc = -1;
  shr_ctrl *r = s->r;

  /* since this function returns signed, cap len */
  if (len > SSIZE_MAX) goto done;

 again:
  rc = -1;
  if (lock(s->ring_fd) < 0) goto done;
  
  size_t a,b,c;
  if (r->i < r->o) {  // available space is a contiguous buffer
    a = r->o - r->i; 
    assert(a == r->n - r->u);
    if (len > a) { /* insufficient space in ring to write len bytes */
      if (s->flags & SHR_NONBLOCK) { rc = 0; len = 0; goto done; }
      goto block;
    }
    memcpy(&r->d[r->i], buf, len);
  } else {            // available space wraps; it's two buffers
    b = r->n - r->i;  // in-head to eob (receives leading input)
    c = r->o;         // out-head to in-head (receives trailing input)
    a = b + c;        // available space
    // the only ambiguous case is i==o, that's why u is needed
    if (r->i == r->o) a = r->n - r->u; 
    assert(a == r->n - r->u);
    if (len > a) { /* insufficient space in ring to write len bytes */
      if (s->flags & SHR_NONBLOCK) { rc = 0; len = 0; goto done; }
      goto block;
    }
    memcpy(&r->d[r->i], buf, MIN(b, len));
    if (len > b) memcpy(r->d, &buf[b], len-b);
  }
  r->i = (r->i + len) % r->n;
  r->u += len;
  s->r->gflags &= ~(W_WANT_SPACE);

  ring_bell(s);
  rc = 0;

 done:

  unlock(s->ring_fd);
  return (rc == 0) ? (ssize_t)len : -1;

 block:
  s->r->gflags |= W_WANT_SPACE;
  unlock(s->ring_fd);
  block(s,W_WANT_SPACE);
  goto again;
}

/* TODO get selectable fd for multi-fd library users.
 * note that the behavior must allow for a non-readable fd 
 * even when data is available, because we may have drained the fifo
 * without consuming the ring entirely. so,
 *
 * the behavior of the reader must be 
 *  check the ring
 *  block for data if none
 */

/*
 * shr_read
 *
 * If blocking, wait until data is available, return num bytes read.
 * If non-blocking, handle as above if data available, else return 0.
 * On error, return -1.
 * On signal while blocked, return -2.
 */
ssize_t shr_read(struct shr *s, char *buf, size_t len) {
  int rc = -1;
  shr_ctrl *r = s->r;
  size_t nr;
  char *from;

  /* since this function returns signed, cap len */
  if (len > SSIZE_MAX) len = SSIZE_MAX;
  if (len == 0) goto done;

 again:
  rc = -1;
  if (lock(s->ring_fd) < 0) goto done;

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

  /* data consumed from the ring. clear any data-wanted notification request
   * in gflags. ring bell to notify writer if awaiting free space.  */
  if (nr > 0) {
    s->r->gflags &= ~(R_WANT_DATA);
    ring_bell(s);
  }

  /* no data? block if in blocking mode */
  if ((nr == 0) && ((s->flags & SHR_NONBLOCK) == 0)) {
    s->r->gflags |= R_WANT_DATA;
    unlock(s->ring_fd);
    rc = block(s, R_WANT_DATA);
    if (rc < 0) goto done;
    goto again;
  }

  rc = 0;

 done:

  unlock(s->ring_fd);
  return (rc == 0) ? (ssize_t)nr : rc;
}

void shr_close(struct shr *s) {
  if (s->ring_fd != -1) close(s->ring_fd);
  if (s->w2r != -1) close(s->w2r);
  if (s->r2w != -1) close(s->r2w);
  if (s->buf && (s->buf != MAP_FAILED)) munmap(s->buf, s->s.st_size);
  free(s);
}

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

/* TODO
 *
 * prefault/mlockall
 * assert gflags and open flags unmixed in open/init
 *
 *
 */

#define CREAT_MODE 0644
#define MIN_RING_SZ (sizeof(shr_ctrl) + 1)
#define SHR_PATH_MAX 128
#define MIN(a,b) (((a) < (b)) ? (a) : (b))

/* gflags */
#define W_WANT_SPACE (1U << 0)
#define R_WANT_DATA  (1U << 1)

static char magic[] = "msgring";
/* this struct is mapped to the beginning of the mmap'd file. 
 * the beginning fields are created at the time of shr_init
 * (in other words, before the lifecycle of shr_open/rw/close).
 * so those fields are fixed for the life of the ring. the flags
 * and internal offsets constantly change, under posix file lock,
 * as data enters or is copied from, the ring. they are volatile
 * because another process updates them in shared memory. 
 */
typedef struct {
  char magic[sizeof(magic)];
  int version;
  char w2r[SHR_PATH_MAX]; /* w->r fifo */
  char r2w[SHR_PATH_MAX]; /* r->w fifo */
  unsigned volatile gflags;
  struct shr_stat stat; /* i/o stats */
  size_t volatile n; /* allocd size */
  size_t volatile u; /* used space */
  size_t volatile i; /* input pos (next write starts here) */
  size_t volatile o; /* output pos (next read starts here) */
  size_t volatile m; /* message count (SHR_MESSAGES mode) */
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
    shr_ctrl * r;
  };
};


static void oom_exit(void) {
  fprintf(stderr, "out of memory\n");
  abort();
}

/* get the lock on the ring file. we use a file lock for any read or write, 
 * since even the reader adjusts the position offsets in the ring buffer. note,
 * we use a blocking wait (F_SETLKW) for the lock. this should be obtainable in
 * quasi bounded time because a peer (reader or writer using this same library)
 * should also only lock/manipulate/release in rapid succession.
 *
 * if a signal comes in while we await the lock, fcntl can return EINTR. since
 * we are a library, we do not alter the application's signal handling.
 * rather, we propagate the condition up to the application to deal with. 
 *
 * also note, since this is a POSIX file lock, anything that closes the 
 * descriptor (such as killing the application holding the lock) releases it.
 *
 * fcntl based locks can be multiply acquired (without reference counting),
 * meaning it is a no-op to relock an already locked file. it is also ok
 * to unlock an already-unlocked file. See Kerrisk, TLPI p1128 "It is not an
 * error to unlock a region on which we don't currently hold a lock". This 
 * simplifies this library because we can unlock without keeping the status
 * of the previous lock (in other words, a failed lock can goto an unlock/return
 * clause and that's ok).
 *
 * lastly, on Linux you can see the active locks in /proc/locks
 *
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

static void hexdump(char *buf, size_t len) {
  size_t i,n=0;
  char c;
  while(n < len) {
    fprintf(stderr,"%08x ", (int)n);
    for(i=0; i < 16; i++) {
      c = (n+i < len) ? buf[n+i] : 0;
      if (n+i < len) fprintf(stderr,"%.2x ", c);
      else fprintf(stderr, "   ");
    }
    for(i=0; i < 16; i++) {
      c = (n+i < len) ? buf[n+i] : ' ';
      if (c < 0x20 || c > 0x7e) c = '.';
      fprintf(stderr,"%c",c);
    }
    fprintf(stderr,"\n");
    n += 16;
  }
}

__attribute__ ((__unused__)) static void debug_ring(struct shr *s) {
  fprintf(stderr,"ring size %ld\n", s->r->n);
  fprintf(stderr,"ring used %ld\n", s->r->u);
  fprintf(stderr,"ring rpos %ld\n", s->r->o);
  fprintf(stderr,"ring wpos %ld\n", s->r->i);
  hexdump(s->r->d, s->r->n);
  fprintf(stderr,"\n");
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
 *
 * flags:
 *    SHR_OVERWRITE - permits ring to exist already, overwrites it
 *    SHR_KEEPEXIST - permits ring to exist already, leaves size/content
 *
 * returns 0 on success
 *        -1 on error
 *        -2 on already-exists error (keepexist/overwrite not requested)
 * 
 *
 *
 */
int shr_init(char *file, size_t sz, int flags, ...) {
  int rc = -1, fd = -1;
  char *buf = NULL;

  size_t file_sz = sizeof(shr_ctrl) + sz;

  if ((flags & SHR_OVERWRITE) && (flags & SHR_KEEPEXIST)) {
    fprintf(stderr,"shr_init: incompatible flags\n");
    goto done;
  }

  if (file_sz < MIN_RING_SZ) {
    fprintf(stderr,"shr_init: too small; min size: %ld\n", (long)MIN_RING_SZ);
    goto done;
  }

  /* if ring exists already, flags determine behavior */
  struct stat st;
  if (stat(file, &st) == 0) { /* exists */
    if (flags & SHR_OVERWRITE) {
      if (unlink(file) < 0) {
        fprintf(stderr, "unlink %s: %s\n", file, strerror(errno));
        goto done;
      }
    } else if (flags & SHR_KEEPEXIST) {
      rc = 0;
      goto done;
    } else {
      fprintf(stderr,"shr_init: %s already exists\n", file);
      rc = -2;
      goto done;
    }
  }

  fd = open(file, O_RDWR|O_CREAT|O_EXCL, CREAT_MODE);
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
  r->u = 0;
  r->i = 0;
  r->o = 0;
  r->m = 0;
  r->n = file_sz - sizeof(*r);

  r->gflags = 0;
  if (flags & SHR_MESSAGES)  r->gflags |= SHR_MESSAGES;
  if (flags & SHR_LRU_STOMP) r->gflags |= SHR_LRU_STOMP;

  if (make_bell(file, r) < 0) goto done;

  rc = 0;

 done:
  if ((rc == -1) && (fd != -1)) unlink(file);
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


/* 
 * shr_stat
 *
 * retrieve statistics about the ring. if reset is non-NULL, the 
 * struct timeval it points to is written into the internal stats
 * structure as the start time of the new stats period, and the 
 * counters are reset as a side effect
 *
 * returns 0 on success (and fills in *stat), -1 on failure
 *
 */
int shr_stat(shr *s, struct shr_stat *stat, struct timeval *reset) {
  int rc = -1;

  if (lock(s->ring_fd) < 0) goto done;
  *stat = s->r->stat; /* struct copy */
  stat->bn = s->r->n;
  stat->bu = s->r->u;
  stat->mu = s->r->m;

  if (reset) {
    memset(&s->r->stat, 0, sizeof(s->r->stat));
    s->r->stat.start = *reset; /* struct copy */
  }

  rc = 0;

 done:
  unlock(s->ring_fd);
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

static int setup_selectable_r(shr *s) {
  int rc = -1, fl, unused = 0;
  char b = 0;

  assert(s->flags & SHR_RDONLY);
  assert(s->flags & SHR_NONBLOCK);
  assert(s->flags & SHR_SELECTFD);

  /* leave the want flag on permanently */
  s->r->gflags |= R_WANT_DATA;

  /* if ring is empty we are done */
  if (s->r->u == 0) {
    rc = 0;
    goto done;
  }

  /* set initial readability since prior data in ring */
  fl = fcntl(s->w2r, F_GETFL, unused);
  if (fl < 0) {
    fprintf(stderr, "fcntl: %s\n", strerror(errno));
    goto done;
  }

  if (fcntl(s->w2r, F_SETFL, fl | O_NONBLOCK) < 0) {
    fprintf(stderr, "fcntl: %s\n", strerror(errno));
    goto done;
  }

  if (write(s->w2r, &b, sizeof(b)) < 0) {
    if ((errno != EWOULDBLOCK) && (errno != EAGAIN)) {
      fprintf(stderr, "write: %s\n", strerror(errno));
      goto done;
    }
  }

  if (fcntl(s->w2r, F_SETFL, fl) < 0) {
    fprintf(stderr, "fcntl: %s\n", strerror(errno));
    goto done;
  }

  rc = 0;

 done:
  return rc;
}

/*
 * shr_get_selectable_fd
 *
 * this call succeeds only if the ring was opened with SHR_RDONLY|SHR_SELECTFD.
 * it returns a file descriptor that can be used with select/poll/epoll to tell
 * when there is data to read. (at which point the application should shr_read).
 *
 */
/* TODO decide whether a w version of this is needed */
int shr_get_selectable_fd(shr *s) {
  int rc = -1;

  if ((s->flags & SHR_SELECTFD) == 0) goto done;
  if ((s->flags & SHR_RDONLY) == 0) goto done;

  rc = 0;

 done:
  return (rc == 0) ? s->w2r : -1;
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
     if (flags & SHR_SELECTFD) {
       if (setup_selectable_r(s) < 0) goto done;
       if (make_nonblock(s->w2r) < 0) goto done;  /* w2r used to epoll */
     }
  }

  if (flags & SHR_WRONLY) {  /* this process is a writer */
     if (flags & SHR_SELECTFD) goto done;       /* select fd mode for r only */
     if (make_nonblock(s->w2r) < 0) goto done;  /* r may be absent, nonblock */
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

/* helper function; given ring offset o (pointing to a message length prefix)
 * read it, taking into account the possibility that the prefix wraps around */
static size_t get_msg_len(struct shr *s, size_t o) {
  size_t msg_len, hdr = sizeof(size_t);
  assert(o < s->r->n);
  assert(s->r->u >= hdr);
  size_t b = s->r->n - o; /* bytes at o til wrap */
  memcpy(&msg_len, &s->r->d[o], MIN(b, hdr));
  if (b < hdr) memcpy( ((char*)&msg_len) + b, s->r->d, hdr-b);
  return msg_len;
}

/* 
 * this function is called under lock to forcibly reclaim space from the ring,
 * (SHR_LRU_STOMP mode). The oldest portion of ring data is sacrificed.
 *
 * if this is a ring of messages (SHR_MESSAGES), preserve boundaries by 
 * moving the read position to the nearest message at or after delta bytes.
 */
static void reclaim(struct shr *s, size_t delta) {
  size_t o, reclaimed=0, msg_len;

  if (s->r->gflags & SHR_MESSAGES) {
    for(o = s->r->o; reclaimed < delta; reclaimed += msg_len) {
      msg_len = get_msg_len(s,o) + sizeof(size_t);
      o = (o + msg_len ) % s->r->n;
      s->r->stat.md++; /* msg drops */
      s->r->m--;       /* msgs in ring */
    }
    delta = reclaimed;
  }

  s->r->o = (s->r->o + delta) % s->r->n;
  s->r->u -= delta;
  s->r->stat.bd += delta; /* bytes dropped */
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
  size_t hdr = (s->r->gflags & SHR_MESSAGES) ? sizeof(len) : 0;

  /* since this function returns signed, cap len */
  if (len > SSIZE_MAX) goto done;
  /* does the buffer exceed total ring capacity */
  if (len + hdr > s->r->n) goto done;
  /* zero length writes/messages are an error */
  if (len == 0) goto done;

 again:
  rc = -1;
  if (lock(s->ring_fd) < 0) goto done;
  
  size_t a,b,c;
  if (r->i < r->o) {  
    // available space is a contiguous buffer (and there is pending data) */
    a = r->o - r->i; 
    assert(a == r->n - r->u);
    if (len + hdr > a) { /* insufficient space in ring to write len bytes */
      if (s->r->gflags & SHR_LRU_STOMP) { reclaim(s, len+hdr - a); goto again; }
      if (s->flags & SHR_NONBLOCK) { rc = 0; len = 0; goto done; }
      goto block;
    }
    if (hdr) memcpy(&r->d[r->i], &len, hdr);
    memcpy(&r->d[r->i] + hdr, buf, len);
    r->i = (r->i + len + hdr) % r->n;
    r->u += (len + hdr);
  } else {            
    // available space wraps; it's two buffers (or its empty or full)
    b = r->n - r->i;  // in-head to eob (receives leading input)
    c = r->o;         // out-head to in-head (receives trailing input)
    a = b + c;        // available space
    // the only ambiguous case is i==o, that's why u is needed
    if (r->i == r->o) a = r->n - r->u; 
    assert(a == r->n - r->u);
    if (len + hdr > a) { /* insufficient space in ring to write len bytes */
      if (s->flags & SHR_NONBLOCK) { rc = 0; len = 0; goto done; }
      if (s->r->gflags & SHR_LRU_STOMP) { reclaim(s, len+hdr - a); goto again; }
      goto block;
    }
    if (hdr) {
      char *l = (char*)&len;
      memcpy(&r->d[r->i], l, MIN(b, hdr));
      if (hdr > b) memcpy(r->d, l + b, hdr-b);
      r->i = (r->i + hdr) % r->n;
      r->u += hdr;
      b = r->n - r->i;
      if (b > 0) memcpy(&r->d[r->i], buf, MIN(b, len));
      if (len > b) memcpy(r->d, &buf[b], len-b);
      r->i = (r->i + len) % r->n;
      r->u += len;
    } else {
      memcpy(&r->d[r->i], buf, MIN(b, len));
      if (len > b) memcpy(r->d, &buf[b], len-b);
      r->i = (r->i + len) % r->n;
      r->u += len;
    }
  }
  s->r->gflags &= ~(W_WANT_SPACE);

  ring_bell(s);
  rc = 0;

 done:

  //debug_ring(s);
  s->r->stat.bw += (rc == 0) ? (len + hdr) : 0;
  s->r->stat.mw += ((rc == 0) && hdr) ? 1 : 0;
  s->r->m += ((rc == 0) && hdr) ? 1 : 0;
  unlock(s->ring_fd);
  return (rc == 0) ? (ssize_t)len : -1;

 block:
  s->r->gflags |= W_WANT_SPACE;
  unlock(s->ring_fd);
  block(s,W_WANT_SPACE);
  goto again;
}

/*
 * shr_read
 *
 * Read from the ring. Block if there is no data in the ring, or return
 * immediately in non-blocking mode. As with traditional unix read(2)- multiple
 * shr_reads may be required to consume the data available in the ring. 
 * In SHR_MESSAGES mode, each read returns exactly one message.
 *
 * returns:
 *   > 0 (number of bytes read from the ring)
 *   0   (ring empty, in non-blocking mode)
 *  -1   (error)
 *  -2   (signal arrived while blocked waiting for ring)
 *  -3   (buffer can't hold message; SHR_MESSAGE mode)
 *   
 */
ssize_t shr_read(struct shr *s, char *buf, size_t len) {
  int rc = -1;
  shr_ctrl *r = s->r;
  size_t nr, wr;
  char *from, b;
  size_t hdr = (s->r->gflags & SHR_MESSAGES) ? sizeof(len) : 0;
  size_t msg_len;

  /* since this function returns signed, cap len */
  if (len > SSIZE_MAX) len = SSIZE_MAX;
  if (len == 0) goto done;

 again:
  rc = -1;
  if (lock(s->ring_fd) < 0) goto done;
  //debug_ring(s);

  if (r->o < r->i) { // readable extent is contiguous 
    assert(r->u == r->i - r->o);
    from = &r->d[r->o];
    nr = r->u;
    if (hdr) {
      assert(nr >= hdr);
      memcpy(&msg_len, from, sizeof(msg_len));
      assert(nr >= hdr + msg_len);
      if (len < msg_len) { rc = -3; goto done; }
      memcpy(buf, from + hdr, msg_len);
      r->o = (r->o + hdr + msg_len) % r->n;
      r->u -= (hdr + msg_len);
      nr = msg_len;
    } else {
      if (len < nr) nr = len;
      memcpy(buf, from, nr);
      r->o = (r->o + nr ) % r->n;
      r->u -= nr;
    }
  } else if ((r->o == r->i) && (r->u == 0)) {
    nr = 0; /* nothing to read */
  } else {
    // the readable extent wraps around the buffer. 
    size_t b,c;
    b = r->n - r->o; // length of the part before eob
    c = r->i;        // length of the part after wrap
    assert(r->u == b + c);
    from = &r->d[r->o];
    nr = r->u;

    /* start by reading the message length word, if in message mode. 
     * the message length header is a size_t prepended to the message. 
     * in message mode hdr == sizeof(size_t) but in byte mode hdr == 0. */
    if (hdr) {
      assert(nr >= hdr);
      char *to = (char*)&msg_len;    // cast so char so we can do wrap math
      memcpy(to, from, MIN(b, hdr)); // copy initial bytes of message length
      if (b < hdr) memcpy( to + b, r->d, hdr-b);  // copy remaining bytes
      assert(nr >= hdr + msg_len);
      if (len < msg_len) { rc = -3; goto done; } // caller buffer insufficient
      r->o = (r->o + hdr) % r->n;    // advance read position past length word
      r->u -= hdr;
      b = r->n - r->o;               // may have wrapped so update r-to-wrap 
      nr = MIN(b, msg_len);
      from = &r->d[r->o];
      memcpy(buf, from, nr);         // copy initial bytes of message itself
      if (b < msg_len) memcpy(buf+b, r->d, msg_len-b); // copy wrapped part
      r->o = (r->o + msg_len ) % r->n; // advance read position past message
      r->u -= msg_len;
      nr = msg_len;
    } else {
      nr = b;                        // in byte mode, copy as much as caller
      if (len < nr) nr = len;        // provided space for
      memcpy(buf, from, nr);
      wr = MIN(len-nr, c);           // copy part after wrap 
      if (wr > 0) {
        memcpy(buf+nr, r->d, wr);
        nr += wr;
      }
      r->o = (r->o + nr ) % r->n;    // advance read position past consumed
      r->u -= nr;
    }
  }

  /*
   * (A) in SELECTFD mode, the w2r fifo has needs to be drained otherwise
   * the caller level-triggered epoll would forever remain readable. the w2r
   * fifo is non-blocking (to us, the reader) in this mode so drain is easy.
   * actually we don't need to drain it, its enough to drain a byte and let
   * the caller epoll retrigger this. the idea is to avoid any expectation
   * that the bytes in the fifo equate to one readable event in the ring.
   * as said in other places that is not possible because of factors such
   * as LRU_STOMP mode which discard ring content even though their fifo
   * notifications may still be there; and fifo reaching its capacity 
   * (which is fine, because its only purpose is to be readable when ring
   * data is availble). so this library is designed so there is no expectation
   * that fifo reads and ring data have a tight correspondence. it is only
   * important that the fifo is readable when there is data, and that the
   * caller drain the ring by calling shr_read in a loop til it returns 0
   * when even a single byte is in the fifo.
   *
   * (B) clear WANT_DATA in gflags, meaning, the writer need not notify us
   * of writes (normally this is only set when a reader is about to enter the
   * blocked state). in SELECTFD mode however we leave the WANT_DATA flag on
   * permanently, since caller uses its own epoll instead of our block.
   *
   * (C) ring the bell, if there's a writer awaiting space;
   *
   */
  if (s->flags & SHR_SELECTFD) {
    if (read(s->w2r, &b, sizeof(b)) < 0) {
      if ((errno != EWOULDBLOCK) && (errno != EAGAIN)) goto done;
    }
  } else if (nr > 0) {
    s->r->gflags &= ~(R_WANT_DATA);
  }

  if (nr > 0) ring_bell(s);

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

  if ((nr > 0) && (rc == 0)) {
    s->r->stat.br += (nr + hdr);
    s->r->stat.mr += (hdr ? 1 : 0);
    s->r->m -= (hdr ? 1 : 0);
  }
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

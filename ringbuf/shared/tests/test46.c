#include <stdio.h>
#include <string.h>
#include <sys/wait.h>
#include <sys/time.h>
#include <assert.h>
#include <errno.h>
#include <stdlib.h>
#include <unistd.h>
#include "shr_ring.h"

/* this test is special. it doubles as a performance test.
   this one does read and write with simultaneous contention and blocking */

/* test message is 36*10 + 1 bytes. 361 bytes just for realism */
char msg[]  =      "1234567890abcdefghijklmnopqrstuvwxyz"
                   "!@#$%^&*()ABCDEFGHIJKLMNOPQRSTUVWXYZ"
                   "1234567890abcdefghijklmnopqrstuvwxyz"
                   "!@#$%^&*()ABCDEFGHIJKLMNOPQRSTUVWXYZ"
                   "1234567890abcdefghijklmnopqrstuvwxyz"
                   "!@#$%^&*()ABCDEFGHIJKLMNOPQRSTUVWXYZ"
                   "1234567890abcdefghijklmnopqrstuvwxyz"
                   "!@#$%^&*()ABCDEFGHIJKLMNOPQRSTUVWXYZ"
                   "1234567890abcdefghijklmnopqrstuvwxyz"
                   "!@#$%^&*()ABCDEFGHIJKLMNOPQRSTUVWXYZ";

int nmsg = 1000000;
#define ring_sz ((sizeof(msg) + sizeof(size_t)) * nmsg)

char *ring = __FILE__ ".ring";

void delay() { usleep(50000); }

#define do_open   'o'
#define do_close  'c'
#define do_write  'w'
#define do_read   'r'
#define do_unlink 'u'
#define do_select 's'
#define do_get_fd 'g'
#define do_empty  'e'
#define do_fill   'f'

void print_elapsed(char *name, struct timeval *start, struct timeval *end, int nmsg) {
  unsigned long usec_start, usec_end, usec_elapsed;
  usec_end = end->tv_sec * 1000000 + end->tv_usec;
  usec_start = start->tv_sec * 1000000 + start->tv_usec;
  usec_elapsed = usec_end - usec_start;
  double msgs_per_sec = usec_elapsed  ? (nmsg * 1000000.0 / usec_elapsed) : 0;
  fprintf(stderr,"%s: %f msgs/sec\n", name, msgs_per_sec);
}

void r(int fd) {
  shr *s = NULL;
  char op, c, buf[sizeof(msg)];
  int rc, selectable_fd=-1, n;
  struct timeval tv_start, tv_end;

  printf("r: ready\n");

  for(;;) {
    rc = read(fd, &op, sizeof(op));
    if (rc < 0) {
      fprintf(stderr,"r read: %s\n", strerror(errno));
      goto done;
    }
    if (rc == 0) {
      printf("r: eof\n");
      goto done;
    }
    assert(rc == sizeof(op));
    switch(op) {
      case do_open:
        s = shr_open(ring, SHR_RDONLY);
        if (s == NULL) goto done;
        printf("r: open\n");
        break;
      case do_get_fd:
        printf("r: get selectable fd\n");
        selectable_fd = shr_get_selectable_fd(s);
        if (selectable_fd < 0) goto done;
        break;
      case do_select:
        printf("r: select\n");
        fd_set fds;
        FD_ZERO(&fds);
        FD_SET(selectable_fd, &fds);
        struct timeval tv = {.tv_sec = 1, .tv_usec =0};
        rc = select(selectable_fd+1, &fds, NULL, NULL, &tv);
        if (rc < 0) printf("r: select %s\n", strerror(errno));
        else if (rc == 0) printf("r: timeout\n");
        else if (rc == 1) printf("r: ready\n");
        else assert(0);
        break;
      case do_read:
        do {
          printf("r: read\n");
          rc = shr_read(s, &c, sizeof(c)); // byte read
          if (rc > 0) printf("r: [%c]\n", c);
          if (rc == 0) printf("r: wouldblock\n");
        } while (rc > 0);
        break;
      case do_empty:
        // printf("r: empty\n");
        gettimeofday(&tv_start,NULL);
        for(n=0; n < nmsg; n++) {
          rc = shr_read(s, buf, sizeof(buf));
          //fprintf(stderr,"r\n");
          if (rc != sizeof(buf)) printf("r: %d != %d\n", (int)rc, (int)sizeof(buf));
        }
        gettimeofday(&tv_end,NULL);
        print_elapsed("r", &tv_start,&tv_end,nmsg);
        break;
      case do_close:
        assert(s);
        shr_close(s);
        printf("r: close\n");
        break;
      default:
        assert(0);
        break;
    }
  }
 done:
  exit(0);
}

void w(int fd) {
  shr *s = NULL;
  char op;
  int rc, n;
  struct timeval tv_start, tv_end;
  struct iovec io[10];

  printf("w: ready\n");

  for(;;) {
    rc = read(fd, &op, sizeof(op));
    if (rc < 0) {
      fprintf(stderr,"w read: %s\n", strerror(errno));
      goto done;
    }
    if (rc == 0) {
      printf("w: eof\n");
      goto done;
    }
    assert(rc == sizeof(op));
    switch(op) {
      case do_open:
        s = shr_open(ring, SHR_WRONLY);
        if (s == NULL) goto done;
        printf("w: open\n");
        break;
      case do_write:
        printf("w: write\n");
        rc = shr_write(s, msg, sizeof(msg));
        if (rc != sizeof(msg)) printf("w: rc %d\n", rc);
        break;
      case do_fill:
        for(n=0; n < 10; n++) {
          io[n].iov_base = msg;
          io[n].iov_len = sizeof(msg);
        }
        gettimeofday(&tv_start,NULL);
        // printf("w: fill\n");
        for(n=0; n < nmsg; n+=10) {
          rc = shr_writev(s, io, 10);
          //fprintf(stderr,"w\n");
          if (rc != (int)(10*sizeof(msg))) printf("w: rc %d\n", rc);
        }
        gettimeofday(&tv_end,NULL);
        print_elapsed("w", &tv_start,&tv_end,nmsg);
        break;
      case do_unlink:
        printf("w: unlink\n");
        shr_unlink(s);
        break;
      case do_close:
        assert(s);
        shr_close(s);
        printf("w: close\n");
        break;
      default:
        assert(0);
        break;
    }
  }
 done:
  exit(0);
}

#define issue(fd,op) do {                           \
  char cmd = op;                                    \
  if (write((fd), &cmd, sizeof(cmd)) < 0) {         \
    fprintf(stderr,"write: %s\n", strerror(errno)); \
    goto done;                                      \
  }                                                 \
} while(0)

int main() {
  int rc = 0;
  pid_t rpid,wpid;

  setbuf(stdout,NULL);
  unlink(ring);

  int pipe_to_r[2];
  int pipe_to_w[2];

  shr_init(ring, ring_sz, SHR_MESSAGES);

  if (pipe(pipe_to_r) < 0) goto done;

  rpid = fork();
  if (rpid < 0) goto done;
  if (rpid == 0) { /* child */
    close(pipe_to_r[1]);
    r(pipe_to_r[0]);
    assert(0); /* not reached */
  }

  /* parent */
  close(pipe_to_r[0]);

  delay();

  if (pipe(pipe_to_w) < 0) goto done;
  wpid = fork();
  if (wpid < 0) goto done;
  if (wpid == 0) { /* child */
    close(pipe_to_r[1]);
    close(pipe_to_w[1]);
    w(pipe_to_w[0]);
    assert(0); /* not reached */
  }
  /* parent */
  close(pipe_to_w[0]);

  int R = pipe_to_r[1];
  int W = pipe_to_w[1];

  issue(W, do_open); delay();
  issue(R, do_open); delay();


  /* contention begins here */

  issue(W, do_fill);
  issue(R, do_empty);

  issue(W, do_close); delay();
  issue(R, do_close); delay();

  close(W); delay();
  close(R);

  waitpid(wpid,NULL,0);
  waitpid(rpid,NULL,0);

 rc = 0;

done:
 unlink(ring);
 printf("end\n");
 return rc;
}

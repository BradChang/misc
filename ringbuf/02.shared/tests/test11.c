#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <errno.h>
#include <stdlib.h>
#include <unistd.h>
#include "shr_ring.h"

char *ring = "test.ring";
char *data = "abcdefghi";

#define do_open 'o'
#define do_close 'c'

void r(int fd) {
  shr *s = NULL;
  char op;
  int rc;

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
  int rc;

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

void delay() { usleep(50000); }

#define issue(fd,op) do {                           \
  char cmd = op;                                    \
  if (write((fd), &cmd, sizeof(cmd)) < 0) {         \
    fprintf(stderr,"write: %s\n", strerror(errno)); \
    goto done;                                      \
  }                                                 \
  delay();                                          \
} while(0)

int main() {
  int rc = 0;
  pid_t pid;

  setbuf(stdout,NULL);

  int pipe_to_r[2];
  int pipe_to_w[2];

  if (pipe(pipe_to_r) < 0) goto done;

  pid = fork();
  if (pid < 0) goto done;
  if (pid == 0) { /* child */
    close(pipe_to_r[1]);
    r(pipe_to_r[0]);
    assert(0); /* not reached */
  }

  /* parent */
  close(pipe_to_r[0]);

  delay();

  if (pipe(pipe_to_w) < 0) goto done;
  pid = fork();
  if (pid < 0) goto done;
  if (pid == 0) { /* child */
    close(pipe_to_r[1]);
    close(pipe_to_w[1]);
    w(pipe_to_w[0]);
    assert(0); /* not reached */
  }
  /* parent */
  close(pipe_to_w[0]);

  int R = pipe_to_r[1];
  int W = pipe_to_w[1];

  delay();

  issue(R, do_open);
  issue(W, do_open);

  issue(W, do_close);
  issue(R, do_close);

  close(W); delay();
  close(R); delay();

 rc = 0;

done:
 printf("end\n");
 return rc;
}

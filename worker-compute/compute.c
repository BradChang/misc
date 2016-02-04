#include <stdio.h>
#include <time.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/prctl.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>

/*
 * template of a multi-process (forking) compute program
 */

/* TODO
 * signal handling
 */


#define NUM_CONFIGS_ALL (1UL << 27U)

/* a few macros used to implement a bit vector */
#define BIT_TEST(c,i)  (c[i/8] &   (1 << (i % 8)))
#define BIT_SET(c,i)   (c[i/8] |=  (1 << (i % 8)))
#define BIT_CLEAR(c,i) (c[i/8] &= ~(1 << (i % 8)))

struct {
  int verbose;
  char *file; /* output file */
  int fd;     /* of file */
  int workers;
  int worker_idx; /* 0-workers */
  /* internals */
  char *buf;     /* mmap'd output file */
  size_t buf_sz; /* size of above (bytes) */
  time_t start;  
  time_t end;  
  int sts_pipe[2]; /* [0] = read end, [1]=write end */
} CF = {
  .buf_sz = NUM_CONFIGS_ALL / 8, /* this many bytes to hold that bit array */
  .fd = -1,
  .workers = 1,
  .file = "output.dat",
};

struct worker_status {
  int idx;
  int status;
};

void usage(char *prog) {
  fprintf(stderr, "usage: %s [-v] [-f <file>] [-j <#workers>]\n", prog);
  exit(-1);
}

/* executed in each worker */
int work() {
  struct worker_status ps;
  ps.idx = CF.worker_idx;
  ps.status = 0;

  prctl(PR_SET_PDEATHSIG, SIGHUP); // get signal on parent exit
  close(CF.sts_pipe[0]); // close read end

  do {
    sleep(10); // replace this with real work
    ps.status += 10;
    if (write(CF.sts_pipe[1], &ps, sizeof(ps)) < 0) {
      fprintf(stderr,"worker %d: write %s\n", ps.idx, strerror(errno));
    }
  } while (ps.status < 100);

  /* close pipe. when all workers have closed it, parent gets eof */
  close(CF.sts_pipe[1]);
  return 0;
}

void cleanup_mapping() {
  if (CF.buf && (CF.buf != MAP_FAILED)) {
    munmap(CF.buf, CF.buf_sz);
    CF.buf == NULL;
  }
  if (CF.fd != -1) {
    close(CF.fd);
    CF.fd = -1;
  }
}

int map() {
  int rc=-1;

  if ( (CF.fd = open(CF.file, O_RDWR|O_CREAT|O_TRUNC, 0644)) == -1) {
      fprintf(stderr,"open %s: %s\n", CF.file, strerror(errno));
      goto done;
  }

  if (ftruncate(CF.fd, CF.buf_sz) == -1) {
      fprintf(stderr,"ftruncate: %s\n", strerror(errno));
      goto done;
  }

  CF.buf = mmap(0, CF.buf_sz, PROT_READ|PROT_WRITE, MAP_SHARED, CF.fd, 0);
  if (CF.buf == MAP_FAILED) {
      fprintf(stderr, "mmap %s\n", strerror(errno));
      goto done;
  }

 rc = 0;

 done:
  return rc;
}

int main(int argc, char *argv[]) {
  struct worker_status ps;
  int opt,verbose=0,rc,i;
  pid_t pid;

  while ( (opt = getopt(argc, argv, "v+f:hj:")) != -1) {
    switch (opt) {
      case 'v': CF.verbose++; break;
      case 'f': CF.file=strdup(optarg); break;
      case 'j': CF.workers=atoi(optarg); break;
      case 'h': default: usage(argv[0]); break;
    }
  }

  time(&CF.start);
  fprintf(stderr,"starting at %s", asctime(localtime(&CF.start)));

  if (pipe(CF.sts_pipe) < 0) goto done;
  if (map() < 0) goto done;

  while (CF.worker_idx < CF.workers) {
    pid = fork();
    if (pid < 0) {fprintf(stderr,"fork: %s\n", strerror(errno)); goto done;}
    if (pid > 0) fprintf(stderr,"worker %d: pid %d\n", CF.worker_idx, pid);
    if (pid== 0) return work();
    CF.worker_idx++;
  }

  close(CF.sts_pipe[1]); // parent close write end of pipe

  do {
    rc = read(CF.sts_pipe[0], &ps, sizeof(ps));
    if (rc == sizeof(ps)) fprintf(stderr,"worker %d: %d\n", ps.idx, ps.status);
    if (rc <= 0) fprintf(stderr,"read: %s\n", (rc<0) ? strerror(errno) : "eof");
  } while (rc > 0);

  for(i=0; i < CF.workers; i++) { wait(NULL); fprintf(stderr,"worker exited\n"); }

  time(&CF.end);
  fprintf(stderr,"ending at %s", asctime(localtime(&CF.end)));

 done:
  cleanup_mapping();
}

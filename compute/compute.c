#include <stdio.h>
#include <time.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>

/*
 *
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
  int index;  /* TODO */
  int workers;
  /* internals */
  char *buf;     /* mmap'd output file */
  size_t buf_sz; /* size of above (bytes) */
  time_t start;  
  time_t end;  
} CF = {
  .buf_sz = NUM_CONFIGS_ALL / 8, /* this many bytes to hold that bit array */
  .fd = -1,
  .file = "3x3x3.dat",
};

void usage(char *prog) {
  fprintf(stderr, "usage: %s [-v] [-f <file>] [-i <i/j>]\n", prog);
  fprintf(stderr,"        i/j = index of this worker, of j workers\n");
  exit(-1);
}

void compute() {
  long i;
  char *c = CF.buf;
  for(i=0; i<NUM_CONFIGS_ALL; i++) {
    /* work goes here */
    BIT_SET(c,i);
  }
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
  int opt,verbose=0;
  while ( (opt = getopt(argc, argv, "v+f:hi:j:")) != -1) {
    switch (opt) {
      case 'v': CF.verbose++; break;
      case 'f': CF.file=strdup(optarg); break;
      case 'i': CF.index=atoi(optarg); break;
      case 'j': CF.workers=atoi(optarg); break;
      case 'h': default: usage(argv[0]); break;
    }
  }
  if (map() < 0) goto done;

  time(&CF.start);
  fprintf(stderr,"starting at %s", asctime(localtime(&CF.start)));

  compute();

  time(&CF.end);
  fprintf(stderr,"ending at %s", asctime(localtime(&CF.end)));

 done:
  cleanup_mapping();
}

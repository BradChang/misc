#include <string.h>
#include <assert.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include "eccode.h"

/* see section 17 of Claude Shannon's 1948 paper 
 * A Mathematical Theory of Communication for the
 * example of an error correcting code used here */

struct {
  char *prog;
  int verbose;
  int mode;

  char *ifile;
  unsigned char *ibuf;
  size_t ilen, ibits;

  char *ofile;
  unsigned char *obuf;
  size_t olen, obits;

} CF = {
  .mode = MODE_ENCODE,
};


void usage() {
  fprintf(stderr,"usage: %s <options> -i <file> -o <file>\n", CF.prog);
  fprintf(stderr," options: -v (verbose)\n");
  fprintf(stderr,"          -h (help)\n");
  fprintf(stderr,"          -e (encode) [default]\n");
  fprintf(stderr,"          -d (decode)\n");
  fprintf(stderr,"          -E (extended encode)\n");
  fprintf(stderr,"          -D (extended decode)\n");
  fprintf(stderr,"          -n (correctable noise- disturb 1/7 bits)\n");
  fprintf(stderr,"          -N (correctable noise- disturb 1/8 bits)\n");
  fprintf(stderr,"          -u (uncorrectable noise- disturb 2/7 bits)\n");
  fprintf(stderr,"          -U (uncorrectable noise- disturb 2/8 bits)\n");
  exit(-1);
}

int mmap_input(void) {
  struct stat s;
  int fd, rc=-1;

  if ( (fd = open(CF.ifile, O_RDONLY)) == -1) {
    fprintf(stderr,"can't open %s: %s\n", CF.ifile, strerror(errno));
    goto done;
  }

  if (fstat(fd, &s) == -1) {
    fprintf(stderr,"can't stat %s: %s\n", CF.ifile, strerror(errno));
    goto done;
  }

  CF.ilen = s.st_size;
  CF.ibuf = mmap(0, CF.ilen, PROT_READ, MAP_PRIVATE, fd, 0);
  if (CF.ibuf == MAP_FAILED) {
    fprintf(stderr, "failed to mmap %s: %s\n", CF.ifile, strerror(errno));
    goto done;
  }

  rc = 0;

 done:
  if (fd != -1) close(fd);
  return rc;
}

int mmap_output(void) {
  int fd, rc=-1;

  if ( (fd = open(CF.ofile, O_RDWR|O_CREAT|O_TRUNC,0644)) == -1) {
    fprintf(stderr,"can't open %s: %s\n", CF.ofile, strerror(errno));
    goto done;
  }

  if (ftruncate(fd, CF.olen) == -1) {
    fprintf(stderr,"ftruncate: %s\n", strerror(errno));
    goto done;
  }

  CF.obuf = mmap(0, CF.olen, PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0);
  if (CF.obuf == MAP_FAILED) {
    fprintf(stderr, "failed to mmap %s: %s\n", CF.ofile, strerror(errno));
    goto done;
  }

  rc = 0;

 done:
  if (fd != -1) close(fd);
  return rc;
}

int main(int argc, char *argv[]) {
  int opt, rc=-1;
  CF.prog = argv[0];

  while ( (opt = getopt(argc,argv,"vednEDNuUhi:o:")) > 0) {
    switch(opt) {
      case 'v': CF.verbose++; break;
      case 'e': CF.mode=MODE_ENCODE; break;
      case 'd': CF.mode=MODE_DECODE; break;
      case 'n': CF.mode=MODE_NOISE; break;
      case 'u': CF.mode=MODE_NOISE_UC; break;
      case 'E': CF.mode=MODE_XENCODE; break;
      case 'D': CF.mode=MODE_XDECODE; break;
      case 'N': CF.mode=MODE_XNOISE; break;
      case 'U': CF.mode=MODE_XNOISE_UC; break;
      case 'i': CF.ifile=strdup(optarg); break;
      case 'o': CF.ofile=strdup(optarg); break;
      case 'h': default: usage(); break;
    }
  }

  if ((!CF.ifile) || (!CF.ofile)) usage();
  if (mmap_input() < 0) goto done;

  CF.olen = ecc_compute_olen(CF.mode, CF.ilen, &CF.ibits, &CF.obits);
  if (mmap_output() < 0) goto done;

  rc = ecc_recode(CF.mode, CF.ibuf, CF.ilen, CF.obuf);
  if (rc) fprintf(stderr,"ecc_recode error\n");

 done:
  if (CF.ibuf) munmap(CF.ibuf, CF.ilen);
  if (CF.obuf) munmap(CF.obuf, CF.olen);
  free(CF.ifile);
  free(CF.ofile);
  return rc;
}


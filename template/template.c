#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <errno.h>
#include <fcntl.h>

struct {
  int verbose;
  char *file; /* file to read. if null, read stdin */
  int fd;     /* input file descriptor def.0=stdin */
} CF;

void usage(char *prog) {
  fprintf(stderr,"usage: %s [-v] <file>\n", prog);
  exit(-1);
}

int main(int argc, char *argv[]) {
  int opt, rc=-1;

  while ( (opt = getopt(argc,argv,"vh")) > 0) {
    switch(opt) {
      case 'v': CF.verbose++; break;
      case 'h': default: usage(argv[0]); break;
    }
  }

  if (optind < argc) {
    CF.file = argv[optind++];
    CF.fd = open(CF.file ,O_RDONLY);
    if (CF.fd == -1) {
      fprintf(stderr,"open %s: %s\n", CF.file, strerror(errno));
      goto done;
    }
  }

  rc = 0;
 
 done:
  if (CF.fd > 0) close(CF.fd);
  return rc;
}

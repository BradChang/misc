#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include "tpl.h"

extern int ungz(char *in, size_t ilen, char **out, size_t *olen);
extern int make_schem_tpl(char *in, size_t ilen, char **out, size_t *olen);

struct {
  int verbose;
  int zcat; // dump unzipped data to stdout
} CF;

void usage(char *exe) {
  fprintf(stderr,"usage: %s [options] <file>\n", exe);
  fprintf(stderr,"          -v  (verbose)\n");
  fprintf(stderr,"          -z  (unzip to stdout)\n");
  exit(-1);
}

char *slurp(char *file, size_t *flen) {
  struct stat stats;
  char *out=NULL;
  int fd=-1, rc=-1;

  if (stat(file, &stats) == -1) {
    fprintf(stderr, "can't stat %s: %s\n", file, strerror(errno));
    goto done;
  }
  *flen  = stats.st_size;
  if (flen == 0) {
    fprintf(stderr, "file %s is zero length\n", file);
    goto done;
  }
  if ( (out = malloc(stats.st_size)) == NULL) {
    fprintf(stderr, "can't malloc space for %s\n", file);
    goto done;
  }
  if ( (fd = open(file,O_RDONLY)) == -1) {
    fprintf(stderr, "can't open %s: %s\n", file, strerror(errno));
    goto done;
  }
  if ( read(fd, out, stats.st_size) != stats.st_size) {
    fprintf(stderr, "short read on %s\n", file);
    goto done;
  }

  rc = 0;

 done:
  if ((rc < 0) && (out != NULL)) { free(out); out = NULL; }
  if (fd != -1) close(fd);
  return out;
}

int main( int argc, char *argv[]) {
  int rc=-1, opt;
  size_t ilen, ulen, img_len;
  char *file=NULL, *in, *unz=NULL, *img=NULL;

  while ( (opt = getopt(argc,argv,"vhz")) > 0) {
    switch(opt) {
      case 'v': CF.verbose++; break;
      case 'z': CF.zcat = 1; break;
      case 'h': default: usage(argv[0]); break;
    }
  }

  if (optind < argc) file = argv[optind++];
  if (file == NULL) usage(argv[0]);

  in = slurp(file, &ilen);
  if (in == NULL) goto done;

  rc = ungz(in, ilen, &unz, &ulen);
  if (rc) goto done;

  if (CF.zcat) write(STDOUT_FILENO,unz,ulen);

  rc = make_schem_tpl(unz, ulen, &img, &img_len);
  if (rc) goto done;

  rc = 0;

 done:
  if (in) free(in);
  if (unz) free(unz);
  if (img) free(img);
  return rc;
}

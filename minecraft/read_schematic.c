#include <zlib.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include "tpl.h"

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

#define want_gzip 16
#define def_windowbits (15 + want_gzip)
int ungz(char *in, size_t ilen, char **out, size_t *olen) {
  int rc = -1, zc;
  char *tmp;
  *out = NULL;
  *olen = 0;

  /* minimal required initialization of z_stream prior to inflateInit2 */
  z_stream zs = {.next_in = in, .avail_in=ilen, .zalloc=Z_NULL, 
                 .zfree=Z_NULL, .opaque=NULL};
  zc = inflateInit2(&zs, def_windowbits);
  if (zc != Z_OK) {
    fprintf(stderr, "inflateInit failed: %s\n", zs.msg);
    goto done;
  }

  /* start with a guess of the space needed to uncompress */
  size_t gzmax = ilen * 3;
  *out = malloc(gzmax);
  if (*out == NULL) { fprintf(stderr, "oom\n"); goto done; }

  /* initialize the remaining parts of z_stream prior to actual deflate */
  zs.next_out = *out;
  zs.avail_out = gzmax;

  /* inflate it .. cannot do this in one pass since final size unknown */
 keepgoing:
  zc = inflate(&zs, Z_NO_FLUSH);
  if ((zc == Z_OK) || (zc == Z_BUF_ERROR)) { /* need to grow buffer */
    off_t cur = (char*)zs.next_out - *out; /* save offset */
    tmp = realloc(*out, gzmax*2);
    if (tmp == NULL) { fprintf(stderr,"oom\n"); goto done; } 
    *out = tmp;
    zs.next_out = *out + cur;
    zs.avail_out += gzmax;
    gzmax *= 2;
    goto keepgoing;
  }
  if (zc != Z_STREAM_END) {
    if (zc == Z_DATA_ERROR) fprintf(stderr,"input data corrupted\n");
    else if (zc == Z_STREAM_ERROR) fprintf(stderr,"stream error\n");
    else if (zc == Z_MEM_ERROR) fprintf(stderr,"insufficient memory\n");
    else fprintf(stderr,"unknown error\n");
    goto done;
  }
  zc = inflateEnd(&zs);
  if (zc != Z_OK) {
    fprintf(stderr,"inflateEnd error: %s\n", zs.msg);
    goto done;
  }

  *olen = zs.total_out;
  rc = 0;

 done:
  if ((rc < 0) && (*out != NULL)) {
    free(*out);
    *out=NULL;
    *olen=0;
  }
  return rc;
}

int main( int argc, char *argv[]) {
  int rc=-1, opt;
  size_t ilen, olen;
  char *file=NULL, *in, *out=NULL;

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

  rc = ungz(in, ilen, &out, &olen);
  if (rc) goto done;

  if (CF.zcat) write(STDOUT_FILENO,out,olen);

  rc = 0;

 done:
  if (in) free(in);
  if (out) free(out);
  return rc;
}

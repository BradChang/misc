#include <zlib.h>
#include <stdio.h>
#include <stdlib.h>

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

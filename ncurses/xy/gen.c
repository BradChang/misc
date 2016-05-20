#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <errno.h>
#include <fcntl.h>
#include "tpl.h"

struct {
  int verbose;
  char *file; /* file to write */
  int fd;     /* input file descriptor def.0=stdin */
} CF;

void usage(char *prog) {
  fprintf(stderr,"usage: %s [-v] <file>\n", prog);
  exit(-1);
}

void do_gen() {

  unsigned min_x, min_y, x, y, xs;
  char coord[10];
  char *s = coord;
  tpl_node *tn= tpl_map("iiA(iis)", &min_x, &min_y, &xs, &y, &s);
  min_x = 30;
  min_y = 10;
  unsigned w = 7; /* column width */

  for(x=0; x < 10; x++) {
    xs = x * w;
    for(y=0; y < 5; y++) {
      snprintf(coord,sizeof(coord),"%u,%u",xs,y);
      tpl_pack(tn,1);
    }
  }
  tpl_pack(tn,0);
  tpl_dump(tn, TPL_FD, CF.fd);
  tpl_free(tn);
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
    CF.fd = open(CF.file ,O_WRONLY|O_CREAT|O_TRUNC, 0644);
    if (CF.fd == -1) {
      fprintf(stderr,"open %s: %s\n", CF.file, strerror(errno));
      goto done;
    }
  }

  do_gen();

  rc = 0;
 
 done:
  if (CF.fd > 0) close(CF.fd);
  return rc;
}

#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <stdio.h>
#include <errno.h>
#include <fcntl.h>
#include <ncurses.h>
#include "tpl.h"

struct {
  int verbose;
  char *file; /* file to read. if null, read stdin */
  int fd;     /* input file descriptor def.0=stdin */
  int delay;  
} CF = {
  .delay = 1,
  .fd = -1,
};

void usage(char *prog) {
  fprintf(stderr,"usage: %s [-v] <file>\n", prog);
  exit(-1);
}

int setup(unsigned minx, unsigned miny) {
  int rc = -1;
  unsigned rows, cols;

  initscr();
  getmaxyx(stdscr, rows, cols);

  if ((rows < minx) || (cols < miny)) {
    fprintf(stderr,"screen too small\n");
    goto done;
  }

  clear();
  if (CF.verbose) fprintf(stderr,"screen is %dx%d. ", rows, cols);
  refresh();
  rc = 0;

 done:
  return rc;
}

int count=0;
void draw(unsigned x, unsigned y, char *s) {
  move(y,x);
  if ((count++ % 2) == 0) attron(A_REVERSE);
  printw(s);
  attroff(A_REVERSE);
  refresh();
  sleep(1);
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

  unsigned minx, miny, x, y;
  char *s;
  int tc;
  tpl_node *tn = tpl_map("iiA(iis)", &minx, &miny, &x, &y, &s);
  assert(tn);

  while( tpl_load(tn, TPL_FD, CF.fd) == 0) {
    tpl_unpack(tn,0);
    if (setup(minx,miny) < 0) goto done;
    while(tpl_unpack(tn,1) > 0) {
      //fprintf(stderr,"from tpl: %u, %u, %s\n", x, y, s);
      draw(x,y,s);
      free(s);
    }
    getch();
  }
  tpl_free(tn);

  rc = 0;
 
 done:
  endwin();
  if (CF.fd != -1) close(CF.fd);
  return rc;
}

#include <string.h>
#include <assert.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <errno.h>
#include <fcntl.h>
#include "shr_ring.h"

/* TODO
 *
 * rewind mode
 * from-file mode
 * watch-dir mode
 * to-files mode
 * ncurses mode
 *
 */

struct {
  char *prog;
  int verbose;
  char *ring;
  enum {mode_status, mode_create, mode_unlink} mode;
  struct shr *shr;
  size_t size;
  int flags;
} CF = {
};

void usage() {
  fprintf(stderr,"usage: %s [options] <ring>\n", CF.prog);
  fprintf(stderr,"options:\n"
                 "         -c [-f <mode>] [-s <size>]      (create ring)\n"
                 "         -q                              (status ring) [default]\n"
                 "         -u                              (unlink ring)\n"
                 "\n"
                 "  <size> is allowed to have k/m/g/t suffix\n"
                 "  <mode> is a legal combination of okml\n"
                 "         o = overwrite, clear and resize ring, if it already exists\n"
                 "         k = keep existing ring size/content, if it already exists\n"
                 "         m = message mode; each i/o is a full message, not a stream\n"
                 "         l = lru-overwrite; reclaim oldest elements when ring fills\n"
                 "\n");
  exit(-1);
}

int main(int argc, char *argv[]) {
  int opt, rc=-1, sc;
  CF.prog = argv[0];
  char unit, *c;
  struct shr_stat stat;

  while ( (opt = getopt(argc,argv,"vhcs:qf:u")) > 0) {
    switch(opt) {
      case 'v': CF.verbose++; break;
      case 'h': default: usage(); break;
      case 'c': CF.mode=mode_create; break;
      case 'q': CF.mode=mode_status; break;
      case 'u': CF.mode=mode_unlink; break;
      case 's':  /* ring size */
         sc = sscanf(optarg, "%ld%c", &CF.size, &unit);
         if (sc == 0) usage();
         if (sc == 2) {
            switch (unit) {
              case 't': case 'T': CF.size *= 1024; /* fall through */
              case 'g': case 'G': CF.size *= 1024; /* fall through */
              case 'm': case 'M': CF.size *= 1024; /* fall through */
              case 'k': case 'K': CF.size *= 1024; break;
              default: usage(); break;
            }
         }
         break;
      case 'f': /* ring mode */
         c = optarg;
         while((*c) != '\0') {
           switch (*c) {
             case 'o': CF.flags |= SHR_OVERWRITE; break;
             case 'k': CF.flags |= SHR_KEEPEXIST; break;
             case 'm': CF.flags |= SHR_MESSAGES; break;
             case 'l': CF.flags |= SHR_LRU_STOMP; break;
             default: usage(); break;
           }
           c++;
         }
         break;
    }
  }

  if (optind < argc) CF.ring = argv[optind++];
  if (CF.ring == NULL) usage();
  
  switch(CF.mode) {

    case mode_create:
      rc = shr_init(CF.ring, CF.size, CF.flags);
      if (rc < 0) goto done;
      break;

    case mode_status:
      if (CF.size || CF.flags) usage();
      CF.shr = shr_open(CF.ring, SHR_RDONLY);
      if (CF.shr == NULL) goto done;
      rc = shr_stat(CF.shr, &stat, NULL);
      if (rc < 0) goto done;
      printf("w: bw %ld, br %ld, mw %ld, mr %ld, md %ld, bd %ld, bn %ld, bu %ld mu %ld\n",
         stat.bw, stat.br, stat.mw, stat.mr, stat.md, stat.bd, stat.bn, stat.bu, stat.mu);
      break;

    case mode_unlink:
      if (CF.size || CF.flags) usage();
      CF.shr = shr_open(CF.ring, SHR_RDONLY);
      if (CF.shr == NULL) goto done;
      rc = shr_unlink(CF.shr);
      if (rc < 0) goto done;
      break;

    default: 
      assert(0);
      break;
  }

  rc = 0;
 
 done:
  if (CF.shr) shr_close(CF.shr);
  return rc;
}

/* this program listens on a TCP port. when a client connects, it is sent
 * the packet stream (each packet prefixed with a length int). */
#define _GNU_SOURCE
#include <sys/types.h>
#include <errno.h>
#include <dirent.h>
#include <sys/inotify.h>
#include <signal.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <assert.h>
#include <pcap.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "utarray.h"
#include "utstring.h"

struct {
  int verbose;
  char *prog;
  char *dir;
  char *dev;
  char *file;
  char *filter;
  int fd,wd;
  enum { none, sniff, one_file, read_dir, watch_dir } mode;
  pcap_t *pcap;
  struct bpf_program fp;
  char err[PCAP_ERRBUF_SIZE];
  char path[PATH_MAX];
  int maxsz;
  int path_file_idx;
  int ticks;
  int port;
  int listener_fd;
  size_t eb_sz;
  UT_array *clients;
  union {
    /* see inotify(7) as inotify_event has a trailing name
     * field allocated beyond the fixed structure; we must
     * reserve enough room for the kernel to populate it */
    struct inotify_event eb;
    char extra[sizeof(struct inotify_event) + PATH_MAX];
  } e;
} cfg = {
  .maxsz = 65535,
  .fd = -1,
  .eb_sz = sizeof(struct inotify_event) + PATH_MAX,
};

#define tcpdump_cmd "tcpdump -G 10 -C 10 -s 0 -w %Y%m%d%H%M%S.pcap"
void usage() {
  fprintf(stderr,"usage: %s [-v] -f <bpf-filter>                \n"
                 "               -i <eth>   (read from interface), or\n"
                 "               -r <file>  (read one pcap file),  or\n"
                 "               -d <dir>   (read directory of pcaps),  or\n"
                 "               -w <dir>   (watch incoming pcap directory)*\n"
                 "\n"
                 "               -p <port>  (tcp port to listen on) \n"
                 "\n"
                 " * -w mode is useful with externally-generated,\n"
                 "           pcap written at time intervals, e.g.\n"
                 "           %s\n", 
          cfg.prog, tcpdump_cmd);
  exit(-1);
}

/* signals that we'll unblock during sigsuspend */
int sigs[] = {0,SIGHUP,SIGTERM,SIGINT,SIGQUIT,SIGALRM};

int setup_listener() {
  int rc = -1, fl;

  /**********************************************************
   * create an IPv4/TCP socket, not yet bound to any address
   *********************************************************/
  int fd = socket(AF_INET, SOCK_STREAM, 0);
  if (fd == -1) {
    fprintf(stderr,"socket: %s\n", strerror(errno));
    goto done;
  }
  int one=1;
  setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one));

  /**********************************************************
   * internet socket address structure: our address and port
   *********************************************************/
  struct sockaddr_in sin;
  sin.sin_family = AF_INET;
  sin.sin_addr.s_addr = htonl(INADDR_ANY);
  sin.sin_port = htons(cfg.port);

  /**********************************************************
   * bind socket to address and port we'd like to receive on
   *********************************************************/
  if (bind(fd, (struct sockaddr*)&sin, sizeof(sin)) == -1) {
    fprintf(stderr,"bind: %s\n", strerror(errno));
    goto done;
  }

  /**********************************************************
   * put socket into listening state 
   *********************************************************/
  if (listen(fd,1) == -1) {
    fprintf(stderr,"listen: %s\n", strerror(errno));
    goto done;
  }

  /* request signal SIGRTMIN to be sent to us when descriptor ready */
  fl = fcntl(fd, F_GETFL); 
  fl |= O_ASYNC;                   /* want a signal on fd ready */
  fcntl(fd, F_SETFL, fl);
  fcntl(fd, F_SETSIG, sigs[0]);    /* use this instead of SIGIO */
  fcntl(fd, F_SETOWN, getpid());   /* send it to our pid */

  cfg.listener_fd = fd;
  rc=0;

 done:
  if ((rc < 0) && (fd != -1)) close(fd);
  return rc;
}

void cb(u_char *data, const struct pcap_pkthdr *hdr, const u_char *pkt) {
  if (cfg.verbose) fprintf(stderr,"packet of length %d\n", hdr->len);
  int *fd=NULL;
  while ( (fd=(int*)utarray_next(cfg.clients,fd))) {
    if (*fd == 0) continue;
    /* write packet size , then packet, on socket error mark as fd 0 */
    if ((write(*fd, &hdr->len, sizeof(uint32_t)) != sizeof(uint32_t)) ||
        (write(*fd, pkt, hdr->len) != hdr->len)) *fd=0;
  }
}

int set_filter() {
  if (cfg.filter == NULL) return 0;

  int rc=-1;
  if ( (rc = pcap_compile(cfg.pcap, &cfg.fp, cfg.filter, 0, PCAP_NETMASK_UNKNOWN)) != 0) {
    fprintf(stderr, "error in filter expression: %s\n", cfg.err);
    goto done;
  }
  if ( (rc = pcap_setfilter(cfg.pcap, &cfg.fp)) != 0) {
    fprintf(stderr, "can't set filter expression: %s\n", cfg.err);
    goto done;
  }
  rc=0;

 done:
  return rc;
}

int ingest_pcap(char *file) {
  int rc = -1;
  strncpy(&cfg.path[cfg.path_file_idx], file, PATH_MAX-cfg.path_file_idx);
  if (cfg.verbose) fprintf(stderr,"reading file %s\n", cfg.path);
  cfg.pcap = pcap_open_offline(cfg.path, cfg.err);
  if (cfg.pcap==NULL)  {fprintf(stderr,"can't open %s: %s\n", cfg.path, cfg.err); goto done;}
  if (set_filter()) goto done;
  rc = pcap_loop(cfg.pcap, 0, cb, NULL);  

 done:
  if (cfg.pcap) {pcap_close(cfg.pcap); cfg.pcap=NULL;}
  return rc;
}

void do_stats(void) {
  struct pcap_stat ps;
  if (cfg.mode != sniff) return;
  if (cfg.verbose == 0 ) return;
  if (pcap_stats(cfg.pcap,&ps)<0) {fprintf(stderr,"pcap_stat error\n"); return;}
  fprintf(stderr,"received : %u\n", ps.ps_recv);
  fprintf(stderr,"dropped: %u\n", ps.ps_drop);
}

static int name_sort(const void *_a, const void *_b) {
  char **a = (char**)_a;
  char **b = (char**)_b;
  return strcmp(*a,*b);
}
static int get_files(char *dir, UT_array *filenames) {
  int rc = -1, i;
  struct dirent *dent;
  struct stat sb;
  char *name;
  DIR *d;

  UT_string *s; 
  utstring_new(s);
  utarray_clear(filenames);

  if ( (d = opendir(dir)) == NULL) {
    fprintf(stderr, "failed to opendir %s: %s\n", dir, strerror(errno));
    goto done;
  }

  while ( (dent = readdir(d)) != NULL) {
    if (dent->d_type != DT_REG) continue;
    name = dent->d_name;
    utstring_clear(s);
    utstring_printf(s, "%s/%s", dir, name);
    char *path = utstring_body(s);
    utarray_push_back(filenames, &path);
  }
  utarray_sort(filenames, name_sort);
  rc = 0;

 done:
  if (d) closedir(d);
  utstring_free(s);
  return rc;
}

int main(int argc, char *argv[]) {
  struct inotify_event *ev, *nx;
  int opt, rc, n, fl, num_bytes;
  UT_array *filenames;
  size_t sz;
  cfg.prog = argv[0];
  sigs[0] = SIGRTMIN+0;  /* we'll choose this RT signal for I/O readiness */
  utarray_new(cfg.clients,&ut_int_icd);
  siginfo_t info;
  int signo;

  while ( (opt=getopt(argc,argv,"vr:i:w:f:d:p:h")) != -1) {
    switch(opt) {
      case 'v': cfg.verbose++; break;
      case 'r': cfg.mode = one_file;  cfg.file=strdup(optarg); break;
      case 'i': cfg.mode = sniff;     cfg.dev=strdup(optarg); break; 
      case 'w': cfg.mode = watch_dir; cfg.dir=strdup(optarg); break; 
      case 'd': cfg.mode = read_dir;  cfg.dir=strdup(optarg); break; 
      case 'f': cfg.filter=strdup(optarg); break; 
      case 'p': cfg.port=atoi(optarg); break; 
      case 'h': default: usage(); break;
    }
  }
  if (cfg.mode==none) usage();
  if (cfg.port==0) usage();
  if (setup_listener()) goto done;

  /* block all signals. we stay blocked always except in sugsuspend */
  sigset_t all;
  sigfillset(&all);
  sigprocmask(SIG_SETMASK,&all,NULL);

  /* a few signals we'll accept during sigwaitinfo */
  sigset_t sw;
  sigemptyset(&sw);
  for(n=0; n < sizeof(sigs)/sizeof(*sigs); n++) sigaddset(&sw, sigs[n]);

  switch(cfg.mode) { // initial setup
    case watch_dir:
      /* setup an inotify watch on the pcap directory. */
      cfg.fd = inotify_init();
      if (cfg.fd==-1) {fprintf(stderr, "inotify_init: %s\n", strerror(errno)); goto done;}
      cfg.wd = inotify_add_watch(cfg.fd, cfg.dir, IN_CLOSE_WRITE);
      if (cfg.wd==-1) {fprintf(stderr,"inotify_add_watch: %s",strerror(errno)); goto done;}
      /* request SIGRTMIN to our pid when fd is ready; see fcntl(2) */
      fl = fcntl(cfg.fd, F_GETFL);
      fl |= O_ASYNC | O_NONBLOCK;
      fcntl(cfg.fd, F_SETFL, fl);
      fcntl(cfg.fd, F_SETSIG, sigs[0]); 
      fcntl(cfg.fd, F_SETOWN, getpid());
      /* set up our scratch buffer for the file names */
      strncpy(cfg.path,cfg.dir,sizeof(cfg.path));
      strncat(cfg.path,"/",sizeof(cfg.path));
      cfg.path_file_idx = strlen(cfg.path);
      break;
    case sniff:
      /* open capture interface and get underlying descriptor */
      cfg.pcap = pcap_open_live(cfg.dev, cfg.maxsz, 1, 0, cfg.err);
      if (!cfg.pcap) {fprintf(stderr,"can't open %s: %s\n",cfg.dev,cfg.err); goto done;}
      cfg.fd = pcap_get_selectable_fd(cfg.pcap);
      if (cfg.fd<0) {fprintf(stderr,"can't get pcap descriptor\n"); goto done;}
      rc = pcap_setnonblock(cfg.pcap, 1, cfg.err);
      if (rc== -1) {fprintf(stderr,"can't set pcap nonblock: %s\n",cfg.err); goto done;}
      set_filter();
      /* request signal SIGRTMIN to be sent to us when descriptor ready */
      fl = fcntl(cfg.fd, F_GETFL); 
      fl |= O_ASYNC;                      /* want a signal on fd ready */
      fcntl(cfg.fd, F_SETFL, fl);
      fcntl(cfg.fd, F_SETSIG, sigs[0]);    /* use this instead of SIGIO */
      fcntl(cfg.fd, F_SETOWN, getpid());   /* send it to our pid */

      break;
    case one_file:
      ingest_pcap(cfg.file);
      goto done;
      break;
    case read_dir:
      utarray_new(filenames, &ut_str_icd);
      get_files(cfg.dir, filenames);
      char **f=NULL;
      while( (f=(char**)utarray_next(filenames,f))) ingest_pcap(*f);
      utarray_free(filenames);
      goto done;
      break;
    default: assert(0); break;
  }


 alarm(1);
 while ( (signo = sigwaitinfo(&sw, &info)) > 0) {

  switch (signo) {
    case SIGALRM: /* periodic work and reschedule */
      if ((++cfg.ticks % 10) == 0) do_stats();
      alarm(1);
      if (cfg.mode != watch_dir) break;
      /* in watch_Dir mode, check if there is data on inotify descriptor */
      if (ioctl(cfg.fd,FIONREAD,&num_bytes) != 0) {
        fprintf(stderr,"ioctl error %s\n",strerror(errno));
        goto done;
      }
      if (num_bytes == 0) break;
      fprintf(stderr,"unsignaled data on inotify descriptor (%u bytes)\n",num_bytes);
      signo = sigs[0]; 
      info.si_fd = -1; 
      // fall through
    default:      /* SIGRTMIN+0 or other (ctrl-c, kill, etc) */
      if (signo != sigs[0]) {
        printf("got signal %d\n", signo);
        goto done;
      }
      /* see if we have a new client connection */
      if (info.si_fd == cfg.listener_fd) {
        struct sockaddr_in cin;
        socklen_t cin_sz = sizeof(cin);
        int fd = accept(cfg.listener_fd,(struct sockaddr*)&cin, &cin_sz);
        if (fd == -1) {
          fprintf(stderr,"accept: %s\n", strerror(errno));
          goto done;
        }
        if (sizeof(cin)==cin_sz) fprintf(stderr,"connection from %s:%d\n", 
          inet_ntoa(cin.sin_addr), (int)ntohs(cin.sin_port));
        utarray_push_back(cfg.clients,&fd);
        break;
      }
      /* normal signal for pcap or inotify fd */
      switch(cfg.mode) { /* SIGRTMIN+0 */
        case sniff:
          // only process up to 10000 packets between checking for interrupts
          rc = pcap_dispatch(cfg.pcap,10000,cb,NULL); 
          if (rc<0) { pcap_perror(cfg.pcap,"pcap error: "); goto done; }
          break;
        case watch_dir:
          while ( (rc=read(cfg.fd,&cfg.e.eb,cfg.eb_sz)) > 0) {
            for(ev = &cfg.e.eb; rc > 0; ev = nx) {

              sz = sizeof(*ev) + ev->len;
              nx = (struct inotify_event*)((char*)ev + sz);
              rc -= sz;

              if (ev->len == 0) continue;
              if (ingest_pcap(ev->name)) goto done; 
            }
          }
          break;
        default: assert(0); break;
      }

      break;
  }
 }

done:
  if (cfg.pcap) pcap_close(cfg.pcap);
  if (cfg.fd > 0) close(cfg.fd);
  utarray_free(cfg.clients);
  if (cfg.listener_fd) close(cfg.listener_fd);
  return 0;
}

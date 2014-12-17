#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <sys/types.h>
#include <fcntl.h>
#include <assert.h>
#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <unistd.h>
#include <readline/readline.h>
#include <readline/history.h>
#include "tpl.h"
#include "utarray.h"
#include "utstring.h"

char *path = "/tmp/cp"; 
char prompt[100];
int running=1;

void usage(char *prog) {
  fprintf(stderr, "usage: %s [-v] [-s|-S socket] [-f file]\n", prog);
  fprintf(stderr, "          -v verbose\n");  
  fprintf(stderr, "          -s path to UNIX domain socket\n");  
  fprintf(stderr, "          -S path to socket (abstract namespace)\n");
  fprintf(stderr, "          -f file to read commands from\n");
  exit(-1);
}

int verbose;
char *file;
int fd;        // connection to unix domain socket (control port)

/* This little parsing function finds one word at a time from the
 * input line. It supports double quotes to group words together. */
const int ws[256] = {[' ']=1, ['\t']=1};
char *find_word(char *c, char **start, char **end) {
  int in_qot=0;
  while ((*c != '\0') && ws[*c]) c++; // skip leading whitespace
  if (*c == '"') { in_qot=1; c++; }
  *start=c;
  if (in_qot) {
    while ((*c != '\0') && (*c != '"')) c++;
    *end = c;
    if (*c == '"') { 
      in_qot=0; c++; 
      if ((*c != '\0') && !ws[*c]) {
        fprintf(stderr,"text follows quoted text without space\n"); return NULL;
      }
    }
    else {fprintf(stderr,"quote mismatch\n"); return NULL;}
  }
  else {
    while ((*c != '\0') && (*c != ' ')) {
      if (*c == '"') {fprintf(stderr,"start-quote within word\n"); return NULL; }
      c++;
    }
    *end = c;
  }
  return c;
}

int do_rqst(char *line) {
  char *c=line, *start=NULL, *end=NULL, **f, *file, *arg;
  tpl_node *tn=NULL,*tr=NULL;
  int rc = -1, len;
  tpl_bin bbuf;
  UT_string *s;

  utstring_new(s);
  tn = tpl_map("A(s)", &arg);

  /* parse the line into argv style words, pack and transmit the request */
  while(*c != '\0') {
    if ( (c = find_word(c,&start,&end)) == NULL) goto done;
    //fprintf(stderr,"[%.*s]\n", (int)(end-start), start);
    assert(start && end);
    len = end-start;
    utstring_clear(s);
    utstring_printf(s,"%.*s",len,start);
    arg = utstring_body(s);
    tpl_pack(tn,1);
    start = end = NULL;
  }
  rc = tpl_dump(tn, TPL_FD, fd);
  if (rc == -1) goto done;

  /* get the reply */
  tr = tpl_map("B", &bbuf);
  if (tpl_load(tr, TPL_FD, fd)) {fprintf(stderr,"protocol error\n"); goto done;}
  tpl_unpack(tr,0);
  if (bbuf.addr) printf("%.*s\n", (int)bbuf.sz, (char*)bbuf.addr);
  if (bbuf.addr) free(bbuf.addr);
  
  rc = 0;

 done:
  utstring_free(s);
  if (tn) tpl_free(tn);
  if (tr) tpl_free(tr);
  return rc;
}

void handle_file() {
  char buf[500];
  FILE *f;
  int len;

  f = fopen(file,"r");
  if (f == NULL) {
    fprintf(stderr,"fopen: %s\n",strerror(errno)); 
    goto done;
  }

  while(fgets(buf, sizeof(buf), f)) {
    len = strlen(buf);
    if ((len >= 2) && (buf[len-1] == '\n')) buf[len-1]='\0'; 
    else { fprintf(stderr, "line too long\n"); break; }
    if (do_rqst(buf)) break;
  }

 done:
  if (f) fclose(f);
  return;
}

/* called inside readline() whenever full line gathered */
void cb_linehandler(char *line) {
  int rc;

  if (line == NULL) { running=0; goto done; } // user EOF
  if (*line=='\0') goto done; // ignore user's empty line

  rc = do_rqst(line);
  if (rc) running=0;
  else add_history(line);

 done:
  if (line) free(line);
}
 
int main(int argc, char *argv[]) {
  struct sockaddr_un addr;
  int opt,rc;
  tpl_bin bbuf;
  char *line;

  while ( (opt = getopt(argc, argv, "v+f:s:S:")) != -1) {
    switch (opt) {
      case 'v': verbose++; break;
      case 'f': file = strdup(optarg); break;
      case 's': path = strdup(optarg); break;
      case 'S': path = calloc(strlen(optarg)+2,1); strcpy(path+1,optarg); break;
      default: usage(argv[0]); break;
    }
  }
  if (optind < argc) usage(argv[0]);
  snprintf(prompt,sizeof(prompt),"[controlsh] %s %% ", path);
  using_history();

  /**************************************************
   * connect to server
   *************************************************/
  if ( (fd = socket(AF_UNIX, SOCK_STREAM, 0)) == -1) {
    fprintf(stderr,"socket: %s\n",strerror(errno));
    goto done;
  }

  memset(&addr, 0, sizeof(addr));
  addr.sun_family = AF_UNIX;
  strncpy(addr.sun_path, path, sizeof(addr.sun_path)-1);

  if (connect(fd, (struct sockaddr*)&addr, sizeof(addr)) == -1) {
    fprintf(stderr, "connect: %s\n",strerror(errno));
    goto done;
  }
  fprintf(stderr,"Connected to %s.\n", path);

  /**************************************************
   * if we're reading commands from file, do so
   *************************************************/
  if (file) { handle_file(file); goto done; }

  /**************************************************
   * handle keyboard interactive input.
   *************************************************/
  rl_callback_handler_install(prompt, cb_linehandler);
  int readline_fd = fileno(rl_instream);
  fd_set fds;
  while(running) {
    FD_ZERO(&fds);
    FD_SET(readline_fd, &fds);
    FD_SET(fd, &fds);

    rc = select(FD_SETSIZE,&fds,NULL,NULL,NULL);
    if (rc < 0) {
      fprintf(stderr,"select: %s\n", strerror(errno));
      goto done;
    }

    /* handle user input, but terminate on server io/close */
    if (FD_ISSET(readline_fd, &fds)) rl_callback_read_char();
    if (FD_ISSET(fd, &fds)) { 
      fprintf(stderr,"Connection closed.\n");
      running=0;
    }
  }

 done:
  if (!file) rl_callback_handler_remove();
  if (fd) close(fd);
  return 0;
}


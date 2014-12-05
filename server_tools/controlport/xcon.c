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
FILE *filef;
char buf[256]; // max line length
int fd;        // connection to unix domain socket (control port)

/* the control port can became readable when we aren't expecting a response.
 * this probably means the control port is shutting down. check for this.
 * if found, confirm its EOF. any other I/O without EOF here would be a bug. */
int last_gasp(int fd) {
  char buf[100];
  fd_set rfds;
  int sr,rc;

  int flags;
  flags = fcntl(fd, F_GETFL);
  fcntl(fd, F_SETFL, flags | O_NONBLOCK);
  do {
    rc=read(fd,buf,sizeof(buf));
    if (rc > 0) fprintf(stderr,"control port: %.*s\n", rc, buf);
    else if (rc == 0) fprintf(stderr,"control port: closed\n");
  } while(rc > 0);
  fcntl(fd, F_SETFL, flags);

  return (rc==-1) ? 0 : 1;
}

char *next_line() {
  size_t len;
  char *line=NULL,*tmp;

  if (file) line=fgets(buf,sizeof(buf), filef);
  else {
    if (last_gasp(fd)) goto done; // before we block on stdin, see if cp closed
    line=readline(prompt);  // must free it
  }
  if (!line) goto done;

  len = strlen(line);
  
  if (file) { 
    /* fgets keeps trailing newline. null it out. if absent, line got truncated*/
    if (buf[len-1] == '\n') buf[len-1]='\0'; 
    else { fprintf(stderr, "line too long\n"); line=NULL; }
  } else {  
    /* copy the mallocd readline buffer and free it */
    tmp = line;
    if (len+1 < sizeof(buf)) {memcpy(buf, line, len+1); line=buf; }
    else { fprintf(stderr, "line too long\n"); line=NULL; }
    free(tmp); 
  }

 done:
  if (file && !line) fclose(filef);
  return line;
}

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

int do_rqst(char *line, int fd) {
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
  snprintf(prompt,sizeof(prompt),"%s> ", path);
  using_history();

  if ( (fd = socket(AF_UNIX, SOCK_STREAM, 0)) == -1) {
    perror("socket error");
    exit(-1);
  }

  memset(&addr, 0, sizeof(addr));
  addr.sun_family = AF_UNIX;
  strncpy(addr.sun_path, path, sizeof(addr.sun_path)-1);

  if (connect(fd, (struct sockaddr*)&addr, sizeof(addr)) == -1) {
    perror("connect error");
    exit(-1);
  }

  if (file && !(filef = fopen(file,"r"))) {
    perror("fopen error"); 
    exit(-1);
  }

  while ( (line=next_line()) != NULL) {
    add_history(line);
    if (do_rqst(line,fd) == -1) break;
  }

  clear_history();
  return 0;
}


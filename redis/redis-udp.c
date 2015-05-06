#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include "tconf.h"
#include "hiredis/hiredis.h"

#define CMD_MAX 100
char *cmds[CMD_MAX];
int ncmds=0;

int add_cmd(char *key, char *value) {
  if (ncmds==CMD_MAX) exit(-1);
  cmds[ncmds++] = strdup(value);
  return 0;
}

tconf_t tc[] = {{"redis", tconf_func, add_cmd}, };

#define BUFSZ 2000
char buf[BUFSZ];
char tmp[BUFSZ];
int port = 3001;             /* no significance */
char *conf = "cmds.conf";
int verbose;
char *redis_host="127.0.0.1";
int redis_port=6379;

void usage(char *prog) {
  fprintf(stderr,"usage: %s [-v] [-u <listen-port>] [-c <conf>] "
                 "[-s <redis-host>] [-p <redis-port]\n", prog);
  exit(-1);
}

int main(int argc, char *argv[]) {
  int rc=-1, i, opt;
  void *reply;

  while ( (opt = getopt(argc, argv, "v+u:c:s:p:")) != -1) {
    switch (opt) {
      case 'v': verbose++; break;
      case 'u': port=atoi(optarg); break;
      case 'c': conf=strdup(optarg); break;
      case 's': redis_host=strdup(optarg); break;
      case 'p': redis_port=atoi(optarg); break;
      default: usage(argv[0]); break;
    }
  }

  /* read config file */
  if (tconf(conf, tc, sizeof(tc)/sizeof(*tc), 0)) goto done;

  /* connect to redis */
  redisContext *c = redisConnect(redis_host,redis_port);
  if (c && c->err) {
    fprintf(stderr,"redisConnect: %s\n", c->errstr);
    goto done;
  }

  /**********************************************************
   * create an IPv4/UDP socket, not yet bound to any address
   *********************************************************/
  int fd = socket(AF_INET, SOCK_DGRAM, 0);
  if (fd == -1) {
    fprintf(stderr, "socket: %s\n", strerror(errno));
    exit(-1);
  }

  /**********************************************************
   * internet socket address structure: our address and port
   *********************************************************/
  struct sockaddr_in sin;
  sin.sin_family = AF_INET;
  sin.sin_addr.s_addr = htonl(INADDR_ANY);
  sin.sin_port = htons(port);

  /**********************************************************
   * bind socket to address and port we'd like to receive on
   *********************************************************/
  if (bind(fd, (struct sockaddr*)&sin, sizeof(sin)) == -1) {
    fprintf(stderr, "bind: %s\n", strerror(errno));
    exit(-1);
  }

  if (verbose) {
    fprintf(stderr,"starting with %d commands:\n", ncmds);
    for(i=0; i < ncmds; i++) fprintf(stderr, "%s\n", cmds[i]);
  }


  while (1) {
    rc = read(fd,buf,BUFSZ);
    if (rc==-1) {
      fprintf(stderr, "read: %s\n", strerror(errno));
      goto done;
    }

    snprintf(tmp,sizeof(tmp),"\"%.*s\"",rc,buf);
    for(i=0; i < ncmds; i++) {
      fprintf(stderr, "running: ");
      fprintf(stderr, cmds[i], tmp);
      fprintf(stderr, "\n");
      reply = redisCommand(c, cmds[i], tmp);
      if (reply) freeReplyObject(reply);
      else fprintf(stderr,"redisCommand: %s\n", c->errstr);
    }
  }

 done:
  return rc;
}

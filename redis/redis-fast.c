#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <sys/time.h>
#include <unistd.h>
#include "hiredis/hiredis.h"

int verbose;
char *redis_host="127.0.0.1";
int redis_port=6379;

void usage(char *prog) {
  fprintf(stderr,"usage: %s [-v] [-n <iterations>]"
                 "[-s <redis-host>] [-p <redis-port]\n", prog);
  exit(-1);
}

void print_result(int n, char *test, struct timeval tv_a, struct timeval tv_b) {
  long unsigned usec_elapsed = (tv_b.tv_sec * 1000000 + tv_b.tv_usec) - 
                               (tv_a.tv_sec * 1000000 + tv_a.tv_usec);
  long unsigned per_sec = n * 1000000.0 / usec_elapsed;
  fprintf(stderr,"%s %lu/s\n", test, per_sec);
}

int main(int argc, char *argv[]) {
  int rc=-1, i, opt, n=10000;
  void *reply;

  while ( (opt = getopt(argc, argv, "v+n:s:p:")) != -1) {
    switch (opt) {
      case 'v': verbose++; break;
      case 'n': n=atoi(optarg); break;
      case 's': redis_host=strdup(optarg); break;
      case 'p': redis_port=atoi(optarg); break;
      default: usage(argv[0]); break;
    }
  }

  /* connect to redis */
  redisContext *c = redisConnect(redis_host,redis_port);
  if (c && c->err) {
    fprintf(stderr,"redisConnect: %s\n", c->errstr);
    goto done;
  }

  struct timeval tv_a, tv_b;

  char big_str[100];
  for(i=0; i < sizeof(big_str); i++) big_str[i] = 'a'+(i%10);
  big_str[sizeof(big_str)-1] = '\0';

  /*************************************************************
   * big string push
   ************************************************************/
  gettimeofday(&tv_a,NULL);
  for(i=0; i < n; i++) {
    reply = redisCommand(c, "LPUSH list %s", big_str);
    if (reply) freeReplyObject(reply);
    else fprintf(stderr,"redisCommand: %s\n", c->errstr);
  }
  gettimeofday(&tv_b,NULL);
  print_result(n,"big string push", tv_a, tv_b);

  reply = redisCommand(c, "DEL list");
  if (reply) freeReplyObject(reply);
  else fprintf(stderr,"redisCommand: %s\n", c->errstr);

  /*************************************************************
   * big string push/trim 
   ************************************************************/
  gettimeofday(&tv_a,NULL);
  for(i=0; i < n; i++) {
    reply = redisCommand(c, "LPUSH list %s", big_str);
    if (reply) freeReplyObject(reply);
    else fprintf(stderr,"redisCommand: %s\n", c->errstr);
    reply = redisCommand(c, "LTRIM list 0 %u", n/2);
    if (reply) freeReplyObject(reply);
    else fprintf(stderr,"redisCommand: %s\n", c->errstr);
  }
  gettimeofday(&tv_b,NULL);
  print_result(n,"big string push/trim", tv_a, tv_b);

  reply = redisCommand(c, "DEL list");
  if (reply) freeReplyObject(reply);
  else fprintf(stderr,"redisCommand: %s\n", c->errstr);

  /*************************************************************
   * pipelined big string push/trim 
   ************************************************************/
  gettimeofday(&tv_a,NULL);
  for(i=0; i < n; i++) {
    redisAppendCommand(c, "LPUSH list %s", big_str);
    redisAppendCommand(c, "LTRIM list 0 %u", n/2);
    redisGetReply(c, &reply);
    if (reply) freeReplyObject(reply);
    else fprintf(stderr,"redisCommand: %s\n", c->errstr);
    redisGetReply(c, &reply);
    if (reply) freeReplyObject(reply);
    else fprintf(stderr,"redisCommand: %s\n", c->errstr);
  }
  gettimeofday(&tv_b,NULL);
  print_result(n,"pipelined big string push/trim", tv_a, tv_b);

  reply = redisCommand(c, "DEL list");
  if (reply) freeReplyObject(reply);
  else fprintf(stderr,"redisCommand: %s\n", c->errstr);



 done:
  return rc;
}

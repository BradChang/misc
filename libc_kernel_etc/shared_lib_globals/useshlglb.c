#include <string.h>
#include <stdio.h>
extern char shared[100];
int main(int argc, char *argv[]) {
  int n;

  for(n=0; n < 100; n++) shared[n]++;

  fprintf(stderr,"shared: ");
  for(n=0; n < 100; n++) {
      fprintf(stderr,"%d",(int)shared[n]);
  }
  fprintf(stderr,"\n");

  sleep(10);

}

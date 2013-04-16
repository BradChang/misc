#include "SDL/SDL.h"
#include "assert.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <string.h>
#include <errno.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <ftdi.h>

#define JOYAXIS_MAX 32768

unsigned char R,G,B,D;

int main(int argc, char *argv[]) {
  int i, n, a,b,l, w, p;
  SDL_Joystick *j=NULL;
  SDL_Event e;
  char *file;
  char *text=NULL;
  size_t text_sz=4;
  int fd=-1;

  if (argc > 1) file=argv[1];
  else {fprintf(stderr,"path required\n"); return -1;}
  if ( (fd = open(file, O_RDWR|O_CREAT, 0777)) == -1) {
    fprintf(stderr,"can't open %s: %s\n", file, strerror(errno));
    goto done;
  }
  if (ftruncate(fd, text_sz) == -1) {
    fprintf(stderr,"can't ftruncate %s: %s\n", file, strerror(errno));
    goto done;
  }
  text = mmap(0, text_sz, PROT_WRITE, MAP_SHARED, fd, 0);
  if (text == MAP_FAILED) {
    close(fd); fd = -1;
    fprintf(stderr,"Failed to mmap %s: %s\n", file, strerror(errno));
    goto done;
  }

  if (SDL_Init(SDL_INIT_EVERYTHING) == -1) {
    fprintf(stderr,"SDL init failed: %s\n", SDL_GetError());
    return -1;
  }
  n = SDL_NumJoysticks();
  if (n==0) {fprintf(stderr, "No joystick\n"); return 0;}

  j = SDL_JoystickOpen(0); // open the first one 
  if (!j) {fprintf(stderr,"can't open joystick: %s\n", SDL_GetError()); return -1;}

  fprintf(stderr,"detecting motion. press joystick button to exit\n");
  while ( (w=SDL_WaitEvent(&e)) != 0) {

    switch (e.type) {
    case SDL_JOYAXISMOTION: 
      if ((e.jaxis.value < -3200) || (e.jaxis.value > 3200)) {// reduce tweakiness
        p = (int)(e.jaxis.value*100.0/JOYAXIS_MAX) + 100;
        switch (e.jaxis.axis) {
         case 0: /* left right */ R = p; break;
         case 1: /* up down */ G = p; break;
         case 2: /* twist */ B = p; break;
         case 3: /* throttle */ D = p; break;
         default: break;
         }
        text[0] = R;
        text[1] = G;
        text[2] = B;
        text[3] = D;
      }
      break;

    //case SDL_JOYBUTTONDOWN: fprintf(stderr,"button press\n"); goto done; break;
    case SDL_QUIT: goto done; break;
    }
  }

 done:
  if (j) SDL_JoystickClose(j);
  if (text) munmap(text, text_sz);
  if (fd != -1) close(fd);
}

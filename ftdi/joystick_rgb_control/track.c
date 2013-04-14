#include "SDL/SDL.h"
#include "assert.h"
#include "tpl.h"
#define JOYAXIS_MAX 32768

int R,G,B,D;

int main(int argc, char *argv[]) {
  int i, n, a,b,l, w, p;
  SDL_Joystick *j;
  SDL_Event e;
  tpl_node *tn;
  tn = tpl_map("iiii",&R,&G,&B,&D);

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
        assert(R>=0 && R<256); assert(G>=0 && G<256); assert(B>=0 && B<256); assert(D>=0 && D<256);
        tpl_pack(tn, 0);
        tpl_dump(tn,TPL_FD,1);
      }
      break;

    //case SDL_JOYBUTTONDOWN: fprintf(stderr,"button press\n"); goto done; break;
    case SDL_QUIT: goto done; break;
    }
  }

 done:
  tpl_free(tn);
  SDL_JoystickClose(j);
}

#include <SDL2/SDL.h>
#include <stdio.h>
#include <unistd.h>

void usage(char* prog) {
  fprintf(stderr,"%s -f <bmp>\n", prog);
  exit(-1);
}

int main(int argc, char *argv[]) {
  SDL_Window *win = NULL;
  SDL_Renderer *ren = NULL;
  SDL_Surface *bmp = NULL;
  SDL_Texture *tex = NULL;
  char *file;
  int opt;

  while ( (opt = getopt(argc,argv,"f:h")) != -1) {
    switch (opt) {
       case 'f': file=strdup(optarg); break;
       case 'h': default: usage(argv[0]);
    }
  }
  if (!file) usage(argv[0]);

  if (SDL_Init(SDL_INIT_VIDEO)) {
    fprintf(stderr,"SDL_Init failed: %s\n", SDL_GetError());
    goto done;
  }

  win = SDL_CreateWindow("Hello World!", 100, 100, 640, 480, SDL_WINDOW_SHOWN);
  if (!win) {
    fprintf(stderr,"SDL_CreateWindow failed: %s\n", SDL_GetError());
    goto done;
  }

  ren = SDL_CreateRenderer(win, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
  if (!ren) {
    fprintf(stderr,"SDL_CreateRenderer failed: %s\n", SDL_GetError());
    goto done;
  }

  bmp = SDL_LoadBMP(file);
  if (!bmp) {
    fprintf(stderr,"SDL_LoadBMP failed: %s\n", SDL_GetError());
    goto done;
  }

  tex = SDL_CreateTextureFromSurface(ren, bmp);
  if (!tex) {
    fprintf(stderr,"SDL_CreateTextureFromSurface failed: %s\n", SDL_GetError());
    goto done;
  }
  SDL_FreeSurface(bmp); bmp=NULL; // done with original surface


  /* present */
  SDL_RenderClear(ren);
  SDL_RenderCopy(ren, tex, NULL, NULL);
  SDL_RenderPresent(ren);
  SDL_Delay(5000);

 done:
  if (win) SDL_DestroyWindow(win);
  if (ren) SDL_DestroyRenderer(ren);
  if (bmp) SDL_FreeSurface(bmp);
  SDL_Quit();
  return 0;
}

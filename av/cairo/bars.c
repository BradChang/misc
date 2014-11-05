/* draw a series of images of sliding color bars */
#include <cairo.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

int count=10;
int delta=10;
int bar_width=100;
int verbose;
int x=1920,y=1080;
int pad=10;

void usage(char *prog) {
  fprintf(stderr,"usage: %s [-v] [-i <count>] [-d <pixels>] [-x|-y <pixels>]\n", 
   prog);
  exit(-1);
}

void make_bar(cairo_t *cr, int n, int i) {
  double height = y-pad*2.0;
  //double d = i
  cairo_move_to(cr, (n+0)*bar_width+pad, y-pad);     // bl vertex
  cairo_line_to(cr, (n+1)*bar_width+pad, y-pad); // br vertex
  cairo_line_to(cr, (n+1)*bar_width+pad, y-height-pad); // tr vertex
  cairo_line_to(cr, (n+0)*bar_width+pad, y-height-pad); // tl vertex
  cairo_close_path(cr);
  double r = (n & 1) ? 1.0 : 0;
  double g = (n / 20.0);
  double b = (n & 1) ? 0 : 1.0;
  cairo_set_source_rgb (cr, r, g, b);
  cairo_fill(cr);
}

int make_image(int i) {
  cairo_surface_t *surface = cairo_image_surface_create (CAIRO_FORMAT_ARGB32, x, y);
  cairo_t *cr = cairo_create (surface);

  /* paint background */
  double r=0.1, g=0.0, b=0.2;
  cairo_set_source_rgb (cr, r, g, b);
  cairo_paint (cr);

  /* make border */
  cairo_set_line_width (cr, 10);
  cairo_set_source_rgb (cr, 0.9, 0.9, 0);
  cairo_rectangle (cr, 0, 0, x, y);
  cairo_stroke (cr);

  int n=0;
  while (n < 20) make_bar(cr,n++,i);

  /* write file */
#define OUTFILE "t.png"
  cairo_destroy (cr);
  cairo_surface_write_to_png (surface, OUTFILE);
  fprintf(stderr,"wrote " OUTFILE "\n");
  cairo_surface_destroy (surface);
  i++;
}

int main(int argc, char *argv[]) {
  int opt,i=0;

  while ( (opt = getopt(argc, argv, "v+i:d:x:y:")) != -1) {
    switch (opt) {
      case 'v': verbose++; break;
      case 'i': count=atoi(optarg); break;
      case 'd': delta=atoi(optarg); break;
      case 'x': x=atoi(optarg); break;
      case 'y': y=atoi(optarg); break;
      default: usage(argv[0]); break;
    }
  }

  while(i < count) { make_image(i); i++; }
  return 0;
}

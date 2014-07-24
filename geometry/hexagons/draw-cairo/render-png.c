/* take on stdin the tpl image with layout information
   render png */

#include <cairo/cairo.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <math.h> 
#include "utarray.h"
#include "tpl.h"

typedef struct {
  int x_min, x_max;
  int y_min, y_max;
} bbox_t;

struct {
  char *outfile;
  char *infile;
  int verbose;
  UT_array *hexv;
  double aspect_ratio;
  int output_pixels_x;
  int output_pixels_y;
  bbox_t bbox;
} cfg = {
  .aspect_ratio = 16.0/9,
  .output_pixels_x = 2560,
  .output_pixels_y = 1440,
};

static const double sq3 = 1.73205080756887729352;

void usage(char *exe) {
  fprintf(stderr,"usage: %s [-v] -i <in.tpl> -o <file.png>\n", exe);
  exit(-1);
}

typedef struct {
  char *id;
  int x;
  int y;
} hex_t;

/* plumbing for utarray of hex_t */ 
void hex_fin(hex_t *h) { if (h->id) free(h->id); }
void hex_cpy(hex_t *dst,const hex_t *src) {
  dst->x = src->x;
  dst->y = src->y;
  dst->id = src->id ? strdup(src->id) : NULL;
}
const UT_icd hex_icd = {
  .sz = sizeof(hex_t), 
 .copy=(ctor_f*)hex_cpy, 
 .dtor=(dtor_f*)hex_fin
};
/* end plumbing */

#define PADDING 10
void find_bounds() {
  hex_t *h=NULL;
  int i=0,x_min,x_max,y_min,y_max;
  while ( (h=(hex_t*)utarray_next(cfg.hexv,h))) {
    if ((i==0) || (h->x < x_min)) x_min=h->x;
    if ((i==0) || (h->x > x_max)) x_max=h->x;
    if ((i==0) || (h->y < y_min)) y_min=h->y;
    if ((i==0) || (h->y > y_max)) y_max=h->y;
    i++;
  }
  x_min -= PADDING; x_max += PADDING;
  y_min -= PADDING; y_max += PADDING;

  cfg.bbox.x_min = x_min; cfg.bbox.x_max = x_max;
  cfg.bbox.y_min = y_min; cfg.bbox.y_max = y_max;
}

/* get coordinate of initial vertex. we're in unit hexagon
   coordinates with no scaling factor */
#define odd(i) (i&1)
void get_hexagon_origin(hex_t *h, double *dx, double *dy) {
  int x = h->x; 
  int y = h->y; 
  *dx = sq3*x + (odd(y) ? (sq3/2) : 0);
  *dy = (3.0/2)*y;
}

void draw_hexagon(cairo_t *cr, hex_t *h) {
  double dx, dy;
  get_hexagon_origin(h,&dx,&dy);
  cairo_translate(cr, dx, dy);

  /* this is a hack that puts the hexagon's sequence number in */
  static int num=0; char num_str[20];
  fprintf(stderr,"drawing hexagon #%d@(%d,%d) at (%f,%f)\n",num,h->x,h->y,dx,dy);
  cairo_move_to(cr, 0.5, 0.5);
  sprintf(num_str,"%d",num++);
  cairo_show_text(cr, num_str);
  cairo_stroke(cr);

  /* draw the hexagon proper */
  cairo_move_to(cr,     0,      0);
  cairo_line_to(cr, sq3/2, -1/2.0);
  cairo_line_to(cr,   sq3,      0);
  cairo_line_to(cr,   sq3,      1);
  cairo_line_to(cr, sq3/2,  3/2.0);
  cairo_line_to(cr,     0,      1);
  cairo_close_path(cr);
  cairo_stroke(cr);

  cairo_translate(cr, -dx, -dy);
}

/* setup user coordinates so that we can draw unit hexagons
   and so that our bounding box has been centered inside the
   16:9 (or whatever) device surface. Math is hard.
 */
void setup_transform(cairo_t *cr) {
  /* width and height of our bbox in user distance */
  double xw = cfg.bbox.x_max - cfg.bbox.x_min; 
  double yw = cfg.bbox.y_max - cfg.bbox.y_min;
  /* scale factors before proportion preservation */
  double sx = cfg.output_pixels_x *1.0 / xw;
  double sy = cfg.output_pixels_y *1.0 / yw;
  /* choose the least scale factor, set both to it */
  if (sx < sy) sy = sx;
  else         sx = sy;
  /* further reduce the scale slightly to pad it */
  sx *= 0.9;
  sy *= 0.9;
  /* TODO translate so that scaled region is centered */
  cairo_scale(cr, sx, sy);
  /* start user space drawing so top left is visible */
  cairo_translate(cr, -1.0*cfg.bbox.x_min, -1.0*cfg.bbox.y_min);
}

void draw_image() {
  cairo_surface_t *surface = cairo_image_surface_create (CAIRO_FORMAT_ARGB32, 
                                  cfg.output_pixels_x, cfg.output_pixels_y);
  cairo_t *cr = cairo_create (surface);

  cairo_set_line_width(cr,0.1);
  cairo_set_source_rgb(cr, 0, 0, 1.0);
  cairo_set_font_size(cr, 0.5);
  setup_transform(cr); 

  hex_t *h=NULL;
  while ( (h=(hex_t*)utarray_next(cfg.hexv,h))) draw_hexagon(cr,h);

  cairo_destroy (cr);
  cairo_surface_write_to_png (surface, cfg.outfile);
  cairo_surface_destroy (surface);
}


int main(int argc, char *argv[]) {
  char *prog = argv[0];
  int opt, rc=-1;
  hex_t h;

  utarray_new(cfg.hexv, &hex_icd);

  while ( (opt = getopt(argc, argv, "v+i:o:h")) != -1) {
    switch (opt) {
      case 'v': cfg.verbose++; break;
      case 'i': cfg.infile = strdup(optarg); break;
      case 'o': cfg.outfile = strdup(optarg); break;
      case 'h': default: usage(prog); break;
    }
  }

  if (!cfg.outfile || !cfg.infile) usage(prog);

  /* read input file */
  tpl_node *tn = tpl_map("A(sii)", &h.id, &h.x, &h.y);
  if (tpl_load(tn, TPL_FILE, cfg.infile)) goto done;
  while(tpl_unpack(tn,1) > 0) utarray_push_back(cfg.hexv, &h);
  tpl_free(tn);

  find_bounds();
  draw_image();
  rc = 0;

 done:
  utarray_free(cfg.hexv);
  return rc;

}

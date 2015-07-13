#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include "nbt.h"
#include "tpl.h"

extern int verbose;

/* the .schematic form is a file in NBT format with these tags
 * (which are shown in a "fully qualified" form representing its 
 * parent tag sequence, the actual tag name is the last element).
 
  Schematic
  Schematic.Height
  Schematic.Length
  Schematic.Width
  Schematic.Entities
  Schematic.TileEntities
  Schematic.TileTicks
  Schematic.Materials
  Schematic.Data
  Schematic.Biomes
  Schematic.Blocks
*/

struct nbt_record *find_record(UT_vector *records, char *name) {
  struct nbt_record *r=NULL;
  while ( (r = (struct nbt_record*)utvector_next(records,r))) {
      if (!strcmp( name, utstring_body(&r->fqname))) return r;
  }

  return NULL;
}

#define BLOCK_AIR 0x00

int schem_to_tpl(char *buf, size_t len, UT_vector *records, char *outfile) {
  struct nbt_record *h, *l, *w, *b;
  char *eob = buf + len, *blocks;
  int rc = -1;
  size_t i;

  h = find_record(records, "Schematic.Height");
  l = find_record(records, "Schematic.Length");
  w = find_record(records, "Schematic.Width");
  b = find_record(records, "Schematic.Blocks");

  if ((!h) || (!l) || (!w) || (!b)) goto done;

  if (buf + h->pos + sizeof(uint16_t) > eob) goto done;
  if (buf + l->pos + sizeof(uint16_t) > eob) goto done;
  if (buf + w->pos + sizeof(uint16_t) > eob) goto done;
  if (buf + b->pos + b->count > eob) goto done;

  uint16_t height, length, width, x, y, z;
  memcpy(&height, buf + h->pos, sizeof(uint16_t)); height = ntohs(height);
  memcpy(&length, buf + l->pos, sizeof(uint16_t)); length = ntohs(length);
  memcpy(&width,  buf + w->pos, sizeof(uint16_t)); width  = ntohs(width );
  if (!height || !length || !width) goto done;
  blocks = buf + b->pos;

  tpl_node *tn = tpl_map("jjjA(jjj)", &height, &length, &width, &x, &y, &z);
  tpl_pack(tn,0);

  /* iterate over the blocks, keep the ones except air. compute its xyz */
  for(i=0; i < b->count; i++) {
    //if (blocks[i] == BLOCK_AIR) continue; 
    x= (i % (width * length)) % width; 
    y= (i % (width * length)) / width; 
    z=  i / (width * length);
    tpl_pack(tn,1);
  }

  tpl_dump(tn, TPL_FILE, outfile);
  tpl_free(tn);
  
  rc = 0;

 done:
  return rc;
}

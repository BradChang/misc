#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include "nbt.h"

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

int schem_to_tpl(UT_vector *records, char *outfile) {
  struct nbt_record *h, *l, *w, *b;
  int rc = -1;

  h = find_record(records, "Schematic.Height");
  l = find_record(records, "Schematic.Length");
  w = find_record(records, "Schematic.Width");
  b = find_record(records, "Schematic.Blocks");

  if ((!h) || (!l) || (!w) || (!b)) goto done;
  
  rc = 0;

 done:
  return rc;
}

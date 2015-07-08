#include <stdio.h>
#include <assert.h>
#include <stdlib.h>
#include <inttypes.h>
#include "tpl.h"

/* make a tpl image from the unzipped schematic input buffer */
int make_schem_tpl(char *in, size_t ilen, char **out, size_t *olen) {
  int rc = -1;
  tpl_node *tn=NULL;

  int16_t width, height, length, x, y, z;
  char id;

  tn = tpl_map("jjj" /* width height length */
               "A(jjjc)", /* x y z block-id */
               &width, &height, &length,
               &x, &y, &z, &id);
  if (tn==NULL) goto done;

  rc = tpl_dump(tn, TPL_MEM, out, olen);
  if (rc < 0) goto done;

  rc = 0;

 done:
  if (tn) tpl_free(tn);
  return rc;
}



#include "eccode.h"

/* call before encoding or decoding to determine the necessary
 * output buffer size to perform the encoding or decoding operation. */
size_t ecc_compute_olen( int mode, size_t ilen, size_t *ibits, size_t *obits) {

  // 4->7. Every byte becomes 14 bits.
  if (mode == MODE_ENCODE) { 
      *ibits = ilen * 8;
      *obits = ilen * 14;
  }

  // 7->4. Every 7 bits becomes 4 bits.
  if (mode == MODE_DECODE) { 
      *ibits = (ilen*8) - ((ilen*8) % 7);
      *obits = (*ibits/7) * 4;
  }

  size_t olen = (*obits/8) + ((*obits % 8) ? 1 : 0);
  return olen;
}


int ecc_recode(int mode, unsigned char *ib, size_t ilen, unsigned char *ob) {
  int rc=-1;
  return rc;
}


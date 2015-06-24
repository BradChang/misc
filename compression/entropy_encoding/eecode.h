
#ifndef _ECCODE_H_
#define _ECCODE_H_

#include <stddef.h>

typedef struct {
  size_t count[256];       /* [n]=count of byte n */
  size_t total;            /* count of all bytes */
  char code_length[256];   /* [n]=#bits to encode symbol n [Shannon's m] */

  unsigned char rank[256]; /* [n]=rank of byte n by frequency (0=highest) */
  unsigned char irank[256];/* [n]=byte whose rank is n (inverse of rank) */
  size_t Pcount[256];      /* [n]=count of bytes more probable than byte n */
} symbol_stats;

/* standard bit vector macros */
#define BIT_TEST(c,i)  (c[(i)/8] &   (1 << ((i) % 8)))
#define BIT_SET(c,i)   (c[(i)/8] |=  (1 << ((i) % 8)))
#define BIT_CLEAR(c,i) (c[(i)/8] &= ~(1 << ((i) % 8)))

/* while the first two are mutually exclusive we use
 * bit flags to support OR'ing additional options */
#define MODE_ENCODE     (1U << 0)
#define MODE_DECODE     (1U << 1)

int ecc_recode(int mode, unsigned char *ib, size_t ilen, unsigned char *ob, symbol_stats *s);
size_t ecc_compute_olen(int mode, unsigned char *ib, size_t ilen, size_t *ibits, size_t *obits, symbol_stats *s);

#endif /* _ECCODE_H_ */

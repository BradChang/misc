
#ifndef _ECCODE_H_
#define _ECCODE_H_

#include <stddef.h>

/* standard bit vector macros */
#define BIT_TEST(c,i)  (c[(i)/8] &   (1 << ((i) % 8)))
#define BIT_SET(c,i)   (c[(i)/8] |=  (1 << ((i) % 8)))
#define BIT_CLEAR(c,i) (c[(i)/8] &= ~(1 << ((i) % 8)))

/* modes below are for standard encoding (-e,-d,-n,-u) */
#define MODE_ENCODE     0  /* 4->7 encoding */
#define MODE_DECODE     1  /* 7->4 decoding */
#define MODE_NOISE      2  /* add correctable noise (1/7 bits) */
#define MODE_NOISE_UC   3  /* add uncorrectable noise (2/7 bits) */

/* modes below are for extended encoding (-E,-D,-N,-U) */
#define MODE_XENCODE   4  /* extended encoding 4->7 + parity */
#define MODE_XDECODE   5  /* extended decoding 7->4 + parity */
#define MODE_XNOISE    6  /* add correctable noise (1/8 bits) */
#define MODE_XNOISE_UC 7  /* add uncorrectable noise (2/8 bits) */

int ecc_recode(int mode, unsigned char *ib, size_t ilen, unsigned char *ob);
size_t ecc_compute_olen(int mode, size_t ilen, size_t *ibits, size_t *obits);

#endif /* _ECCODE_H_ */

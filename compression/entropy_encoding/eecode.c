#include <math.h> /* log, ceil */
#include "eecode.h"

/* Code length m[i] is in [0,8] because 8 bits is maximum entropy.
 * The Shannon paper describes m[i] as the integer that satisfies:
 *   log2( 1/p(i) ) <= m(i) < 1 + log2( 1/p(i) )
 * In other words- if the left side isn't an integer, round it up.
 */
#define log2(a) (log(a)/log(2))
void find_code_lengths(symbol_stats *s) {
  int i;

  for(i=0; i < 256; i++) {
    if (s->count[i] == 0) continue;
    double ivp = s->total*1.0 / s->count[i]; /* 1/p(i) */
    double mlb = log2(ivp);                  /* m(i) lower bound */
    if (mlb == 0.0) mlb = 1.0;               /* case of one symbol */
    s->code_length[i] = ceil( mlb );         /* "round up" */
  }
}

void count_symbols(symbol_stats *s, unsigned char *ib, size_t ilen) {
  size_t i;
  for(i=0; i < ilen; i++) s->count[ ib[i] ]++;
  for(i=0; i < 256; i++) s->total += s->count[i];
}

/* call before encoding or decoding to determine the necessary
 * output buffer size to perform the (de-)encoding operation. */
size_t ecc_compute_olen( int mode, unsigned char *ib, size_t ilen, size_t *ibits, size_t *obits, symbol_stats *s) {
  size_t i;

  if (mode == MODE_ENCODE) {
    *ibits = ilen * 8;
    *obits = 0;
    count_symbols(s, ib, ilen);
    find_code_lengths(s);
    for(i=0; i < ilen; i++) *obits += s->code_length[ ib[i] ];
  }

  if (mode == MODE_DECODE) {
    /* consult dictionary and stored olen in ib */
    *ibits = ilen * 8;
    *obits = 0;
  }

  size_t olen = (*obits/8) + ((*obits % 8) ? 1 : 0);
  return olen;
}

/* 
 * Entropy encoding is a method of producing a binary code
 * that gives shorter codes to common symbols and longer code
 * to more rare symbols. Symbols in our case are single bytes.
 * The idea is to transmit the information efficiently (with a
 * bits/symbol that approaches the entropy of the source).
 *
 * As described by Claude Shannon in "A Mathematical Theory of Communication",
 * Bell System Technical Journal, July 1948. Section 9
 *
 * 
 */ 

int ecc_recode(int mode, unsigned char *ib, size_t ilen, unsigned char *ob, symbol_stats *s) {
  int rc=-1;

  if (mode == MODE_ENCODE) {
    /* generate codes */
  }

  rc = 0;

 //done:
  return rc;
}


#define _GNU_SOURCE /* qsort_r */
#include <math.h>   /* log,ceil */
#include <stdlib.h> /* qsort_r */
#include <stdio.h>  /* fprintf */
#include <assert.h> 
#include "eecode.h"

static void count_symbols(symbol_stats *s, unsigned char *ib, size_t ilen) {
  size_t i;
  for(i=0; i < ilen; i++) s->count[ ib[i] ]++;
  s->nbytes = ilen;
}

/* Determine the code length in bits, for each byte value [0-255].
 * Code length m[i] is in [1,8] because 8 bits is maximum entropy.
 * The Shannon paper describes m[i] as the integer that satisfies:
 *   log2( 1/p(i) ) <= m(i) < 1 + log2( 1/p(i) )
 * In other words- if the left side isn't an integer, round it up.
 */
#define log2(a) (log(a)/log(2))
static void find_code_lengths(symbol_stats *s) {
  int i;

  for(i=0; i < 256; i++) {
    if (s->count[i] == 0) continue;

    double lb = log(s->nbytes);
    double lc = log(s->count[i]);
    double dv = lb - lc;
    double m = dv/log(2);

    if (m == 0.0) m = 1.0;            /* only one symbol */
    s->code_length[i] = ceil(m);      /* "round up" */
    assert(s->code_length[i] <= sizeof(unsigned)*8);
  }
}

static int sort_by_count_desc(const void *_a, const void *_b, void *_arg) {
  unsigned char a = *(unsigned char*)_a;
  unsigned char b = *(unsigned char*)_b;
  symbol_stats *s = (symbol_stats*)_arg;
  int rc;

  if (s->count[a] < s->count[b])  rc =  1;
  if (s->count[a] > s->count[b])  rc = -1;
  if (s->count[a] == s->count[b]) rc =  0;

  return rc;
}

/* the binary code for a symbol i is constructed as 
 * the binary expansion (bits after the binary point)
 * of the cumulative probability of the symbols more 
 * probable than itself. Shannon calls this P(i) (p.402)
 * taken only to the m(i) digit.
 */
static void generate_codes(symbol_stats *s) {
  unsigned int c;
  unsigned int i,j;

  /* sort bytes (desc) by their probabilities */
  for(i=0; i < 256; i++) s->irank[i] = i;  /* [0,255] unsorted */
  qsort_r(s->irank,256,sizeof(*s->irank),sort_by_count_desc,s);
  for(i=0; i < 256; i++) s->rank[ s->irank[i] ] = i;

  /* get P(i) for each byte (well, just its numerator Pcount[i]) */
  for(i=0; i < 256; i++) {
    for(j=0; j < s->rank[i]; j++) { /* for ranks higher than i's */
      s->Pcount[i] += s->count[ s->irank[j] ];
    }
  }

  /* Ready to make the binary code: expand P(i) to m(i) places. 
   * Iterate over m(i) bits (from msb to lsb) in code[i].
   * Set each bit if code[i] remains <= P(i), else clear it.
   */
  for(i=0; i < 256; i++) {
    size_t Pn = s->Pcount[i] << s->code_length[i];
    size_t Pd = s->nbytes;

    j = s->code_length[i];
    while (j--) {
      c = s->code[i] | (1U << j);
      if (c <= (Pn/Pd)) s->code[i] = c;
    }
  }
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
    generate_codes(s);
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

void dump_symbol_stats(symbol_stats *s) {
  unsigned i,j;
  fprintf(stderr,"byte c count rank code-len bitcode\n");
  fprintf(stderr,"---- - ----- ---- -------- ----------\n");
  for(i=0; i < 256; i++) {
    if (s->count[i] == 0) continue;
    fprintf(stderr,"0x%02x %c %5ld %3d %8d ", i, 
    (i>=' ' && i <= '~') ? i : ' ',
    (long)s->count[i],
    s->rank[i],
    s->code_length[i]);

    j = s->code_length[i];
    while (j--) fprintf(stderr,"%c",(s->code[i] & (1U << j)) ? '1':'0');
    fprintf(stderr,"\n");
  }
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
  }

  rc = 0;

 //done:
  return rc;
}


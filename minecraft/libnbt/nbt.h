#ifndef _NBT_H
#define _NBT_H

#include <inttypes.h>
#include "utvector.h"

/* result of parsing. it is one contiguous memory buffer the caller can free */
struct nbt {
  char *tagc;           /* count of tags */
  char *typev;          /* type of tag[i]*/
  char **tagv;          /* "fully qualified" name of each tag */
  off_t *tagp;          /* offset of tag payload (in buffer) */
  uint32_t *countv;     /* item count in tagp; varies by tag  */
  unsigned char data[]; /* c99 flexible array member - names */
};

struct nbt_tag {
  char type;
  char *name;
  uint16_t len;
};

#define TAGS                                                                  \
 x( TAG_End,        0, 0 )                                                    \
 x( TAG_Byte,       1, sizeof(int8_t)  )                                      \
 x( TAG_Short,      2, sizeof(int16_t) ) /* 16-bit, signed, big endian */     \
 x( TAG_Int,        3, sizeof(int32_t) ) /* 32-bit, signed, big endian */     \
 x( TAG_Long,       4, sizeof(int64_t) ) /* 64-bit signed, big endian */      \
 x( TAG_Float,      5, sizeof(int32_t) ) /* 32-bit, big endian, IEEE 754 */   \
 x( TAG_Double,     6, sizeof(int64_t) ) /* 64-bit, big endian, IEEE 754 */   \
 x( TAG_Byte_Array, 7, 0 )               /* TAG_Int length-prefixed bytes */  \
 x( TAG_String,     8, 0 )               /* TAG_Short length-prefixed UTF8 */ \
 x( TAG_List,       9, 0 )               /* tag type, TAG_Int length, data */ \
 x( TAG_Compound,  10, 0 )               /* list of named tags unti TAG_End */

#define TAG_MAX TAG_Compound

#define x(t,i,s) t=i,
enum { TAGS } nbt_tag;
#undef x
#define x(t,i,s) #t,
static const char *nbt_tag_str[] = { TAGS NULL };
#undef x
#define x(t,i,s) [i]=s,
static size_t nbt_tag_sizes[] = { TAGS };
#undef x

/* API */
int ungz(char *in, size_t ilen, char **out, size_t *olen);
int parse_nbt(char *in, size_t ilen, int verbose);

#endif /* _NBT_H_ */

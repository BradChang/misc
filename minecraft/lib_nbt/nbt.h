#ifndef _NBT_H
#define _NBT_H

#include <inttypes.h>
#include "utvector.h"

struct tag {
  /* info about the name */
  char type;
  char *name;
  uint16_t len;

  /* info about the payload:
   * TAG_String: data is the start of UTF8, count is the number of bytes.
   * TAG_List/TAG_Byte_Array: data is payload, count is the number count.
   * TAG_Compound: data is payload, count is 0, it is not known a priori.
   * Other types: count is 1. If you parse, note NBT data are big-endian.
   */
  char *data;
  uint32_t count;  
};

typedef struct {  /* to parse TAG_List or TAG_Compound */
  struct tag tag;
  struct {        /* for TAG_List, sub-items have this tag type and count */
    char type;
    uint32_t left;
    uint32_t total;
  } list;
} nbt_stack_frame;
const UT_vector_mm nbt_stack_frame_mm = {.sz=sizeof(nbt_stack_frame)};

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
enum { TAGS } tag;
#undef x
#define x(t,i,s) #t,
static const char *tag_str[] = { TAGS NULL };
#undef x
#define x(t,i,s) [i]=s,
static size_t tag_sizes[] = { TAGS };
#undef x

#endif /* _NBT_H_ */

#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <stdlib.h>
#include <inttypes.h>
#include <arpa/inet.h>
#include "utvector.h"
#include "tpl.h"

extern int verbose;

typedef struct {
  char tag;  /* TAG_Compound or TAG_List */
  char *tag_name; /* points into buffer */
  uint16_t name_len;
  struct {  /* for TAG_List, items are: */
    char tag;
    uint32_t left;
    uint32_t total;
  } list;
} nbt_parse;
const UT_vector_mm nbt_parse_mm = {.sz=sizeof(nbt_parse)};

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

#define x(t,i,s) t=i,
enum { TAGS } tag;
#undef x
#define x(t,i,s) #t,
const char *tag_str[] = { TAGS NULL };
#undef x
#define x(t,i,s) [i]=s,
size_t tag_sizes[] = { TAGS };
#undef x

#define TAG_MAX TAG_Compound

/* this function is rather special. it consults the stack. if we're
 * parsing a list, we are not expecting a named tag, so it returns 0.
 * in that case, it also sets tag_type to the list's per-element type,
 * and decrements the list count. if we're not in a list, return 1.
 */
int expect_named_tag(UT_vector *nbt_stack, char *tag_type) {
  nbt_parse *top;

  top = (nbt_parse*)utvector_tail(nbt_stack);
  if (top == NULL) return 1;
  if (top->tag == TAG_Compound) return 1;
  assert(top->tag == TAG_List);
  if (top->list.left == 0) {
    utvector_pop(nbt_stack);
    return 1;
  }

  /* in a list with remaining elements */
  *tag_type = top->list.tag;
  top->list.left--;

  return 0;
}

void dump(char tag, char *tag_name, uint16_t name_len, UT_vector *nbt_stack) {
  nbt_parse *top;
  uint32_t seen;
  int indent;

  indent = utvector_len(nbt_stack);
  while(indent--) fprintf(stderr," ");

  /* print list elements by their position, others by their name */
  top = (nbt_parse*)utvector_tail(nbt_stack);
  if (top && (top->tag == TAG_List)) { 
    seen = top->list.total - top->list.left;
    fprintf(stderr,"%u/%u (%s)\n", seen, top->list.total, tag_str[top->list.tag]);
  } else {
    fprintf(stderr,"%.*s (%s)\n", (int)name_len, tag_name, tag_str[tag]);
  }
}

/* here is a sample of the start of a .schematic file 
*  see Minecraft wiki for NBT file format and Schematic file format 
*
* NBT (named binary tag) files have the structure
* byte tagType
* TAG_String name
* [payload]
*
* A .schematic is a particular NBT formatted file containing
*
* TAG_Compound Schematic
*  TAG_Short Width
*  TAG_Short Height
*  TAG_Short Length
*  TAG_String Materials
*  TAG_Byte_Array Blocks (block ID's in 
*  TAG_Byte_Array Data 
*  TAG_List Entities
*  TAG_List TileEntities
*
00000000  0a 00 09 53 63 68 65 6d  61 74 69 63 02 00 06 48  |...Schematic...H|
00000010  65 69 67 68 74 00 1d 02  00 06 4c 65 6e 67 74 68  |eight.....Length|
00000020  00 3f 02 00 05 57 69 64  74 68 00 39 09 00 08 45  |.?...Width.9...E|
00000030  6e 74 69 74 69 65 73 0a  00 00 00 12 09 00 0b 44  |ntities........D|
00000040  72 6f 70 43 68 61 6e 63  65 73 05 00 00 00 05 3d  |ropChances.....=|
00000050  ae 14 7b 3d ae 14 7b 3d  ae 14 7b 3d ae 14 7b 3d  |..{=..{=..{=..{=|
00000060  ae 14 7b 04 00 09 55 55  49 44 4c 65 61 73 74 ad  |..{...UUIDLeast.|
00000070  43 52 6d a5 73 11 da 09  00 0a 41 74 74 72 69 62  |CRm.s.....Attrib|
00000080  75 74 65 73 0a 00 00 00  04 08 00 04 4e 61 6d 65  |utes........Name|
00000090  00 11 67 65 6e 65 72 69  63 2e 6d 61 78 48 65 61  |..generic.maxHea|
*/

/* make a tpl image from the unzipped schematic input buffer */
int make_schem_tpl(char *in, size_t ilen, char **out, size_t *olen) {
  int rc = -1;
  tpl_node *tn=NULL;
  char *p = in;
  size_t len = ilen, tag_size;
  UT_vector *nbt_stack;
  char tag_type, *tag_name, *tag_payload, list_type;
  uint16_t name_len, str_len;
  int32_t array_len;
  nbt_parse np, *top;

  nbt_stack = utvector_new(&nbt_parse_mm);

  int16_t width, height, length, x, y, z, l;
  char id;

  tn = tpl_map("jjj" /* width height length */
               "A(jjjc)", /* x y z block-id */
               &width, &height, &length,
               &x, &y, &z, &id);
  if (tn==NULL) goto done;

  /* parse the .schematic NBT. we only want dimensions and block ids */
  while(len > 0) {
  

    if (expect_named_tag(nbt_stack, &tag_type)) {

      tag_type = *p;
      len--; p++;

      if (tag_type == TAG_End) { /* special case */
        top = (nbt_parse*)utvector_pop(nbt_stack);
        if (top == NULL) goto done;
        continue;
      }

      /* get length-prefixed name */
      if (len < sizeof(uint16_t)) goto done;
      memcpy(&name_len, p, sizeof(uint16_t)); name_len = ntohs(name_len);
      len -= sizeof(uint16_t); p += sizeof(uint16_t);
      if (len < name_len) goto done;
      tag_name = p;
      len -= name_len; p += name_len;
      tag_payload = p;
    }

    if (verbose) dump(tag_type, tag_name, name_len, nbt_stack);

    switch(tag_type) {
      case TAG_Byte:
      case TAG_Short:
      case TAG_Int:
      case TAG_Long:
      case TAG_Float:
      case TAG_Double:
        tag_size = tag_sizes[tag_type]; assert(tag_size > 0);
        if (len < tag_size) goto done;
        len -= tag_size; p += tag_size;
        /* note if you were to parse the datum - endian swap needed */
        break;
      case TAG_Byte_Array:
        if (len < sizeof(array_len)) goto done;
        memcpy(&array_len, p, sizeof(array_len)); array_len = ntohl(array_len);
        len -= sizeof(array_len); p += sizeof(array_len);
        tag_size = array_len;
        if (len < tag_size) goto done;
        len -= tag_size; p += tag_size;
        break;
      case TAG_String:
        if (len < sizeof(str_len)) goto done;
        memcpy(&str_len, p, sizeof(str_len)); str_len = ntohs(str_len);
        len -= sizeof(str_len); p += sizeof(str_len);
        tag_size = str_len;
        if (len < tag_size) goto done;
        len -= tag_size; p += tag_size;
        break;
      case TAG_List:
        if (len < sizeof(list_type)) goto done;
        list_type = *p;
        len -= sizeof(list_type); p += sizeof(list_type);
        if (len < sizeof(array_len)) goto done;
        memcpy(&array_len, p, sizeof(array_len)); array_len = ntohl(array_len);
        len -= sizeof(array_len); p += sizeof(array_len);
        np.tag = TAG_List;
        np.tag_name = tag_name;
        np.name_len = name_len;
        np.list.tag = list_type;
        np.list.left = array_len;
        np.list.total = array_len;
        utvector_push(nbt_stack, &np);
        break;
      case TAG_Compound:
        np.tag = TAG_Compound;
        np.tag_name = tag_name;
        np.name_len = name_len;
        utvector_push(nbt_stack, &np);
        break;
      case TAG_End:  /* handled above */
        assert(0); 
        break;
      default:       /* unknown tag */
        goto done; 
        break;
    }
  }

  if (utvector_len(nbt_stack) >0) goto done;

  rc = tpl_dump(tn, TPL_MEM, out, olen);
  if (rc < 0) goto done;

  rc = 0;

 done:
  if (rc < 0) fprintf(stderr,"nbt parse failed\n");
  if (tn) tpl_free(tn);
  utvector_free(nbt_stack);
  return rc;
}



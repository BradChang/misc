#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <stdlib.h>
#include <inttypes.h>
#include "utvector.h"
#include "tpl.h"

typedef struct {
  int tag;  /* TAG_Compound or TAG_List */
  struct {  /* for TAG_List, items are: */
    int tag;
    int count;
  } list;
} nbt_parse;
const UT_vector_mm nbt_parse_mm = {.sz=sizeof(nbt_parse)};

#define TAG_End        0 
#define TAG_Byte       1 
#define TAG_Short      2 /* 16-bit, signed, big endian */
#define TAG_Int        3 /* 32-bit, signed, big endian */
#define TAG_Long       4 /* 64-bit signed, big endian */
#define TAG_Float      5 /* 32-bit, big endian, IEEE 754-2008 binary32 */
#define TAG_Double     6 /* 64-bit, big endian, IEEE 754-2008 binary64 */
#define TAG_Byte_Array 7 /* length (TAG_Int)-prefixed byte array */
#define TAG_String     8 /* length (TAG_Short)-prefixed UTF8 */
#define TAG_List       9 /* TAG_Byte tagID, TAG_Int length, array */
#define TAG_Compound  10 /* list of named tags unti TAG_End */
#define TAG_MAX TAG_Compound

size_t tag_sizes[TAG_MAX+1] = { /* the payload sizes for the simple tags */
  [TAG_Byte] =  sizeof(int8_t),
  [TAG_Short] = sizeof(int16_t),
  [TAG_Int] =   sizeof(int32_t),
  [TAG_Long] =  sizeof(int64_t),
  [TAG_Float]=  sizeof(int32_t),
  [TAG_Double]= sizeof(int64_t),
};

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
  char tag_type, *tag_name, *tag_payload;
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
  while(len) {
    tag_type = *p;

    if (tag_type == TAG_End) { /* special case */
      top = (nbt_parse*)utvector_tail(nbt_stack);
      if (top->tag != TAG_Compound) goto done;
      utvector_pop(nbt_stack);
      len--;
      continue;
    }

    /* other tags all have a length-prefixed name */
    if (len < sizeof(uint16_t)) goto done;
    memcpy(&name_len, p, sizeof(uint16_t)); name_len = ntohs(name_len);
    len -= sizeof(uint16_t); p += sizeof(uint16_t);
    if (len < name_len) goto done;
    tag_name = p;
    len -= name_len; p += name_len;
    tag_payload = p;
    //notate(tag_type, tag_name, name_len, tag_payload, len);

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
        break;
      case TAG_Compound: 
        break;
      case TAG_End: 
        assert(0); 
        break;
      default: 
        goto done; 
        break;
    }
  }

  /* first we want TAG_Compound, with a payload of Tag_String "Schematic" */
  if (len < 1 + 2 + 9) goto done;
  if (*p != TAG_Compound) goto done; p++;
  memcpy(&l, p, sizeof(l)); l = ntohs(l);
  if (l != 9) goto done; p += sizeof(l);
  if (memcmp(p,"Schematic",9)) goto done;
  np.tag = TAG_Compound;
  utvector_push(nbt_stack, &np);

  rc = tpl_dump(tn, TPL_MEM, out, olen);
  if (rc < 0) goto done;

  rc = 0;

 done:
  if (tn) tpl_free(tn);
  utvector_free(nbt_stack);
  return rc;
}



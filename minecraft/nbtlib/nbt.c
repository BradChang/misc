#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <stdlib.h>
#include <inttypes.h>
#include <arpa/inet.h>
#include "nbt.h"

typedef struct {  /* to parse TAG_List or TAG_Compound */
  struct nbt_tag tag;
  struct {        /* for TAG_List, sub-items have this tag type and count */
    char type;
    uint32_t left;
    uint32_t total;
  } list;
} nbt_stack_frame;
const UT_vector_mm nbt_stack_frame_mm = {.sz=sizeof(nbt_stack_frame)};

/* 
*  see Minecraft wiki for NBT (Named Binary Tag) file format 
*/

/* this function is consults the stack. if we're parsing a list, we are not
 * expecting a named tag, so it returns 0.  in that case, it also sets tag_type
 * to the list's per-element type, and decrements the list count. if we're not
 * in a list, return 1.
 */
static int expect_named_tag(UT_vector *nbt_stack, char *tag_type) {
  nbt_stack_frame *top;

  top = (nbt_stack_frame*)utvector_tail(nbt_stack);
  if (top == NULL) return 1;
  if (top->tag.type == TAG_Compound) return 1;
  assert(top->tag.type == TAG_List);
  if (top->list.left == 0) {
    utvector_pop(nbt_stack);
    return 1;
  }

  /* in a list with remaining elements */
  *tag_type = top->list.type;
  top->list.left--;

  return 0;
}

static int is_list_item(UT_vector *nbt_stack) {
  nbt_stack_frame *top;
  top = (nbt_stack_frame*)utvector_tail(nbt_stack);
  if (top && (top->tag.type == TAG_List)) return 1;
  return 0;
}

static void dump(struct nbt_tag *tag, UT_vector *nbt_stack) {
  nbt_stack_frame *top;
  uint32_t seen;
  int indent;

  indent = utvector_len(nbt_stack);
  while(indent--) fprintf(stderr," ");

  /* print list elements by their position, others by their name */
  if (is_list_item(nbt_stack)) {
    top = (nbt_stack_frame*)utvector_tail(nbt_stack);
    seen = top->list.total - top->list.left;
    fprintf(stderr,"%u/%u (%s)\n", seen, top->list.total, nbt_tag_str[top->list.type]);
  } else {
    fprintf(stderr,"%.*s (%s)\n", (int)tag->len, tag->name, nbt_tag_str[tag->type]);
  }
}

static void record(struct nbt_tag *tag, char *pos, uint32_t count, UT_vector *nbt_stack) {
  if (is_list_item(nbt_stack)) return;
}


int parse_nbt(char *in, size_t ilen, int verbose) {
  char *p = in, list_type;
  size_t len = ilen, sz;
  uint16_t str_len;
  UT_vector *nbt_stack;
  nbt_stack_frame np, *top;
  int32_t alen;
  struct nbt_tag tag;
  int rc = -1;

  nbt_stack = utvector_new(&nbt_stack_frame_mm);

  while(len > 0) {

    if (expect_named_tag(nbt_stack, &tag.type)) {

      tag.type = *p;
      len--; p++;

      if (tag.type == TAG_End) { /* special case */
        top = (nbt_stack_frame*)utvector_pop(nbt_stack);
        if (top == NULL) goto done;
        continue;
      }

      /* get length-prefixed name */
      if (len < sizeof(uint16_t)) goto done;
      memcpy(&tag.len, p, sizeof(uint16_t)); tag.len = ntohs(tag.len);
      len -= sizeof(uint16_t); p += sizeof(uint16_t);
      if (len < tag.len) goto done;
      tag.name = p;
      len -= tag.len; p += tag.len;
    }

    if (verbose) dump(&tag, nbt_stack);

    switch(tag.type) {
      case TAG_Byte:
      case TAG_Short:
      case TAG_Int:
      case TAG_Long:
      case TAG_Float:
      case TAG_Double: /* note if you parse these - endian swap needed */
        sz = nbt_tag_sizes[tag.type]; assert(sz > 0);
        if (len < sz) goto done;
        len -= sz; p += sz;
        record(&tag,p,1,nbt_stack);
        break;
      case TAG_Byte_Array:
        if (len < sizeof(alen)) goto done;
        memcpy(&alen, p, sizeof(alen)); alen = ntohl(alen);
        len -= sizeof(alen); p += sizeof(alen);
        if (len < alen) goto done;
        record(&tag,p,alen,nbt_stack);
        len -= alen; p += alen;
        break;
      case TAG_String:
        if (len < sizeof(str_len)) goto done;
        memcpy(&str_len, p, sizeof(str_len)); str_len = ntohs(str_len);
        len -= sizeof(str_len); p += sizeof(str_len);
        if (len < str_len) goto done;
        record(&tag,p,str_len,nbt_stack);
        len -= str_len; p += str_len;
        break;
      case TAG_List:
        if (len < sizeof(list_type)) goto done;
        list_type = *p;
        len -= sizeof(list_type); p += sizeof(list_type);
        if (len < sizeof(alen)) goto done;
        memcpy(&alen, p, sizeof(alen)); alen = ntohl(alen);
        len -= sizeof(alen); p += sizeof(alen);
        record(&tag,p,alen,nbt_stack);
        np.tag = tag;
        np.list.type = list_type;
        np.list.left = alen;
        np.list.total = alen;
        utvector_push(nbt_stack, &np);
        break;
      case TAG_Compound:
        np.tag = tag;
        record(&tag,p,0,nbt_stack);
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
  rc = 0;

 done:
  if (rc < 0) fprintf(stderr,"nbt parse failed\n");
  utvector_free(nbt_stack);
  return rc;
}


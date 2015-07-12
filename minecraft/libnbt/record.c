#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <stdlib.h>
#include <inttypes.h>
#include <arpa/inet.h>
#include "nbt.h"
#include "record.h"

static void _nbt_record_init(void *_r, unsigned num) {
  struct nbt_record *r = (struct nbt_record *)_r;
  while(num--) { utstring_init(&r->fqname); r++; }
}
static void _nbt_record_fini(void *_r, unsigned num) {
  struct nbt_record *r = (struct nbt_record *)_r;
  while(num--) { utstring_done(&r->fqname); r++; }
}
static void _nbt_record_clear(void *_r, unsigned num) {
  struct nbt_record *r = (struct nbt_record *)_r;
  while(num--) { utstring_clear(&r->fqname); r++; }
}
static void _nbt_record_copy(void *_dst, void *_src, unsigned num) {
  struct nbt_record *dst = (struct nbt_record *)_dst;
  struct nbt_record *src = (struct nbt_record *)_src;
  while(num--) { 
    dst->tag = src->tag;
    dst->pos = src->pos;
    dst->count = src->count;
    utstring_concat(&dst->fqname, &src->fqname); 
    dst++; src++;
  }
}
const UT_vector_mm nbt_record_mm = {
  .sz=sizeof(struct nbt_record),
  .init=_nbt_record_init,
  .fini=_nbt_record_fini,
  .clear=_nbt_record_clear,
  .copy=_nbt_record_copy
};

void nbt_record_tag(struct nbt_tag *tag, off_t pos, uint32_t count, 
                  UT_vector /* of nbt_stack_frame */ *nbt_stack, 
                  UT_vector /* of struct nbt_record */ *records) {

  /* do not record each item of a list; we record the list itself */
  nbt_stack_frame *top;
  top = (nbt_stack_frame*)utvector_tail(nbt_stack);
  if (top && (top->tag.type == TAG_List)) return;

  /* record the tag. prepend stack tags to "fully-qualify" the name */
  struct nbt_record *r = (struct nbt_record*)utvector_extend(records);
  r->tag = *tag;
  r->pos = pos;
  r->count = count;
  nbt_stack_frame *f = NULL;
  while ( (f = (nbt_stack_frame*)utvector_next(nbt_stack,f))) {
    utstring_printf(&r->fqname, "%.*s.", (int)f->tag.len, f->tag.name);
  }
  utstring_printf(&r->fqname, "%.*s", (int)tag->len, tag->name);
  utvector_push(records, r);
}




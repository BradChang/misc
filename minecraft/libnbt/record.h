#ifndef _RECORD_H
#define _RECORD_H

#include "nbt.h"
#include "utvector.h"
#include "utstring.h"

/* internal bookkeeping for recording tags during parsing */

void nbt_record_tag(struct nbt_tag *tag, off_t pos, uint32_t count, 
                  UT_vector /* of nbt_stack_frame */ *nbt_stack, 
                  UT_vector /* of struct nbt_record */ *records);

extern const UT_vector_mm nbt_record_mm;


#endif /* _RECORD_H_ */

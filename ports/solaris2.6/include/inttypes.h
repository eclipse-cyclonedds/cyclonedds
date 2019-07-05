#ifndef DDSRT_FIXUP_INTTYPES_H
#define DDSRT_FIXUP_INTTYPES_H

#include_next "inttypes.h"
#define PRIuPTR "lu"
#define PRIxPTR "lx"

#endif /* DDSRT_FIXUP_INTTYPES_H */

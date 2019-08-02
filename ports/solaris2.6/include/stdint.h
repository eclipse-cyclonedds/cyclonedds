#ifndef DDSRT_FIXUP_STDINT_H
#define DDSRT_FIXUP_STDINT_H

#include <sys/int_types.h>
#include <limits.h>

#ifndef UINT32_C
#define UINT32_C(v)  (v ## U)
#endif

#endif /* DDSRT_FIXUP_STDINT_H */

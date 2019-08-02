#ifndef DDSRT_FIXUP_MATH_H
#define DDSRT_FIXUP_MATH_H

#include_next "math.h"

/* INFINITY, HUGE_VALF, HUGE_VALL are all standard C99, but Solaris 2.6
   antedates that by a good margin and GCC's fixed-up headers don't
   define it, so we do it here */
#undef HUGE_VAL
#ifdef __GNUC__
#  define INFINITY (__builtin_inff ())
#  define HUGE_VAL (__builtin_huge_val ())
#  define HUGE_VALF (__builtin_huge_valf ())
#  define HUGE_VALL (__builtin_huge_vall ())
#else
#  define INFINITY 1e10000
#  define HUGE_VAL 1e10000
#  define HUGE_VALF 1e10000f
#  define HUGE_VALL 1e10000L
#endif

#endif /* DDSRT_FIXUP_MATH_H */

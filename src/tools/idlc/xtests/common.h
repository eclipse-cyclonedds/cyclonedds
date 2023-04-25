// Copyright(c) 2021 to 2022 ZettaScale Technology and others
//
// This program and the accompanying materials are made available under the
// terms of the Eclipse Public License v. 2.0 which is available at
// http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
// v. 1.0 which is available at
// http://www.eclipse.org/org/documents/edl-v10.php.
//
// SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

#ifndef CMP_H
#define CMP_H

#include <stdlib.h>
#include <string.h>
#include "dds/ddsrt/heap.h"
#include "dds/ddsrt/string.h"

#define STR16 "abcdef0123456789"
#define STR128 STR16 STR16 STR16 STR16 STR16 STR16 STR16 STR16

#define A(s) { (s) = dds_alloc (sizeof (*(s))); }
#define SEQA(s,l) \
{ \
  (s)._length = (s)._maximum = l; \
  (s)._release = true; \
  (s)._buffer = dds_alloc (l * sizeof (*(s)._buffer)); \
}
#define STRA(s,str) \
{ \
  (s) = dds_alloc (strlen(str) + 1); \
  ddsrt_strlcpy ((s), (str), strlen((str)) + 1); \
}
#define STRCPY(s,str) \
{ \
  ddsrt_strlcpy ((s), (str), sizeof ((s))); \
}
#define EXTA(s,n) \
{ \
  (s) = dds_alloc (sizeof (*(s))); \
  *(s) = (n); \
}

#define CMP(a,b,f,n) { \
  if ((a->f) != (n)) return -2; \
  if ((a->f) != (b->f)) return (a->f) > (b->f) ? 1 : -1; \
}
#define CMPSTR(a,b,f,s) { \
  if (strcmp(a->f, (s))) return -2; \
  if (strcmp((a->f), (b->f))) return strcmp((a->f), (b->f)); \
}
#define CMPEXT(a,b,f,n) { \
  if ((a->f) && *(a->f) != (n)) return -2; \
  if ((!(a->f) && (b->f)) || ((a->f) && !(b->f))) return 2; \
  if (*(a->f) != *(b->f)) return *(a->f) > *(b->f) ? 1 : -1; \
}
#define CMPEXTF(a,b,f,f2,n) { \
  if ((a->f) && (*(a->f)).f2 != (n)) return -2; \
  if ((!(a->f) && (b->f)) || ((a->f) && !(b->f))) return 2; \
  if ((*(a->f)).f2 != (*(b->f)).f2) return (*(a->f)).f2 > (*(b->f)).f2 ? 1 : -1; \
}
#define CMPEXTEXTF(a,b,f,f2,n) { \
  if ((a->f) && (a->f)->f2 && *((a->f)->f2) != (n)) return -2; \
  if ((!(a->f) && (b->f)) || ((a->f) && !(b->f))) return 2; \
  if ((!((a->f)->f2) && ((b->f)->f2)) || (((a->f)->f2) && !((b->f)->f2))) return 3; \
  if (*((a->f)->f2) != *((b->f)->f2)) return *((a->f)->f2) > *((b->f)->f2) ? 1 : -1; \
}
#define CMPEXTA(a,b,f,i,n) { \
  if ((a->f) && (*(a->f))[i] != (n)) return -2; \
  if ((!(a->f) && (b->f)) || ((a->f) && !(b->f))) return 2; \
  if ((*(a->f))[i] != (*(b->f))[i]) return (*(a->f))[i] > (*(b->f))[i] ? 1 : -1; \
}
#define CMPEXTA2(a,b,f,i,i2,n) { \
  if ((a->f) && (*(a->f))[i][i2] != n) return -2; \
  if ((!(a->f) && (b->f)) || ((a->f) && !(b->f))) return 2; \
  if ((*(a->f))[i][i2] != (*(b->f))[i][i2]) return (*(a->f))[i][i2] > (*(b->f))[i][i2] ? 1 : -1; \
}
#define CMPEXTAF(a,b,f,i,f2,n) { \
  if ((a->f) && (*(a->f))[i].f2 != n) return -2; \
  if ((!(a->f) && (b->f)) || ((a->f) && !(b->f))) return 2; \
  if ((*(a->f))[i].f2 != (*(b->f))[i].f2) return (*(a->f))[i].f2 > (*(b->f))[i].f2 ? 1 : -1; \
}
#define CMPEXTSTR(a,b,f,s) { \
  if ((a->f) && strcmp(*(a->f), s)) return -2; \
  if ((!(a->f) && (b->f)) || ((a->f) && !(b->f))) return 2; \
  if (strcmp(*(a->f), *(b->f))) return strcmp(*(a->f), *(b->f)); \
}
#define CMPEXTASTR(a,b,f,i,s) { \
  if ((a->f) && strcmp((*(a->f))[(i)], s)) return -2; \
  if ((!(a->f) && (b->f)) || ((a->f) && !(b->f))) return 2; \
  if (strcmp((*(a->f))[(i)], (*(b->f))[(i)])) return strcmp((*(a->f))[(i)], (*(b->f))[(i)]); \
}


#define NO_KEY_CMP \
  int cmp_key (const void *sa, const void *sb) { (void) sa; (void) sb; abort (); }


void init_sample (void *s);
int cmp_sample (const void *sa, const void *sb);
int cmp_key (const void *sa, const void *sb);

#endif /* CMP_H */

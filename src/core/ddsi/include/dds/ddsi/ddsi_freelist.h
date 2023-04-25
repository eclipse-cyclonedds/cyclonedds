// Copyright(c) 2006 to 2020 ZettaScale Technology and others
//
// This program and the accompanying materials are made available under the
// terms of the Eclipse Public License v. 2.0 which is available at
// http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
// v. 1.0 which is available at
// http://www.eclipse.org/org/documents/edl-v10.php.
//
// SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

#ifndef DDSI_FREELIST_H
#define DDSI_FREELIST_H

#include "dds/ddsrt/atomics.h"
#include "dds/ddsrt/sync.h"

#if defined (__cplusplus)
extern "C" {
#endif

#define DDSI_FREELIST_NONE 1
#define DDSI_FREELIST_ATOMIC_LIFO 2
#define DDSI_FREELIST_DOUBLE 3

#define DDSI_FREELIST_TYPE DDSI_FREELIST_DOUBLE

#ifndef DDSI_FREELIST_TYPE
#if DDSRT_HAVE_ATOMIC_LIFO
#define DDSI_FREELIST_TYPE DDSI_FREELIST_ATOMIC_LIFO
#else
#define DDSI_FREELIST_TYPE DDSI_FREELIST_DOUBLE
#endif
#endif

#if DDSI_FREELIST_TYPE == DDSI_FREELIST_NONE

struct ddsi_freelist {
  char dummy;
};

#elif DDSI_FREELIST_TYPE == DDSI_FREELIST_ATOMIC_LIFO

struct ddsi_freelist {
  ddsrt_atomic_lifo_t x;
  ddsrt_atomic_uint32_t count;
  uint32_t max;
  size_t linkoff;
};

#elif DDSI_FREELIST_TYPE == DDSI_FREELIST_DOUBLE

#define NN_FREELIST_NPAR 4
#define NN_FREELIST_NPAR_LG2 2
#define NN_FREELIST_MAGSIZE 256

struct ddsi_freelist_m {
  void *x[NN_FREELIST_MAGSIZE];
  void *next;
};

struct ddsi_freelist1 {
  ddsrt_mutex_t lock;
  uint32_t count;
  struct ddsi_freelist_m *m;
};

struct ddsi_freelist {
  struct ddsi_freelist1 inner[NN_FREELIST_NPAR];
  ddsrt_atomic_uint32_t cc;
  ddsrt_mutex_t lock;
  struct ddsi_freelist_m *mlist;
  struct ddsi_freelist_m *emlist;
  uint32_t count;
  uint32_t max;
  size_t linkoff;
};

#endif

/** @component ddsi_freelist */
void ddsi_freelist_init (struct ddsi_freelist *fl, uint32_t max, size_t linkoff);

/** @component ddsi_freelist */
void ddsi_freelist_fini (struct ddsi_freelist *fl, void (*free) (void *elem));

/** @component ddsi_freelist */
bool ddsi_freelist_push (struct ddsi_freelist *fl, void *elem);

/** @component ddsi_freelist */
void *ddsi_freelist_pushmany (struct ddsi_freelist *fl, void *first, void *last, uint32_t n);

/** @component ddsi_freelist */
void *ddsi_freelist_pop (struct ddsi_freelist *fl);

#if defined (__cplusplus)
}
#endif

#endif /* DDSI_FREELIST_H */

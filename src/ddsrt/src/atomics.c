/*
 * Copyright(c) 2006 to 2018 ADLINK Technology Limited and others
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v. 2.0 which is available at
 * http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
 * v. 1.0 which is available at
 * http://www.eclipse.org/org/documents/edl-v10.php.
 *
 * SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause
 */
#include "dds/ddsrt/atomics.h"

/* LD, ST */
extern inline uint32_t ddsrt_atomic_ld32 (const volatile ddsrt_atomic_uint32_t *x);
#if DDSRT_HAVE_ATOMIC64
extern inline uint64_t ddsrt_atomic_ld64 (const volatile ddsrt_atomic_uint64_t *x);
#endif
extern inline uintptr_t ddsrt_atomic_ldptr (const volatile ddsrt_atomic_uintptr_t *x);
extern inline void *ddsrt_atomic_ldvoidp (const volatile ddsrt_atomic_voidp_t *x);
extern inline void ddsrt_atomic_st32 (volatile ddsrt_atomic_uint32_t *x, uint32_t v);
#if DDSRT_HAVE_ATOMIC64
extern inline void ddsrt_atomic_st64 (volatile ddsrt_atomic_uint64_t *x, uint64_t v);
#endif
extern inline void ddsrt_atomic_stptr (volatile ddsrt_atomic_uintptr_t *x, uintptr_t v);
extern inline void ddsrt_atomic_stvoidp (volatile ddsrt_atomic_voidp_t *x, void *v);
/* INC */
extern inline void ddsrt_atomic_inc32 (volatile ddsrt_atomic_uint32_t *x);
#if DDSRT_HAVE_ATOMIC64
extern inline void ddsrt_atomic_inc64 (volatile ddsrt_atomic_uint64_t *x);
#endif
extern inline void ddsrt_atomic_incptr (volatile ddsrt_atomic_uintptr_t *x);
extern inline uint32_t ddsrt_atomic_inc32_ov (volatile ddsrt_atomic_uint32_t *x);
extern inline uint32_t ddsrt_atomic_inc32_nv (volatile ddsrt_atomic_uint32_t *x);
#if DDSRT_HAVE_ATOMIC64
extern inline uint64_t ddsrt_atomic_inc64_nv (volatile ddsrt_atomic_uint64_t *x);
#endif
extern inline uintptr_t ddsrt_atomic_incptr_nv (volatile ddsrt_atomic_uintptr_t *x);
/* DEC */
extern inline void ddsrt_atomic_dec32 (volatile ddsrt_atomic_uint32_t *x);
#if DDSRT_HAVE_ATOMIC64
extern inline void ddsrt_atomic_dec64 (volatile ddsrt_atomic_uint64_t *x);
#endif
extern inline void ddsrt_atomic_decptr (volatile ddsrt_atomic_uintptr_t *x);
extern inline uint32_t ddsrt_atomic_dec32_nv (volatile ddsrt_atomic_uint32_t *x);
#if DDSRT_HAVE_ATOMIC64
extern inline uint64_t ddsrt_atomic_dec64_nv (volatile ddsrt_atomic_uint64_t *x);
#endif
extern inline uintptr_t ddsrt_atomic_decptr_nv (volatile ddsrt_atomic_uintptr_t *x);
extern inline uint32_t ddsrt_atomic_dec32_ov (volatile ddsrt_atomic_uint32_t *x);
#if DDSRT_HAVE_ATOMIC64
extern inline uint64_t ddsrt_atomic_dec64_ov (volatile ddsrt_atomic_uint64_t *x);
#endif
extern inline uintptr_t ddsrt_atomic_decptr_ov (volatile ddsrt_atomic_uintptr_t *x);
/* ADD */
extern inline void ddsrt_atomic_add32 (volatile ddsrt_atomic_uint32_t *x, uint32_t v);
#if DDSRT_HAVE_ATOMIC64
extern inline void ddsrt_atomic_add64 (volatile ddsrt_atomic_uint64_t *x, uint64_t v);
#endif
extern inline void ddsrt_atomic_addptr (volatile ddsrt_atomic_uintptr_t *x, uintptr_t v);
extern inline void ddsrt_atomic_addvoidp (volatile ddsrt_atomic_voidp_t *x, ptrdiff_t v);
extern inline uint32_t ddsrt_atomic_add32_ov (volatile ddsrt_atomic_uint32_t *x, uint32_t v);
extern inline uint32_t ddsrt_atomic_add32_nv (volatile ddsrt_atomic_uint32_t *x, uint32_t v);
#if DDSRT_HAVE_ATOMIC64
extern inline uint64_t ddsrt_atomic_add64_nv (volatile ddsrt_atomic_uint64_t *x, uint64_t v);
#endif
extern inline uintptr_t ddsrt_atomic_addptr_nv (volatile ddsrt_atomic_uintptr_t *x, uintptr_t v);
extern inline void *ddsrt_atomic_addvoidp_nv (volatile ddsrt_atomic_voidp_t *x, ptrdiff_t v);
/* SUB */
extern inline void ddsrt_atomic_sub32 (volatile ddsrt_atomic_uint32_t *x, uint32_t v);
#if DDSRT_HAVE_ATOMIC64
extern inline void ddsrt_atomic_sub64 (volatile ddsrt_atomic_uint64_t *x, uint64_t v);
#endif
extern inline void ddsrt_atomic_subptr (volatile ddsrt_atomic_uintptr_t *x, uintptr_t v);
extern inline void ddsrt_atomic_subvoidp (volatile ddsrt_atomic_voidp_t *x, ptrdiff_t v);
extern inline uint32_t ddsrt_atomic_sub32_ov (volatile ddsrt_atomic_uint32_t *x, uint32_t v);
extern inline uint32_t ddsrt_atomic_sub32_nv (volatile ddsrt_atomic_uint32_t *x, uint32_t v);
#if DDSRT_HAVE_ATOMIC64
extern inline uint64_t ddsrt_atomic_sub64_nv (volatile ddsrt_atomic_uint64_t *x, uint64_t v);
#endif
extern inline uintptr_t ddsrt_atomic_subptr_nv (volatile ddsrt_atomic_uintptr_t *x, uintptr_t v);
extern inline void *ddsrt_atomic_subvoidp_nv (volatile ddsrt_atomic_voidp_t *x, ptrdiff_t v);
/* AND */
extern inline void ddsrt_atomic_and32 (volatile ddsrt_atomic_uint32_t *x, uint32_t v);
#if DDSRT_HAVE_ATOMIC64
extern inline void ddsrt_atomic_and64 (volatile ddsrt_atomic_uint64_t *x, uint64_t v);
#endif
extern inline void ddsrt_atomic_andptr (volatile ddsrt_atomic_uintptr_t *x, uintptr_t v);
extern inline uint32_t ddsrt_atomic_and32_ov (volatile ddsrt_atomic_uint32_t *x, uint32_t v);
#if DDSRT_HAVE_ATOMIC64
extern inline uint64_t ddsrt_atomic_and64_ov (volatile ddsrt_atomic_uint64_t *x, uint64_t v);
#endif
extern inline uintptr_t ddsrt_atomic_andptr_ov (volatile ddsrt_atomic_uintptr_t *x, uintptr_t v);
extern inline uint32_t ddsrt_atomic_and32_nv (volatile ddsrt_atomic_uint32_t *x, uint32_t v);
#if DDSRT_HAVE_ATOMIC64
extern inline uint64_t ddsrt_atomic_and64_nv (volatile ddsrt_atomic_uint64_t *x, uint64_t v);
#endif
extern inline uintptr_t ddsrt_atomic_andptr_nv (volatile ddsrt_atomic_uintptr_t *x, uintptr_t v);
/* OR */
extern inline void ddsrt_atomic_or32 (volatile ddsrt_atomic_uint32_t *x, uint32_t v);
#if DDSRT_HAVE_ATOMIC64
extern inline void ddsrt_atomic_or64 (volatile ddsrt_atomic_uint64_t *x, uint64_t v);
#endif
extern inline void ddsrt_atomic_orptr (volatile ddsrt_atomic_uintptr_t *x, uintptr_t v);
extern inline uint32_t ddsrt_atomic_or32_ov (volatile ddsrt_atomic_uint32_t *x, uint32_t v);
#if DDSRT_HAVE_ATOMIC64
extern inline uint64_t ddsrt_atomic_or64_ov (volatile ddsrt_atomic_uint64_t *x, uint64_t v);
#endif
extern inline uintptr_t ddsrt_atomic_orptr_ov (volatile ddsrt_atomic_uintptr_t *x, uintptr_t v);
extern inline uint32_t ddsrt_atomic_or32_nv (volatile ddsrt_atomic_uint32_t *x, uint32_t v);
#if DDSRT_HAVE_ATOMIC64
extern inline uint64_t ddsrt_atomic_or64_nv (volatile ddsrt_atomic_uint64_t *x, uint64_t v);
#endif
extern inline uintptr_t ddsrt_atomic_orptr_nv (volatile ddsrt_atomic_uintptr_t *x, uintptr_t v);
/* CAS */
extern inline int ddsrt_atomic_cas32 (volatile ddsrt_atomic_uint32_t *x, uint32_t exp, uint32_t des);
#if DDSRT_HAVE_ATOMIC64
extern inline int ddsrt_atomic_cas64 (volatile ddsrt_atomic_uint64_t *x, uint64_t exp, uint64_t des);
#endif
extern inline int ddsrt_atomic_casptr (volatile ddsrt_atomic_uintptr_t *x, uintptr_t exp, uintptr_t des);
extern inline int ddsrt_atomic_casvoidp (volatile ddsrt_atomic_voidp_t *x, void *exp, void *des);
#if DDSRT_ATOMIC_LIFO_SUPPORT
extern inline int ddsrt_atomic_casvoidp2 (volatile ddsrt_atomic_uintptr2_t *x, uintptr_t a0, uintptr_t b0, uintptr_t a1, uintptr_t b1);
#endif
/* FENCES */
extern inline void ddsrt_atomic_fence (void);
extern inline void ddsrt_atomic_fence_ldld (void);
extern inline void ddsrt_atomic_fence_acq (void);
extern inline void ddsrt_atomic_fence_rel (void);

#if DDSRT_ATOMIC_LIFO_SUPPORT
void ddsrt_atomic_lifo_init (ddsrt_atomic_lifo_t *head)
{
  head->aba_head.s.a = head->aba_head.s.b = 0;
}
void ddsrt_atomic_lifo_push (ddsrt_atomic_lifo_t *head, void *elem, size_t linkoff)
{
  uintptr_t a0, b0;
  do {
    a0 = *((volatile uintptr_t *) &head->aba_head.s.a);
    b0 = *((volatile uintptr_t *) &head->aba_head.s.b);
    *((volatile uintptr_t *) ((char *) elem + linkoff)) = b0;
  } while (!ddsrt_atomic_casvoidp2 (&head->aba_head, a0, b0, a0+1, (uintptr_t)elem));
}
void *ddsrt_atomic_lifo_pop (ddsrt_atomic_lifo_t *head, size_t linkoff) {
  uintptr_t a0, b0, b1;
  do {
    a0 = *((volatile uintptr_t *) &head->aba_head.s.a);
    b0 = *((volatile uintptr_t *) &head->aba_head.s.b);
    if (b0 == 0) {
      return NULL;
    }
    b1 = (*((volatile uintptr_t *) ((char *) b0 + linkoff)));
  } while (!ddsrt_atomic_casvoidp2 (&head->aba_head, a0, b0, a0+1, b1));
  return (void *) b0;
}
void ddsrt_atomic_lifo_pushmany (ddsrt_atomic_lifo_t *head, void *first, void *last, size_t linkoff)
{
  uintptr_t a0, b0;
  do {
    a0 = *((volatile uintptr_t *) &head->aba_head.s.a);
    b0 = *((volatile uintptr_t *) &head->aba_head.s.b);
    *((volatile uintptr_t *) ((char *) last + linkoff)) = b0;
  } while (!ddsrt_atomic_casvoidp2 (&head->aba_head, a0, b0, a0+1, (uintptr_t)first));
}
#endif

#if DDSRT_HAVE_ATOMIC64
void ddsrt_atomics_init (void)
{
}

void ddsrt_atomics_fini (void)
{
}

#else

/* Emulation by hashing the variable's address to a small set of mutexes.  */
#include "dds/ddsrt/sync.h"

#define N_MUTEXES_LG2 4
#define N_MUTEXES     (1 << N_MUTEXES_LG2)
static ddsrt_mutex_t mutexes[N_MUTEXES];

void ddsrt_atomics_init (void)
{
  for (int i = 0; i < N_MUTEXES; i++)
    ddsrt_mutex_init (&mutexes[i]);
}

void ddsrt_atomics_fini (void)
{
  for (int i = 0; i < N_MUTEXES; i++)
    ddsrt_mutex_destroy (&mutexes[i]);
}

static uint32_t atomic64_lock_index (const volatile ddsrt_atomic_uint64_t *x)
{
  const uint32_t u = (uint16_t) ((uintptr_t) x >> 3);
  const uint32_t v = u * 0xb4817365;
  return v >> (32 - N_MUTEXES_LG2);
}

int ddsrt_atomic_cas64 (volatile ddsrt_atomic_uint64_t *x, uint64_t exp, uint64_t des)
{
  const uint32_t idx = atomic64_lock_index (x);
  ddsrt_mutex_lock (&mutexes[idx]);
  if (x->v == exp)
  {
    x->v = des;
    ddsrt_mutex_unlock (&mutexes[idx]);
    return true;
  }
  else
  {
    ddsrt_mutex_unlock (&mutexes[idx]);
    return false;
  }
}

uint64_t ddsrt_atomic_ld64(const volatile ddsrt_atomic_uint64_t *x)
{
  const uint32_t idx = atomic64_lock_index (x);
  ddsrt_mutex_lock (&mutexes[idx]);
  const uint64_t v = x->v;
  ddsrt_mutex_unlock (&mutexes[idx]);
  return v;
}

void ddsrt_atomic_st64(volatile ddsrt_atomic_uint64_t *x, uint64_t v)
{
  const uint32_t idx = atomic64_lock_index (x);
  ddsrt_mutex_lock (&mutexes[idx]);
  x->v = v;
  ddsrt_mutex_unlock (&mutexes[idx]);
}

void ddsrt_atomic_inc64 (volatile ddsrt_atomic_uint64_t *x)
{
  const uint32_t idx = atomic64_lock_index (x);
  ddsrt_mutex_lock (&mutexes[idx]);
  ++x->v;
  ddsrt_mutex_unlock (&mutexes[idx]);
}

uint64_t ddsrt_atomic_inc64_nv (volatile ddsrt_atomic_uint64_t *x)
{
  const uint32_t idx = atomic64_lock_index (x);
  ddsrt_mutex_lock (&mutexes[idx]);
  const uint64_t nv = ++x->v;
  ddsrt_mutex_unlock (&mutexes[idx]);
  return nv;
}

void ddsrt_atomic_dec64 (volatile ddsrt_atomic_uint64_t *x)
{
  const uint32_t idx = atomic64_lock_index (x);
  ddsrt_mutex_lock (&mutexes[idx]);
  --x->v;
  ddsrt_mutex_unlock (&mutexes[idx]);
}

uint64_t ddsrt_atomic_dec64_nv (volatile ddsrt_atomic_uint64_t *x)
{
  const uint32_t idx = atomic64_lock_index (x);
  ddsrt_mutex_lock (&mutexes[idx]);
  const uint64_t nv = --x->v;
  ddsrt_mutex_unlock (&mutexes[idx]);
  return nv;
}

void ddsrt_atomic_add64 (volatile ddsrt_atomic_uint64_t *x, uint64_t v)
{
  const uint32_t idx = atomic64_lock_index (x);
  ddsrt_mutex_lock (&mutexes[idx]);
  x->v += v;
  ddsrt_mutex_unlock (&mutexes[idx]);
}

uint64_t ddsrt_atomic_add64_nv (volatile ddsrt_atomic_uint64_t *x, uint64_t v)
{
  const uint32_t idx = atomic64_lock_index (x);
  ddsrt_mutex_lock (&mutexes[idx]);
  const uint64_t ov = x->v;
  const uint64_t nv = ov + v;
  x->v = nv;
  ddsrt_mutex_unlock (&mutexes[idx]);
  return nv;
}

void ddsrt_atomic_sub64 (volatile ddsrt_atomic_uint64_t *x, uint64_t v)
{
  const uint32_t idx = atomic64_lock_index (x);
  ddsrt_mutex_lock (&mutexes[idx]);
  x->v -= v;
  ddsrt_mutex_unlock (&mutexes[idx]);
}

uint64_t ddsrt_atomic_sub64_nv (volatile ddsrt_atomic_uint64_t *x, uint64_t v)
{
  const uint32_t idx = atomic64_lock_index (x);
  ddsrt_mutex_lock (&mutexes[idx]);
  const uint64_t ov = x->v;
  const uint64_t nv = ov - v;
  x->v = nv;
  ddsrt_mutex_unlock (&mutexes[idx]);
  return nv;
}

void ddsrt_atomic_and64 (volatile ddsrt_atomic_uint64_t *x, uint64_t v)
{
  const uint32_t idx = atomic64_lock_index (x);
  ddsrt_mutex_lock (&mutexes[idx]);
  x->v &= v;
  ddsrt_mutex_unlock (&mutexes[idx]);
}

uint64_t ddsrt_atomic_and64_ov (volatile ddsrt_atomic_uint64_t *x, uint64_t v)
{
  const uint32_t idx = atomic64_lock_index (x);
  ddsrt_mutex_lock (&mutexes[idx]);
  const uint64_t ov = x->v;
  const uint64_t nv = ov & v;
  x->v = nv;
  ddsrt_mutex_unlock (&mutexes[idx]);
  return ov;
}

uint64_t ddsrt_atomic_and64_nv (volatile ddsrt_atomic_uint64_t *x, uint64_t v)
{
  const uint32_t idx = atomic64_lock_index (x);
  ddsrt_mutex_lock (&mutexes[idx]);
  const uint64_t ov = x->v;
  const uint64_t nv = ov & v;
  x->v = nv;
  ddsrt_mutex_unlock (&mutexes[idx]);
  return nv;
}

void ddsrt_atomic_or64 (volatile ddsrt_atomic_uint64_t *x, uint64_t v)
{
  const uint32_t idx = atomic64_lock_index (x);
  ddsrt_mutex_lock (&mutexes[idx]);
  x->v |= v;
  ddsrt_mutex_unlock (&mutexes[idx]);
}

uint64_t ddsrt_atomic_or64_ov (volatile ddsrt_atomic_uint64_t *x, uint64_t v)
{
  const uint32_t idx = atomic64_lock_index (x);
  ddsrt_mutex_lock (&mutexes[idx]);
  const uint64_t ov = x->v;
  const uint64_t nv = ov | v;
  x->v = nv;
  ddsrt_mutex_unlock (&mutexes[idx]);
  return ov;
}

uint64_t ddsrt_atomic_or64_nv (volatile ddsrt_atomic_uint64_t *x, uint64_t v)
{
  const uint32_t idx = atomic64_lock_index (x);
  ddsrt_mutex_lock (&mutexes[idx]);
  const uint64_t ov = x->v;
  const uint64_t nv = ov | v;
  x->v = nv;
  ddsrt_mutex_unlock (&mutexes[idx]);
  return nv;
}

#endif /* DDSRT_HAVE_ATOMIC64 */

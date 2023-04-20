// Copyright(c) 2006 to 2021 ZettaScale Technology and others
//
// This program and the accompanying materials are made available under the
// terms of the Eclipse Public License v. 2.0 which is available at
// http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
// v. 1.0 which is available at
// http://www.eclipse.org/org/documents/edl-v10.php.
//
// SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

#include "dds/ddsrt/atomics.h"

/* LD, ST */
DDS_EXPORT extern inline uint32_t ddsrt_atomic_ld32 (const volatile ddsrt_atomic_uint32_t *x);
#if DDSRT_HAVE_ATOMIC64
DDS_EXPORT extern inline uint64_t ddsrt_atomic_ld64 (const volatile ddsrt_atomic_uint64_t *x);
#endif
DDS_EXPORT extern inline uintptr_t ddsrt_atomic_ldptr (const volatile ddsrt_atomic_uintptr_t *x);
DDS_EXPORT extern inline void *ddsrt_atomic_ldvoidp (const volatile ddsrt_atomic_voidp_t *x);
DDS_EXPORT extern inline void ddsrt_atomic_st32 (volatile ddsrt_atomic_uint32_t *x, uint32_t v);
#if DDSRT_HAVE_ATOMIC64
DDS_EXPORT extern inline void ddsrt_atomic_st64 (volatile ddsrt_atomic_uint64_t *x, uint64_t v);
#endif
DDS_EXPORT extern inline void ddsrt_atomic_stptr (volatile ddsrt_atomic_uintptr_t *x, uintptr_t v);
DDS_EXPORT extern inline void ddsrt_atomic_stvoidp (volatile ddsrt_atomic_voidp_t *x, void *v);
/* INC */
DDS_EXPORT extern inline void ddsrt_atomic_inc32 (volatile ddsrt_atomic_uint32_t *x);
#if DDSRT_HAVE_ATOMIC64
DDS_EXPORT extern inline void ddsrt_atomic_inc64 (volatile ddsrt_atomic_uint64_t *x);
#endif
DDS_EXPORT extern inline uint32_t ddsrt_atomic_inc32_ov (volatile ddsrt_atomic_uint32_t *x);
DDS_EXPORT extern inline uint32_t ddsrt_atomic_inc32_nv (volatile ddsrt_atomic_uint32_t *x);
#if DDSRT_HAVE_ATOMIC64
DDS_EXPORT extern inline uint64_t ddsrt_atomic_inc64_nv (volatile ddsrt_atomic_uint64_t *x);
#endif
/* DEC */
DDS_EXPORT extern inline void ddsrt_atomic_dec32 (volatile ddsrt_atomic_uint32_t *x);
#if DDSRT_HAVE_ATOMIC64
DDS_EXPORT extern inline void ddsrt_atomic_dec64 (volatile ddsrt_atomic_uint64_t *x);
#endif
DDS_EXPORT extern inline uint32_t ddsrt_atomic_dec32_nv (volatile ddsrt_atomic_uint32_t *x);
#if DDSRT_HAVE_ATOMIC64
DDS_EXPORT extern inline uint64_t ddsrt_atomic_dec64_nv (volatile ddsrt_atomic_uint64_t *x);
#endif
DDS_EXPORT extern inline uint32_t ddsrt_atomic_dec32_ov (volatile ddsrt_atomic_uint32_t *x);
#if DDSRT_HAVE_ATOMIC64
DDS_EXPORT extern inline uint64_t ddsrt_atomic_dec64_ov (volatile ddsrt_atomic_uint64_t *x);
#endif
/* ADD */
DDS_EXPORT extern inline void ddsrt_atomic_add32 (volatile ddsrt_atomic_uint32_t *x, uint32_t v);
#if DDSRT_HAVE_ATOMIC64
DDS_EXPORT extern inline void ddsrt_atomic_add64 (volatile ddsrt_atomic_uint64_t *x, uint64_t v);
#endif
DDS_EXPORT extern inline uint32_t ddsrt_atomic_add32_ov (volatile ddsrt_atomic_uint32_t *x, uint32_t v);
DDS_EXPORT extern inline uint32_t ddsrt_atomic_add32_nv (volatile ddsrt_atomic_uint32_t *x, uint32_t v);
#if DDSRT_HAVE_ATOMIC64
DDS_EXPORT extern inline uint64_t ddsrt_atomic_add64_nv (volatile ddsrt_atomic_uint64_t *x, uint64_t v);
#endif
/* SUB */
DDS_EXPORT extern inline void ddsrt_atomic_sub32 (volatile ddsrt_atomic_uint32_t *x, uint32_t v);
#if DDSRT_HAVE_ATOMIC64
DDS_EXPORT extern inline void ddsrt_atomic_sub64 (volatile ddsrt_atomic_uint64_t *x, uint64_t v);
#endif
DDS_EXPORT extern inline uint32_t ddsrt_atomic_sub32_ov (volatile ddsrt_atomic_uint32_t *x, uint32_t v);
DDS_EXPORT extern inline uint32_t ddsrt_atomic_sub32_nv (volatile ddsrt_atomic_uint32_t *x, uint32_t v);
#if DDSRT_HAVE_ATOMIC64
DDS_EXPORT extern inline uint64_t ddsrt_atomic_sub64_nv (volatile ddsrt_atomic_uint64_t *x, uint64_t v);
#endif
/* AND */
DDS_EXPORT extern inline void ddsrt_atomic_and32 (volatile ddsrt_atomic_uint32_t *x, uint32_t v);
#if DDSRT_HAVE_ATOMIC64
DDS_EXPORT extern inline void ddsrt_atomic_and64 (volatile ddsrt_atomic_uint64_t *x, uint64_t v);
#endif
DDS_EXPORT extern inline uint32_t ddsrt_atomic_and32_ov (volatile ddsrt_atomic_uint32_t *x, uint32_t v);
#if DDSRT_HAVE_ATOMIC64
DDS_EXPORT extern inline uint64_t ddsrt_atomic_and64_ov (volatile ddsrt_atomic_uint64_t *x, uint64_t v);
#endif
DDS_EXPORT extern inline uint32_t ddsrt_atomic_and32_nv (volatile ddsrt_atomic_uint32_t *x, uint32_t v);
#if DDSRT_HAVE_ATOMIC64
DDS_EXPORT extern inline uint64_t ddsrt_atomic_and64_nv (volatile ddsrt_atomic_uint64_t *x, uint64_t v);
#endif
/* OR */
DDS_EXPORT extern inline void ddsrt_atomic_or32 (volatile ddsrt_atomic_uint32_t *x, uint32_t v);
#if DDSRT_HAVE_ATOMIC64
DDS_EXPORT extern inline void ddsrt_atomic_or64 (volatile ddsrt_atomic_uint64_t *x, uint64_t v);
#endif
DDS_EXPORT extern inline uint32_t ddsrt_atomic_or32_ov (volatile ddsrt_atomic_uint32_t *x, uint32_t v);
#if DDSRT_HAVE_ATOMIC64
DDS_EXPORT extern inline uint64_t ddsrt_atomic_or64_ov (volatile ddsrt_atomic_uint64_t *x, uint64_t v);
#endif
DDS_EXPORT extern inline uint32_t ddsrt_atomic_or32_nv (volatile ddsrt_atomic_uint32_t *x, uint32_t v);
#if DDSRT_HAVE_ATOMIC64
DDS_EXPORT extern inline uint64_t ddsrt_atomic_or64_nv (volatile ddsrt_atomic_uint64_t *x, uint64_t v);
#endif
/* CAS */
DDS_EXPORT extern inline int ddsrt_atomic_cas32 (volatile ddsrt_atomic_uint32_t *x, uint32_t exp, uint32_t des);
#if DDSRT_HAVE_ATOMIC64
DDS_EXPORT extern inline int ddsrt_atomic_cas64 (volatile ddsrt_atomic_uint64_t *x, uint64_t exp, uint64_t des);
#endif
DDS_EXPORT extern inline int ddsrt_atomic_casptr (volatile ddsrt_atomic_uintptr_t *x, uintptr_t exp, uintptr_t des);
DDS_EXPORT extern inline int ddsrt_atomic_casvoidp (volatile ddsrt_atomic_voidp_t *x, void *exp, void *des);
#if DDSRT_HAVE_ATOMIC_LIFO
DDS_EXPORT extern inline int ddsrt_atomic_casvoidp2 (volatile ddsrt_atomic_uintptr2_t *x, uintptr_t a0, uintptr_t b0, uintptr_t a1, uintptr_t b1);
#endif
/* FENCES */
DDS_EXPORT extern inline void ddsrt_atomic_fence (void);
DDS_EXPORT extern inline void ddsrt_atomic_fence_ldld (void);
DDS_EXPORT extern inline void ddsrt_atomic_fence_stst (void);
DDS_EXPORT extern inline void ddsrt_atomic_fence_acq (void);
DDS_EXPORT extern inline void ddsrt_atomic_fence_rel (void);

#if DDSRT_HAVE_ATOMIC_LIFO
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

/* On platforms that don't provide 64-bit atomic operations, emulate them by hashing
   the variable's address to a small set of mutexes.

   This also defines the GCC builtins on SPARCv8 for 32-bit operations.  It would be
   more appropriate to simply define the ddsrt_atomic_... functions properly in that
   case and avoid squatting in the __sync_... namespace, but SPARCv8 support really
   is just for fun and it doesn't seem worth the bother right now */

#if DDSRT_HAVE_ATOMIC64

void ddsrt_atomics_init (void) { }
void ddsrt_atomics_fini (void) { }

#else

#include "dds/ddsrt/sync.h"

/* SPARCv8 depends on these mutexes already for one-shot initialisation of the ddsrt
   code.  Using PTHREAD_MUTEX_INITIALIZER guarantees they are properly initialized.
   Once a platform shows up that defines that macro where we don't used pthread mutexes
   something else will have to be done. */
#define N_MUTEXES_LG2 4
#define N_MUTEXES     (1 << N_MUTEXES_LG2)
#if !defined(PTHREAD_MUTEX_INITIALIZER) || defined(__ZEPHYR__)
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
#else
static ddsrt_mutex_t mutexes[N_MUTEXES] = {
  { PTHREAD_MUTEX_INITIALIZER }, { PTHREAD_MUTEX_INITIALIZER },
  { PTHREAD_MUTEX_INITIALIZER }, { PTHREAD_MUTEX_INITIALIZER },
  { PTHREAD_MUTEX_INITIALIZER }, { PTHREAD_MUTEX_INITIALIZER },
  { PTHREAD_MUTEX_INITIALIZER }, { PTHREAD_MUTEX_INITIALIZER },
  { PTHREAD_MUTEX_INITIALIZER }, { PTHREAD_MUTEX_INITIALIZER },
  { PTHREAD_MUTEX_INITIALIZER }, { PTHREAD_MUTEX_INITIALIZER },
  { PTHREAD_MUTEX_INITIALIZER }, { PTHREAD_MUTEX_INITIALIZER },
  { PTHREAD_MUTEX_INITIALIZER }, { PTHREAD_MUTEX_INITIALIZER }
};
void ddsrt_atomics_init (void) { }
void ddsrt_atomics_fini (void) { }
#endif

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

#define DDSRT_FAKE_ATOMIC64(name, oper, ret) \
  uint64_t ddsrt_atomic_##name##64_##ret (volatile ddsrt_atomic_uint64_t *x, uint64_t v); \
  uint64_t ddsrt_atomic_##name##64_##ret (volatile ddsrt_atomic_uint64_t *x, uint64_t v) \
  { \
    const uint64_t idx = atomic64_lock_index (x); \
    ddsrt_mutex_lock (&mutexes[idx]); \
    const uint64_t ov = x->v; \
    const uint64_t nv = ov oper v; \
    x->v = nv; \
    ddsrt_mutex_unlock (&mutexes[idx]); \
    return ret; \
  }
#define DDSRT_FAKE_ATOMIC64_TRIPLET(name, oper) \
  DDSRT_FAKE_ATOMIC64(name, oper, nv) \
  DDSRT_FAKE_ATOMIC64(name, oper, ov) \
  void ddsrt_atomic_##name##64 (volatile ddsrt_atomic_uint64_t *x, uint64_t v) { \
    (void) ddsrt_atomic_##name##64_ov (x, v); \
  }

uint64_t ddsrt_atomic_ld64 (const volatile ddsrt_atomic_uint64_t *x)
{
  const uint32_t idx = atomic64_lock_index (x);
  ddsrt_mutex_lock (&mutexes[idx]);
  const uint64_t v = x->v;
  ddsrt_mutex_unlock (&mutexes[idx]);
  return v;
}

void ddsrt_atomic_st64 (volatile ddsrt_atomic_uint64_t *x, uint64_t v)
{
  const uint32_t idx = atomic64_lock_index (x);
  ddsrt_mutex_lock (&mutexes[idx]);
  x->v = v;
  ddsrt_mutex_unlock (&mutexes[idx]);
}

DDSRT_FAKE_ATOMIC64_TRIPLET(add, +)
DDSRT_FAKE_ATOMIC64_TRIPLET(sub, -)
DDSRT_FAKE_ATOMIC64_TRIPLET(or,  |)
DDSRT_FAKE_ATOMIC64_TRIPLET(and, &)

void ddsrt_atomic_inc64 (volatile ddsrt_atomic_uint64_t *x) {
  ddsrt_atomic_add64 (x, 1);
}
uint64_t ddsrt_atomic_inc64_nv (volatile ddsrt_atomic_uint64_t *x) {
  return ddsrt_atomic_add64_nv (x, 1);
}
void ddsrt_atomic_dec64 (volatile ddsrt_atomic_uint64_t *x) {
  ddsrt_atomic_sub64 (x, 1);
}
uint64_t ddsrt_atomic_dec64_nv (volatile ddsrt_atomic_uint64_t *x) {
  return ddsrt_atomic_sub64_nv (x, 1);
}

#undef DDSRT_FAKE_ATOMIC64_TRIPLET
#undef DDSRT_FAKE_ATOMIC64

/* SPARCv8 doesn't support any atomic operations beyond a simple atomic exchange.  GCC happily
   compiles the __sync_* functions into library calls, and implementing them as such will do
   the trick.  The rarity of SPARCv8 machines (EOL'd 2 decades ago) */
#ifdef __sparc_v8__
#define DDSRT_FAKE_SYNC(name, size, oper, ret)                          \
  unsigned __sync_##name##_##size (volatile unsigned *x, unsigned v) \
  {                                                                     \
    const uint32_t idx = atomic64_lock_index ((const volatile ddsrt_atomic_uint64_t *) x); \
    ddsrt_mutex_lock (&mutexes[idx]);                                   \
    const uint32_t ov = *x;                                             \
    const uint32_t nv = ov oper v;                                      \
    *x = nv;                                                            \
    ddsrt_mutex_unlock (&mutexes[idx]);                                 \
    return ret;                                                         \
  }
#define DDSRT_FAKE_SYNC_PAIR(name, size, oper)          \
  DDSRT_FAKE_SYNC(name##_and_fetch, size, oper, nv)     \
  DDSRT_FAKE_SYNC(fetch_and_##name, size, oper, ov)

DDSRT_FAKE_SYNC_PAIR (add, 4, +)
DDSRT_FAKE_SYNC_PAIR (sub, 4, -)
DDSRT_FAKE_SYNC_PAIR (or,  4, |)
DDSRT_FAKE_SYNC_PAIR (and, 4, &)

bool __sync_bool_compare_and_swap_4 (volatile unsigned *x, unsigned exp, unsigned des)
{
  const uint32_t idx = atomic64_lock_index ((const volatile ddsrt_atomic_uint64_t *) x);
  ddsrt_mutex_lock (&mutexes[idx]);
  if (*x == exp)
  {
    *x = des;
    ddsrt_mutex_unlock (&mutexes[idx]);
    return true;
  }
  else
  {
    ddsrt_mutex_unlock (&mutexes[idx]);
    return false;
  }
}

#undef DDSRT_FAKE_SYNC_PAIR
#undef DDSRT_FAKE_SYNC
#endif /* SPARCv8 hack */

#endif /* DDSRT_HAVE_ATOMIC64 */

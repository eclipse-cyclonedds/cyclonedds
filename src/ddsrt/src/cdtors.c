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
#include "dds/ddsrt/cdtors.h"
#include "dds/ddsrt/sync.h"
#include "dds/ddsrt/time.h"
#include "dds/ddsrt/random.h"

#if _WIN32
/* Sockets API initialization is only necessary on Microsoft Windows. The
   overly correct approach would be to abstract this a little further, but for
   now a pragmatic approach will suffice. */
extern void ddsrt_winsock_init(void);
extern void ddsrt_winsock_fini(void);
extern void ddsrt_time_init(void);
extern void ddsrt_time_fini(void);
#endif

#define INIT_STATUS_OK 0x80000000u
static ddsrt_atomic_uint32_t init_status = DDSRT_ATOMIC_UINT32_INIT(0);
static ddsrt_mutex_t init_mutex;
static ddsrt_cond_t init_cond;

void ddsrt_init (void)
{
  uint32_t v;
  v = ddsrt_atomic_inc32_nv(&init_status);
retry:
  if (v > INIT_STATUS_OK)
    return;
  else if (v == 1) {
    ddsrt_mutex_init(&init_mutex);
    ddsrt_cond_init(&init_cond);
#if _WIN32
    ddsrt_winsock_init();
    ddsrt_time_init();
#endif
    ddsrt_random_init();
    ddsrt_atomics_init();
    ddsrt_atomic_or32(&init_status, INIT_STATUS_OK);
  } else {
    while (v > 1 && !(v & INIT_STATUS_OK)) {
#ifndef __COVERITY__
      /* This sleep makes Coverity warn about possibly sleeping while holding in a lock
         in many places, all because just-in-time creation of a thread descriptor ends
         up here.  Since sleeping is merely meant as a better alternative to spinning,
         skip the sleep when being analyzed. */
      dds_sleepfor(10000000);
#endif
      v = ddsrt_atomic_ld32(&init_status);
    }
    goto retry;
  }
}

void ddsrt_fini (void)
{
  uint32_t v, nv;
  do {
    v = ddsrt_atomic_ld32(&init_status);
    if (v == (INIT_STATUS_OK | 1)) {
      nv = 1;
    } else {
      nv = v - 1;
    }
  } while (!ddsrt_atomic_cas32(&init_status, v, nv));
  if (nv == 1)
  {
    ddsrt_cond_destroy(&init_cond);
    ddsrt_mutex_destroy(&init_mutex);
    ddsrt_random_fini();
    ddsrt_atomics_fini();
#if _WIN32
    ddsrt_winsock_fini();
    ddsrt_time_fini();
#endif
    ddsrt_atomic_dec32(&init_status);
  }
}

ddsrt_mutex_t *ddsrt_get_singleton_mutex(void)
{
  return &init_mutex;
}

ddsrt_cond_t *ddsrt_get_singleton_cond(void)
{
  return &init_cond;
}

#ifdef _WIN32
#include "dds/ddsrt/threads.h"
#include "dds/ddsrt/misc.h"

DDSRT_WARNING_GNUC_OFF(missing-prototypes)
DDSRT_WARNING_CLANG_OFF(missing-prototypes)

/* Executed before DllMain within the context of the thread. Located here too
   avoid removal due to link time optimization. */
void WINAPI
ddsrt_cdtor(PVOID handle, DWORD reason, PVOID reserved)
{
  (void)handle;
  (void)reason;
  (void)reserved;
  switch (reason) {
    case DLL_PROCESS_ATTACH:
      ddsrt_init();
      /* fall through */
    case DLL_THREAD_ATTACH:
      ddsrt_thread_init(reason);
      break;
    case DLL_THREAD_DETACH: /* Specified when thread exits. */
      ddsrt_thread_fini(reason);
      break;
    case DLL_PROCESS_DETACH: /* Specified when main thread exits. */
      ddsrt_thread_fini(reason);
      ddsrt_fini();
      break;
    default:
      break;
  }
}

DDSRT_WARNING_GNUC_ON(missing-prototypes)
DDSRT_WARNING_CLANG_ON(missing-prototypes)

// These instructions are very specific to the Windows platform. They register
// a function (or multiple) as a TLS initialization function. TLS initializers
// are executed when a thread (or program) attaches or detaches. In contrast to
// DllMain, a TLS initializer is also executed when the library is linked
// statically. TLS initializers are always executed before DllMain (both when
// the library is attached and detached). See http://www.nynaeve.net/?p=190,
// for a detailed explanation on TLS initializers. Boost and/or POSIX Threads
// for Windows code bases may also form good sources of information on this
// subject.
//
// These instructions could theoretically be hidden in the build system, but
// doing so would be going a bit overboard as only Windows offers (and
// requires) this type of functionality/initialization. Apart from that the
// logic isn't exactly as trivial as for example determining the endianness of
// a platform, so keeping this close to the implementation is probably wise.
#if __MINGW32__
  PIMAGE_TLS_CALLBACK __crt_xl_tls_callback__ __attribute__ ((section(".CRT$XLZ"))) = ddsrt_cdtor;
#elif _WIN64
  #pragma comment (linker, "/INCLUDE:_tls_used")
  #pragma comment (linker, "/INCLUDE:tls_callback_func")
  #pragma const_seg(".CRT$XLZ")
  EXTERN_C const PIMAGE_TLS_CALLBACK tls_callback_func = ddsrt_cdtor;
  #pragma const_seg()
#else
  #pragma comment (linker, "/INCLUDE:__tls_used")
  #pragma comment (linker, "/INCLUDE:_tls_callback_func")
  #pragma data_seg(".CRT$XLZ")
  EXTERN_C PIMAGE_TLS_CALLBACK tls_callback_func = ddsrt_cdtor;
  #pragma data_seg()
 #endif
#else /* _WIN32 */
void __attribute__((constructor)) ddsrt_ctor(void);
void __attribute__((destructor)) ddsrt_dtor(void);

void __attribute__((constructor)) ddsrt_ctor(void)
{
  ddsrt_init();
}

void __attribute__((destructor)) ddsrt_dtor(void)
{
  ddsrt_fini();
}
#endif /* _WIN32 */


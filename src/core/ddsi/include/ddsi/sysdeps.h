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
#ifndef SYSDEPS_H
#define SYSDEPS_H

#include "os/os.h"

#include "ddsi/q_inline.h"

#ifndef os_sockECONNRESET
#ifdef WSAECONNRESET
#define os_sockECONNRESET WSAECONNRESET
#else
#define os_sockECONNRESET ECONNRESET
#endif
#endif

#ifndef os_sockEPERM
#ifdef WSAEACCES
#define os_sockEPERM WSAEACCES
#else
#define os_sockEPERM EPERM
#endif
#endif

#if defined (__linux) || defined (__sun) || defined (__APPLE__) || defined (INTEGRITY) || defined (AIX) || defined (OS_RTEMS_DEFS_H) || defined (__VXWORKS__) || defined (__Lynx__) || defined (__linux__) || defined (OS_QNX_DEFS_H)
#define SYSDEPS_HAVE_MSGHDR 1
#define SYSDEPS_HAVE_RECVMSG 1
#define SYSDEPS_HAVE_SENDMSG 1
#endif

#if defined (__linux) || defined (__sun) || defined (__APPLE__) || defined (INTEGRITY) || defined (AIX) || defined (OS_RTEMS_DEFS_H) || defined (__VXWORKS__) || defined (__linux__) || defined (OS_QNX_DEFS_H)
#define SYSDEPS_HAVE_IOVEC 1
#endif

#if defined (__linux) || defined (__sun) || defined (__APPLE__) || defined (AIX) || defined (__Lynx__) || defined (OS_QNX_DEFS_H)
#define SYSDEPS_HAVE_RANDOM 1
#include <unistd.h>
#endif

#if defined (__linux) || defined (__sun) || defined (__APPLE__) || defined (AIX) || defined (OS_QNX_DEFS_H)
#define SYSDEPS_HAVE_GETRUSAGE 1
#include <sys/time.h> /* needed for Linux, exists on all four */
#include <sys/times.h> /* needed for AIX, exists on all four */
#include <sys/resource.h>
#endif

#if defined (__linux) && defined (CLOCK_THREAD_CPUTIME_ID)
#define SYSDEPS_HAVE_CLOCK_THREAD_CPUTIME 1
#endif

#if defined (INTEGRITY)
#include <sys/uio.h>
#include <limits.h>
#endif

#if defined (VXWORKS_RTP)
#include <net/uio.h>
#endif

#if defined (_WIN32)
typedef SOCKET os_handle;
#define Q_VALID_SOCKET(s) ((s) != INVALID_SOCKET)
#define Q_INVALID_SOCKET INVALID_SOCKET
#else /* All Unixes have socket() return -1 on error */
typedef int os_handle;
#define Q_VALID_SOCKET(s) ((s) != -1)
#define Q_INVALID_SOCKET -1
#endif

/* From MSDN: from Vista & 2k3 onwards, a macro named MemoryBarrier is
   defined, XP needs inline assembly.  Unfortunately, MemoryBarrier()
   is a function on x86 ...

   Definition below is taken from the MSDN page on MemoryBarrier() */
#ifndef MemoryBarrier
#if NTDDI_VERSION >= NTDDI_WS03 && defined _M_IX86
#define MemoryBarrier() do {                    \
    LONG Barrier;                               \
    __asm {                                     \
      xchg Barrier, eax                         \
    }                                           \
  } while (0)
#endif /* x86 */

/* Don't try interworking with thumb - one thing at a time. Do a DMB
   SY if supported, else no need for a memory barrier. (I think.) */
#if defined _M_ARM && ! defined _M_ARMT
#define MemoryBarrierARM __emit (0xf57ff05f) /* 0xf57ff05f or 0x5ff07ff5 */
#if _M_ARM > 7
/* if targetting ARMv7 the dmb instruction is available */
#define MemoryBarrier() MemoryBarrierARM
#else
/* else conditional on actual hardware platform */
extern void (*q_maybe_membar) (void);
#define MemoryBarrier() q_maybe_membar ()
#define NEED_ARM_MEMBAR_SUPPORT 1
#endif /* ARM version */
#endif /* ARM */

#endif /* !def MemoryBarrier */

#if defined (__sun) && !defined (_XPG4_2)
#define SYSDEPS_MSGHDR_ACCRIGHTS 1
#else
#define SYSDEPS_MSGHDR_ACCRIGHTS 0
#endif
#if SYSDEPS_MSGHDR_ACCRIGHTS
#define SYSDEPS_MSGHDR_FLAGS 0
#else
#define SYSDEPS_MSGHDR_FLAGS 1
#endif

#if defined (__cplusplus)
}
#endif

#if defined (__cplusplus)
extern "C" {
#endif

#define ASSERT_RDLOCK_HELD(x) ((void) 0)
#define ASSERT_WRLOCK_HELD(x) ((void) 0)
#define ASSERT_MUTEX_HELD(x) ((void) 0)

#if SYSDEPS_HAVE_IOVEC
typedef struct iovec ddsi_iovec_t;
#elif defined _WIN32 && !defined WINCE
typedef struct ddsi_iovec {
  unsigned iov_len;
  void *iov_base;
} ddsi_iovec_t;
#define DDSI_IOVEC_MATCHES_WSABUF do { \
  struct ddsi_iovec_matches_WSABUF { \
    char sizeof_matches[sizeof(struct ddsi_iovec) == sizeof(WSABUF) ? 1 : -1]; \
    char base_off_matches[offsetof(struct ddsi_iovec, iov_base) == offsetof(WSABUF, buf) ? 1 : -1]; \
    char base_size_matches[sizeof(((struct ddsi_iovec *)8)->iov_base) == sizeof(((WSABUF *)8)->buf) ? 1 : -1]; \
    char len_off_matches[offsetof(struct ddsi_iovec, iov_len) == offsetof(WSABUF, len) ? 1 : -1]; \
    char len_size_matches[sizeof(((struct ddsi_iovec *)8)->iov_len) == sizeof(((WSABUF *)8)->len) ? 1 : -1]; \
  }; } while (0)
#else
typedef struct ddsi_iovec {
  void *iov_base;
  size_t iov_len;
} ddsi_iovec_t;
#endif

#if ! SYSDEPS_HAVE_MSGHDR
struct msghdr
{
  void *msg_name;
  socklen_t msg_namelen;
  ddsi_iovec_t *msg_iov;
  size_t msg_iovlen;
  void *msg_control;
  size_t msg_controllen;
  int msg_flags;
};
#endif

#ifndef MSG_TRUNC
#define MSG_TRUNC 1
#endif

#if ! SYSDEPS_HAVE_RECVMSG
/* Only implements iovec of length 1, no control */
ssize_t recvmsg (os_handle fd, struct msghdr *message, int flags);
#endif
#if ! SYSDEPS_HAVE_SENDMSG
ssize_t sendmsg (os_handle fd, const struct msghdr *message, int flags);
#endif
#if ! SYSDEPS_HAVE_RANDOM
long random (void);
#endif

int64_t get_thread_cputime (void);

int os_threadEqual (os_threadId a, os_threadId b);
void log_stacktrace (const char *name, os_threadId tid);

#if (_LP64 && __GCC_HAVE_SYNC_COMPARE_AND_SWAP_16) || (!_LP64 && __GCC_HAVE_SYNC_COMPARE_AND_SWAP_8)
  #define HAVE_ATOMIC_LIFO 1
  #if _LP64
  typedef union { __int128 x; struct { uintptr_t a, b; } s; } os_atomic_uintptr2_t;
  #else
  typedef union { uint64_t x; struct { uintptr_t a, b; } s; } os_atomic_uintptr2_t;
  #endif
  typedef struct os_atomic_lifo {
    os_atomic_uintptr2_t aba_head;
  } os_atomic_lifo_t;
  void os_atomic_lifo_init (os_atomic_lifo_t *head);
  void os_atomic_lifo_push (os_atomic_lifo_t *head, void *elem, size_t linkoff);
  void os_atomic_lifo_pushmany (os_atomic_lifo_t *head, void *first, void *last, size_t linkoff);
  void *os_atomic_lifo_pop (os_atomic_lifo_t *head, size_t linkoff);
#endif

#if defined (__cplusplus)
}
#endif

#endif /* SYSDEPS_H */

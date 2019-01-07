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
#ifndef OS_PLATFORM_H
#define OS_PLATFORM_H

#include <vxWorks.h>
#include <stdbool.h>
#include <stdint.h>
#include <unistd.h>

#define PRIdSIZE "zd"
#define PRIuSIZE "zu"
#define PRIxSIZE "zx"

#ifdef _WRS_KERNEL
/* inttypes.h does not exist in VxWorks DKM */
#include <st_inttypes.h>
#include <cafe/inttypes.h>

/* The above inttypes includes don't seem to define uintmax_t &c. */
#ifdef _WRS_CONFIG_LP64 /* Used in cafe/inttypes.h too */
#define _PFX_64 "l"
typedef unsigned long int       uintmax_t;
#else
#define _PFX_64 "ll"
typedef unsigned long long int  uintmax_t;
#endif

/* FIXME: Not a complete replacement for inttypes.h (yet); no SCN/PRI?LEAST/FAST/etc */
/* FIXME: Wrap all of them in #ifndefs */
#define PRId8      "d"
#define PRId16     "d"
#define PRId32     "d"
#define PRId64     _PFX_64 "d"

#define PRIi8      "i"
#define PRIi16     "i"
#define PRIi32     "i"
#define PRIi64     _PFX_64 "i"

#define PRIo8      "o"
#define PRIo16     "o"
#define PRIo32     "o"
#define PRIo64     _PFX_64 "o"

#define PRIu8      "u"
#define PRIu16     "u"
#define PRIu32     "u"
#define PRIu64     _PFX_64 "u"

#define PRIx8      "x"
#define PRIx16     "x"
#define PRIx32     "x"
/*#define PRIx64     _PFX_64 "x" // Defined in cafe/inttypes.h apparently */

#define PRIX8      "X"
#define PRIX16     "X"
#define PRIX32     "X"
#define PRIX64     _PFX_64 "X"

#define PRIdMAX    _PFX_64 "d"
#define PRIiMAX    _PFX_64 "i"
#define PRIoMAX    _PFX_64 "o"
#define PRIuMAX    _PFX_64 "u"
#define PRIxMAX    _PFX_64 "x"
#define PRIXMAX    _PFX_64 "X"

#define PRIdPTR    _PFX_64 "d"
#define PRIiPTR    _PFX_64 "i"
#define PRIoPTR    _PFX_64 "o"
/*#define PRIuPTR    _PFX_64 "u" // Defined in cafe/inttypes.h apparently */
/*#define PRIxPTR    _PFX_64 "x" // Defined in cafe/inttypes.h apparently */
#define PRIXPTR    _PFX_64 "X"

#define INFINITY infinity()
#define NAN       ((float)(INFINITY * 0.0F))

#if !defined(__PPC) && !defined(__x64_64__)
/* FIXME: Is this still required for VxWorks 7? */
#define OS_USE_ALLIGNED_MALLOC
#endif

#else
#include <inttypes.h>
#endif /* _WRS_KERNEL */

#if defined (__cplusplus)
extern "C" {
#endif

#define OS_VXWORKS 1
#define OS_HAVE_GETRUSAGE 0

typedef double os_timeReal;
typedef int os_timeSec;
#ifdef _WRS_KERNEL
typedef RTP_ID os_procId; /* typedef struct wind_rtp *RTP_ID */
#define PRIprocId "d"
#else
typedef pid_t os_procId;
#define PRIprocId "d"

/* If unistd.h is included after stdint.h, intptr_t will be defined twice.
 * It seems like this is an issue with the VxWorks provided header-files. The
 * define done by stdint.h is not checked in unistd.h. Below is a workaround
 * for this issue. */
#if !defined _INTPTR_T && defined _INTPTR
# define _INTPTR_T _INTPTR
#endif
#endif

#include "os/posix/os_platform_socket.h"
#ifdef _WRS_KERNEL
/* Pulling in netinet/in.h automatically pulls in net/mbuf.h in VxWorks DKM */
#undef m_next
#undef m_flags
#endif
#include "os/posix/os_platform_sync.h"
#include "os/posix/os_platform_thread.h"
#include "os/posix/os_platform_stdlib.h"

#if defined (__cplusplus)
}
#endif

#endif /* OS_PLATFORM_H */

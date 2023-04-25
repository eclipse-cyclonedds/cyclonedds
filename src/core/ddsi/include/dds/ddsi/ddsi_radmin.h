// Copyright(c) 2006 to 2021 ZettaScale Technology and others
//
// This program and the accompanying materials are made available under the
// terms of the Eclipse Public License v. 2.0 which is available at
// http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
// v. 1.0 which is available at
// http://www.eclipse.org/org/documents/edl-v10.php.
//
// SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

#ifndef DDSI_RADMIN_H
#define DDSI_RADMIN_H

#include <stddef.h>

#include "dds/ddsrt/time.h"
#include "dds/ddsrt/atomics.h"
#include "dds/ddsrt/threads.h"
#include "dds/ddsrt/align.h"
#include "dds/ddsrt/static_assert.h"
#include "dds/ddsi/ddsi_locator.h"
#include "dds/ddsi/ddsi_protocol.h"

#if defined (__cplusplus)
extern "C" {
#endif

struct ddsi_rbuf;
struct ddsi_rmsg;
struct ddsi_rdata;
struct ddsi_rsample_info;
struct ddsi_tran_conn;

struct ddsi_rmsg_chunk {
  struct ddsi_rbuf *rbuf;
  struct ddsi_rmsg_chunk *next;

  /* Size is 0 after initial allocation, must be set with
     ddsi_rmsg_setsize after receiving a packet from the kernel and
     before processing it.  */
  union {
    uint32_t size;

    /* to ensure reasonable alignment of payload */
    int64_t l;
    double d;
    void *p;
  } u;

  /* unsigned char payload[] -- disallowed by C99 because of nesting */
};

struct ddsi_rmsg {
  /* Reference count: all references to rdatas of this message are
     counted. The rdatas themselves do not have a reference count.

     The refcount is biased by RMSG_REFCOUNT_UNCOMMITED_BIAS while
     still being inserted to allow verifying it is still uncommitted
     when allocating memory, increasing refcounts, &c.

     Each rdata adds RMS_REFCOUNT_RDATA_BIAS when it leaves
     defragmentation until it has been rejected by reordering or has
     been scheduled for delivery.  This allows delaying the
     decrementing of refcounts until after a sample has been added to
     all radmins even though be delivery of it may take place in
     concurrently. */
  ddsrt_atomic_uint32_t refcount;

  /* Worst-case memory requirement is gigantic (64kB UDP packet, only
     1-byte final fragments, each of one a new interval, or maybe 1
     byte messages, destined for many readers and in each case
     introducing a new interval, with receiver state changes in
     between, &c.), so we can either:

     - allocate a _lot_ and cover the worst case

     - allocate enough for all "reasonable" cases, discarding data when that limit is hit

     - dynamically add chunks of memory, and even entire receive buffers.

     The latter seems the best approach, especially because it also
     covers the second one.  We treat the other chunks specially,
     which is not strictly required but also not entirely
     unreasonable, considering that the first chunk has the refcount &
     the real packet. */
  struct ddsi_rmsg_chunk *lastchunk;

  /* whether to log */
  bool trace;

  struct ddsi_rmsg_chunk chunk;
};
DDSRT_STATIC_ASSERT (sizeof (struct ddsi_rmsg) == offsetof (struct ddsi_rmsg, chunk) + sizeof (struct ddsi_rmsg_chunk));
#define DDSI_RMSG_PAYLOAD(m) ((unsigned char *) (m + 1))
#define DDSI_RMSG_PAYLOADOFF(m, o) (DDSI_RMSG_PAYLOAD (m) + (o))


struct ddsi_rdata {
  struct ddsi_rmsg *rmsg;         /* received (and refcounted) in rmsg */
  struct ddsi_rdata *nextfrag;    /* fragment chain */
  uint32_t min, maxp1;          /* fragment as byte offsets */
  uint16_t submsg_zoff;         /* offset to submessage from packet start, or 0 */
  uint16_t payload_zoff;        /* offset to payload from packet start */
  uint16_t keyhash_zoff;        /* offset to keyhash from packet start, or 0 */
#ifndef NDEBUG
  ddsrt_atomic_uint32_t refcount_bias_added;
#endif
};

/* All relative offsets in packets that we care about (submessage
   header, payload, writer info) are at multiples of 4 bytes and
   within 64kB, so technically we can make do with 14 bits instead of
   16, in case we run out of space.

   If we _really_ need to squeeze out every last bit, only the submsg
   offset really requires 14 bits, the for the others we could use an
   offset relative to the submessage header so that it is limited by
   the maximum size of the inline QoS ...  Defining the macros now, so
   we have the option to do wild things. */
#ifndef NDEBUG
#define DDSI_ZOFF_TO_OFF(zoff) ((unsigned) (zoff))
#define DDSI_OFF_TO_ZOFF(off) (assert ((off) < 65536), ((unsigned short) (off)))
#else
#define DDSI_ZOFF_TO_OFF(zoff) ((unsigned) (zoff))
#define DDSI_OFF_TO_ZOFF(off) ((unsigned short) (off))
#endif
#define DDSI_RDATA_PAYLOAD_OFF(rdata) DDSI_ZOFF_TO_OFF ((rdata)->payload_zoff)
#define DDSI_RDATA_SUBMSG_OFF(rdata) DDSI_ZOFF_TO_OFF ((rdata)->submsg_zoff)
#define DDSI_RDATA_KEYHASH_OFF(rdata) DDSI_ZOFF_TO_OFF ((rdata)->keyhash_zoff)

#if defined (__cplusplus)
}
#endif

#endif /* DDSI_RADMIN_H */

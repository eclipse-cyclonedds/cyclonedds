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
#include <ctype.h>
#include <stddef.h>
#include <stdarg.h>
#include <stdlib.h>
#include <limits.h>

#if HAVE_VALGRIND && ! defined (NDEBUG)
#include <memcheck.h>
#define USE_VALGRIND 1
#else
#define USE_VALGRIND 0
#endif

#include "os/os.h"

#include "util/ut_avl.h"
#include "ddsi/q_protocol.h"
#include "ddsi/q_rtps.h"
#include "ddsi/q_misc.h"

#include "ddsi/q_config.h"
#include "ddsi/q_log.h"

#include "ddsi/q_plist.h"
#include "ddsi/q_unused.h"
#include "ddsi/q_radmin.h"
#include "ddsi/q_bitset.h"
#include "ddsi/q_thread.h"
#include "ddsi/q_globals.h" /* for mattr, cattr */

/* OVERVIEW ------------------------------------------------------------

   The receive path of DDSI2 has any number of receive threads that
   accept data from sockets and (synchronously) push it up the
   protocol stack, potentially offloading processing to other threads
   at some point.  In particular, delivery of data can safely be
   offloaded.

   Each receive thread MUST process each message synchronously to the
   point where all additional indexing and other administrative data
   derived from the message has been stored in memory.  This storage
   is _always_ adjacent to the message that caused it.  Also, once it
   finishes processing a message, the reference count of that message
   may not be incremented anymore.

   In practice that means the receive thread can do everything by
   itself (handling acks and heartbeats, handling discovery,
   delivering data to the kernel), or it can offload everything but
   defragmentation and reordering.

   The data structures and functions in this file are all concerned
   with the storage of messages in buffers, organising their parts
   into ordered chains of fragments of (DDS) samples, reordering them
   into chains of consecutive samples, and queueing these chains for
   further processing.

   Storage is organised in the following hierarchy; rdata is included
   because it is is very intimately involved with the reference
   counting.  For the indexing structures for defragmenting and
   reordering messages, see RDATA, DEFRAG and REORDER below.

   nn_rbufpool

                One or more rbufs. Currently, an rbufpool is owned by
                a single receive thread, and only this thread may
                allocate memory from the rbufs contained in the pool
                and increment reference counts to the messages in it,
                while all threads may decrement these reference counts
                / release memory from it.

                (It is probably better to share the pool amongst all
                threads and make the rbuf the thing owned by this
                thread; and in fact the buffer pool isn't really
                necessary 'cos they handle multiple messages and
                therefore the malloc/free overhead is negligible.  It
                does provide a convenient location for storing some
                constant data.)

   nn_rbuf

                Largish buffer for receiving several UDP packets and
                for storing partially decoded and indexing information
                directly following the packet.

   nn_rmsg

                One message in an rbuf; the layout for one message is
                rmsg, raw udp packet, decoder stuff mixed with rdata,
                defragmentation and message reordering state.  One
                rbuf can contain many messages.

   nn_rdata

                Represents one Data/DataFrag submessage.  These
                contain some administrative data & point to the
                corresponding part of the message, and are referenced
                by the defragmentation and reordering (defrag, reorder)
                tables and the delivery queues.

   Each rmsg contains a reference count tracking all references to all
   rdatas contained in that message.  All data for one message in the
   rbuf (raw data, decoder info, &c.) is dependent on the refcount of
   the rmsg: once that reference count goes to zero _all_ dependent
   stuff becomes invalid immediately.

   As noted, the receive thread that owns the rbuf is the only one
   allowed to add data to it, which implies that this thread must do
   all defragmenting and reordering synchronously.  Delivery can be
   offloaded to another thread, and it remains to be seen which thread
   is best used for deserializing the data.

   The main advantage of restricting the adding of data to the buffer
   to the buffer's owning thread is that it allows us to simply append
   decoding information to the message as it becomes available while
   processing the message, without risking interference from another
   thread.  This includes decoded parameter lists/inline QoS settings,
   defragmenting information, &c.

   Once the synchronous processing of a message (a UDP packet) is
   completed, every adminstrative thing related to that message is
   contained in a single block of memory, and can be released very
   easily, regardless of whether the rbuf is a circular buffer, has a
   minimalistic heap inside it, or is simply discarded when the end is
   reached.

   Each rdata (submessage) that has been delivered (or need never be
   delivered) is not referenced anywhere and will therefore not
   contribute to rmsg::refcount, so once all rdatas of an rmsg have
   been delivered, rmsg::refcount will drop to 0.  If all submessages
   are processed by the receive thread, or delivery is delegated to
   other threads that happen to finish doing so before the receive
   thread is done processing the message, the message can be discarded
   trivially by not even updating the memory allocation info in the
   rbuf.

   Just creating an rdata is not sufficient reason for the reference
   count in the corresponding rmsg to be incremented: that happens
   once the defragmenter decides to not throw it away (either because
   it stores it or because it returns it for forwarding to reordering
   or delivery).  (Which is possible because both defragmentation and
   reordering are synchronous.)

   While synchronously processing the message, the reference count is
   biased by 2**31 just so we can detect some illegal activities.
   Furthermore, while still synchronous, each rdata contributes the
   number of actual references to the message plus 2**20 to the
   refcount.  This second bias allows delaying accounting for the
   actual references until after processing all reorder admins, saving
   us from having to update them potentially many times.

   The space needed for processing a message is limited: a UDP packet
   is never larger than 64kB (and it seems very unwise to actually use
   such large packets!), and there is only a finite amount of data
   that gets added to it while interpreting the message.  Although the
   exact amount is not yet known, it seems very unlikely that the
   decoding data for one packet would exceed 64kB size, though one had
   better be careful just in case.  So a maximum RMSG size of 128kB
   and an RBUF size of 1MB should be quite reasonable.

   Sequence of operations:

     receive_thread ()
     {
       ...
       rbpool = nn_rbufpool_new (1MB, 128kB)
       ...

       while ...
         rmsg = nn_rmsg_new (rbpool)
         actualsize = recvfrom (rmsg.payload, 64kB)
         nn_rmsg_setsize (rmsg, actualsize)
         process (rmsg)
         nn_rmsg_commit (rmsg)

       ... ensure no references to any buffer in rbpool exist ...
       nn_rbufpool_free (rbpool)
       ...
     }

   If there are no outstanding references to the message, commit()
   simply discards it and new() returns the same address next time
   round.

   Processing of a single message in process() is roughly as follows:

     for rdata in each Data/DataFrag submessage in rmsg
       sampleinfo.seq = XX;
       sampleinfo.fragsize = XX;
       sampleinfo.size = XX;
       sampleinfo.(others) = XX if first fragment, else not important
       sample = nn_defrag_rsample (pwr->defrag, rdata, &sampleinfo)
       if sample
         fragchain = nn_rsample_fragchain (sample)
         refcount_adjust = 0;

         if send-to-proxy-writer-reorder
           if nn_reorder_rsample (&sc, pwr->reorder, sample, &refcount_adjust)
              == DELIVER
             deliver-to-group (pwr, sc)
         else
           for (m in out-of-sync-reader-matches)
             sample' = nn_reorder_rsample_dup (rmsg, sample)
             if nn_reorder_rsample (&sc, m->reorder, sample, &refcount_adjust)
                == DELIVER
               deliver-to-reader (m->reader, sc)

         nn_fragchain_adjust_refcount (fragchain, refcount_adjust)
       fi
     rof

   Where deliver-to-x() must of course decrement refcounts after
   delivery when done, using nn_fragchain_unref().  See also REORDER
   for the subtleties of the refcount game.

   Note that there is an alternative to all this trickery with
   fragment chains and deserializing off these fragments chains:
   allocating sufficient memory upon reception of the first fragment,
   and then just memcpy'ing the bytes in, with a simple bitmask to
   keep track of which fragments have been received and which have not
   yet been.

   _The_ argument against that is a very unreliable network with huge
   messages: the way we do it here never needs more than a constant
   factor over what is actually received, whereas the simple
   alternative would blow up nearly instantaneously.  Maybe not if you
   drop samples halfway through defragmenting aggressively, but then
   you can't get anything through anymore if there are multiple
   writers.

   Gaps and Heartbeats prune the defragmenting index and are (when
   needed) stored as intervals of specially marked rdatas in the
   reordering indices.

   The procedure for a Gap is:

     for a Gap [a,b] in rmsg
       defrag_notegap (a, b+1)
       refcount_adjust = 0
       gap = nn_rdata_newgap (rmsg);
       if nn_reorder_gap (&sc, reorder, gap, a, b+1, &refcount_adjust)
         deliver-to-group (pwr, sc)
       for (m in out-of-sync-reader-matches)
         if nn_reorder_gap (&sc, m->reorder, gap, a, b+1, &refcount_adjust)
           deliver-to-reader (m->reader, sc)
       nn_fragchain_adjust_refcount (gap, refcount_adjust)

   Note that a Gap always gets processed both by the primary and by
   the secondary reorder admins.  This is because it covers a range.

   A heartbeat is similar, except that a heartbeat [a,b] results in a
   gap [1,a-1]. */

/* RBUFPOOL ------------------------------------------------------------ */

struct nn_rbufpool {
  /* An rbuf pool is owned by a receive thread, and that thread is the
     only allocating rmsgs from the rbufs in the pool. Any thread may
     be releasing buffers to the pool as they become empty.

     Currently, we only have maintain a current rbuf, which gets
     replaced when allocating a new one from it fails. Any rbufs that
     are released are freed completely if different from the current
     one.

     Could trivially be done lockless, except that it requires
     compare-and-swap, and we don't have that. But it hardly ever
     happens anyway. */
  os_mutex lock;
  struct nn_rbuf *current;
  uint32_t rbuf_size;
  uint32_t max_rmsg_size;
#ifndef NDEBUG
  /* Thread that owns this pool, so we can check that no other thread
     is calling functions only the owner may use. */
  os_threadId owner_tid;
#endif
};

static struct nn_rbuf *nn_rbuf_alloc_new (struct nn_rbufpool *rbufpool);
static void nn_rbuf_release (struct nn_rbuf *rbuf);

static uint32_t align8uint32 (uint32_t x)
{
  return (x + 7u) & (uint32_t)-8;
}

#ifndef NDEBUG
#define ASSERT_RBUFPOOL_OWNER(rbp) (assert (os_threadEqual (os_threadIdSelf (), (rbp)->owner_tid)))
#else
#define ASSERT_RBUFPOOL_OWNER(rbp) ((void) (0))
#endif

static uint32_t max_uint32 (uint32_t a, uint32_t b)
{
  return a >= b ? a : b;
}

static uint32_t max_rmsg_size_w_hdr (uint32_t max_rmsg_size)
{
  /* rbuf_alloc allocates max_rmsg_size, which is actually max
     _payload_ size (this is so 64kB max_rmsg_size always suffices for
     a UDP packet, regardless of internal structure).  We use it for
     nn_rmsg and nn_rmsg_chunk, but the difference in size is
     negligible really.  So in the interest of simplicity, we always
     allocate for the worst case, and may waste a few bytes here or
     there. */
  return
    max_uint32 ((uint32_t) offsetof (struct nn_rmsg, chunk.u.payload),
                (uint32_t) offsetof (struct nn_rmsg_chunk, u.payload))
    + max_rmsg_size;
}

struct nn_rbufpool *nn_rbufpool_new (uint32_t rbuf_size, uint32_t max_rmsg_size)
{
  struct nn_rbufpool *rbp;

  assert (max_rmsg_size > 0);
  assert (rbuf_size >= max_rmsg_size_w_hdr (max_rmsg_size));

  if ((rbp = os_malloc (sizeof (*rbp))) == NULL)
    goto fail_rbp;
#ifndef NDEBUG
  rbp->owner_tid = os_threadIdSelf ();
#endif

  os_mutexInit (&rbp->lock);

  rbp->rbuf_size = rbuf_size;
  rbp->max_rmsg_size = max_rmsg_size;

#if USE_VALGRIND
  VALGRIND_CREATE_MEMPOOL (rbp, 0, 0);
#endif

  if ((rbp->current = nn_rbuf_alloc_new (rbp)) == NULL)
    goto fail_rbuf;
  return rbp;

 fail_rbuf:
#if USE_VALGRIND
  VALGRIND_DESTROY_MEMPOOL (rbp);
#endif
  os_mutexDestroy (&rbp->lock);
  os_free (rbp);
 fail_rbp:
  return NULL;
}

void nn_rbufpool_setowner (UNUSED_ARG_NDEBUG (struct nn_rbufpool *rbp), UNUSED_ARG_NDEBUG (os_threadId tid))
{
#ifndef NDEBUG
  rbp->owner_tid = tid;
#endif
}

void nn_rbufpool_free (struct nn_rbufpool *rbp)
{
#if 0
  /* Anyone may free it: I want to be able to stop the receive
     threads, then stop all other asynchronous processing, then clear
     out the buffers.  That's is the only way to verify that the
     reference counts are all 0, as they should be. */
  ASSERT_RBUFPOOL_OWNER (rbp);
#endif
  nn_rbuf_release (rbp->current);
#if USE_VALGRIND
  VALGRIND_DESTROY_MEMPOOL (rbp);
#endif
  os_mutexDestroy (&rbp->lock);
  os_free (rbp);
}

/* RBUF ---------------------------------------------------------------- */

struct nn_rbuf {
  os_atomic_uint32_t n_live_rmsg_chunks;
  uint32_t size;
  uint32_t max_rmsg_size;
  struct nn_rbufpool *rbufpool;

  /* Allocating sequentially, releasing in random order, not bothering
     to reuse memory as soon as it becomes available again. I think
     this will have to change eventually, but this is the easiest
     approach.  Changes would be confined rmsg_new and rmsg_free. */
  unsigned char *freeptr;

  union {
    /* raw data array, nn_rbuf::size bytes long in reality */
    unsigned char raw[1];

    /* to ensure reasonable alignment of raw[] */
    int64_t l;
    double d;
    void *p;
  } u;
};

static struct nn_rbuf *nn_rbuf_alloc_new (struct nn_rbufpool *rbufpool)
{
  struct nn_rbuf *rb;
  ASSERT_RBUFPOOL_OWNER (rbufpool);

  if ((rb = os_malloc (offsetof (struct nn_rbuf, u.raw) + rbufpool->rbuf_size)) == NULL)
    return NULL;
#if USE_VALGRIND
  VALGRIND_MAKE_MEM_NOACCESS (rb->u.raw, rbufpool->rbuf_size);
#endif

  rb->rbufpool = rbufpool;
  os_atomic_st32 (&rb->n_live_rmsg_chunks, 1);
  rb->size = rbufpool->rbuf_size;
  rb->max_rmsg_size = rbufpool->max_rmsg_size;
  rb->freeptr = rb->u.raw;
  DDS_LOG(DDS_LC_RADMIN, "rbuf_alloc_new(%p) = %p\n", (void *) rbufpool, (void *) rb);
  return rb;
}

static struct nn_rbuf *nn_rbuf_new (struct nn_rbufpool *rbufpool)
{
  struct nn_rbuf *rb;
  assert (rbufpool->current);
  ASSERT_RBUFPOOL_OWNER (rbufpool);
  if ((rb = nn_rbuf_alloc_new (rbufpool)) != NULL)
  {
    os_mutexLock (&rbufpool->lock);
    nn_rbuf_release (rbufpool->current);
    rbufpool->current = rb;
    os_mutexUnlock (&rbufpool->lock);
  }
  return rb;
}

static void nn_rbuf_release (struct nn_rbuf *rbuf)
{
  struct nn_rbufpool *rbp = rbuf->rbufpool;
  DDS_LOG(DDS_LC_RADMIN, "rbuf_release(%p) pool %p current %p\n", (void *) rbuf, (void *) rbp, (void *) rbp->current);
  if (os_atomic_dec32_ov (&rbuf->n_live_rmsg_chunks) == 1)
  {
    DDS_LOG(DDS_LC_RADMIN, "rbuf_release(%p) free\n", (void *) rbuf);
    os_free (rbuf);
  }
}

/* RMSG ---------------------------------------------------------------- */

/* There are at most 64kB / 32B = 2**11 rdatas in one rmsg, because an
   rmsg is limited to 64kB and a Data submessage is at least 32B bytes
   in size.  With 1 bit taken for committed/uncommitted (needed for
   debugging purposes only), there's room for up to 2**20 out-of-sync
   readers matched to one proxy writer.  I believe it sufficiently
   unlikely that anyone will ever attempt to have 1 million readers on
   one node to one topic/partition ... */
#define RMSG_REFCOUNT_UNCOMMITTED_BIAS (1u << 31)
#define RMSG_REFCOUNT_RDATA_BIAS (1u << 20)
#ifndef NDEBUG
#define ASSERT_RMSG_UNCOMMITTED(rmsg) (assert (os_atomic_ld32 (&(rmsg)->refcount) >= RMSG_REFCOUNT_UNCOMMITTED_BIAS))
#else
#define ASSERT_RMSG_UNCOMMITTED(rmsg) ((void) 0)
#endif

static void *nn_rbuf_alloc (struct nn_rbufpool *rbufpool)
{
  /* Note: only one thread calls nn_rmsg_new on a pool */
  uint32_t asize = max_rmsg_size_w_hdr (rbufpool->max_rmsg_size);
  struct nn_rbuf *rb;
  DDS_LOG(DDS_LC_RADMIN, "rmsg_rbuf_alloc(%p, %u)\n", (void *) rbufpool, asize);
  ASSERT_RBUFPOOL_OWNER (rbufpool);
  rb = rbufpool->current;
  assert (rb != NULL);
  assert (rb->freeptr >= rb->u.raw);
  assert (rb->freeptr <= rb->u.raw + rb->size);

  if ((uint32_t) (rb->u.raw + rb->size - rb->freeptr) < asize)
  {
    /* not enough space left for new rmsg */
    if ((rb = nn_rbuf_new (rbufpool)) == NULL)
      return NULL;

    /* a new one should have plenty of space */
    assert ((uint32_t) (rb->u.raw + rb->size - rb->freeptr) >= asize);
  }

  DDS_LOG(DDS_LC_RADMIN, "rmsg_rbuf_alloc(%p, %u) = %p\n", (void *) rbufpool, asize, (void *) rb->freeptr);
#if USE_VALGRIND
  VALGRIND_MEMPOOL_ALLOC (rbufpool, rb->freeptr, asize);
#endif
  return rb->freeptr;
}

static void init_rmsg_chunk (struct nn_rmsg_chunk *chunk, struct nn_rbuf *rbuf)
{
  chunk->rbuf = rbuf;
  chunk->next = NULL;
  chunk->size = 0;
  os_atomic_inc32 (&rbuf->n_live_rmsg_chunks);
}

struct nn_rmsg *nn_rmsg_new (struct nn_rbufpool *rbufpool)
{
  /* Note: only one thread calls nn_rmsg_new on a pool */
  struct nn_rmsg *rmsg;
  DDS_LOG(DDS_LC_RADMIN, "rmsg_new(%p)\n", (void *) rbufpool);

  rmsg = nn_rbuf_alloc (rbufpool);
  if (rmsg == NULL)
    return NULL;

  /* Reference to this rmsg, undone by rmsg_commit(). */
  os_atomic_st32 (&rmsg->refcount, RMSG_REFCOUNT_UNCOMMITTED_BIAS);
  /* Initial chunk */
  init_rmsg_chunk (&rmsg->chunk, rbufpool->current);
  rmsg->lastchunk = &rmsg->chunk;
  /* Incrementing freeptr happens in commit(), so that discarding the
     message is really simple. */
  DDS_LOG(DDS_LC_RADMIN, "rmsg_new(%p) = %p\n", (void *) rbufpool, (void *) rmsg);
  return rmsg;
}

void nn_rmsg_setsize (struct nn_rmsg *rmsg, uint32_t size)
{
  uint32_t size8 = align8uint32 (size);
  DDS_LOG(DDS_LC_RADMIN, "rmsg_setsize(%p, %u => %u)\n", (void *) rmsg, size, size8);
  ASSERT_RBUFPOOL_OWNER (rmsg->chunk.rbuf->rbufpool);
  ASSERT_RMSG_UNCOMMITTED (rmsg);
  assert (os_atomic_ld32 (&rmsg->refcount) == RMSG_REFCOUNT_UNCOMMITTED_BIAS);
  assert (rmsg->chunk.size == 0);
  assert (size8 <= rmsg->chunk.rbuf->max_rmsg_size);
  assert (rmsg->lastchunk == &rmsg->chunk);
  rmsg->chunk.size = size8;
#if USE_VALGRIND
  VALGRIND_MEMPOOL_CHANGE (rmsg->chunk.rbuf->rbufpool, rmsg, rmsg, offsetof (struct nn_rmsg, chunk.u.payload) + rmsg->chunk.size);
#endif
}

void nn_rmsg_free (struct nn_rmsg *rmsg)
{
  /* Note: any thread may call rmsg_free.

     FIXME: note that we could optimise by moving rbuf->freeptr back
     in (the likely to be fairly normal) case free space follows this
     rmsg.  Except that that would require synchronising new() and
     free() which we don't do currently.  And ideally, you'd use
     compare-and-swap for this. */
  struct nn_rmsg_chunk *c;
  DDS_LOG(DDS_LC_RADMIN, "rmsg_free(%p)\n", (void *) rmsg);
  assert (os_atomic_ld32 (&rmsg->refcount) == 0);
  c = &rmsg->chunk;
  while (c)
  {
    struct nn_rbuf *rbuf = c->rbuf;
    struct nn_rmsg_chunk *c1 = c->next;
#if USE_VALGRIND
    if (c == &rmsg->chunk) {
      VALGRIND_MEMPOOL_FREE (rbuf->rbufpool, rmsg);
    } else {
      VALGRIND_MEMPOOL_FREE (rbuf->rbufpool, c);
    }
#endif
    assert (os_atomic_ld32 (&rbuf->n_live_rmsg_chunks) > 0);
    nn_rbuf_release (rbuf);
    c = c1;
  }
}

static void commit_rmsg_chunk (struct nn_rmsg_chunk *chunk)
{
  struct nn_rbuf *rbuf = chunk->rbuf;
  DDS_LOG(DDS_LC_RADMIN, "commit_rmsg_chunk(%p)\n", (void *) chunk);
  rbuf->freeptr = chunk->u.payload + chunk->size;
}

void nn_rmsg_commit (struct nn_rmsg *rmsg)
{
  /* Note: only one thread calls rmsg_commit -- the one that created
     it in the first place.

     If there are no outstanding references, we can simply reuse the
     memory.  This happens, e.g., when the message is invalid, doesn't
     contain anything processed asynchronously, or the scheduling
     happens to be such that any asynchronous activities have
     completed before we got to commit. */
  struct nn_rmsg_chunk *chunk = rmsg->lastchunk;
  DDS_LOG(DDS_LC_RADMIN, "rmsg_commit(%p) refcount 0x%x last-chunk-size %u\n",
                 (void *) rmsg, rmsg->refcount.v, chunk->size);
  ASSERT_RBUFPOOL_OWNER (chunk->rbuf->rbufpool);
  ASSERT_RMSG_UNCOMMITTED (rmsg);
  assert (chunk->size <= chunk->rbuf->max_rmsg_size);
  assert ((chunk->size % 8) == 0);
  assert (os_atomic_ld32 (&rmsg->refcount) >= RMSG_REFCOUNT_UNCOMMITTED_BIAS);
  assert (os_atomic_ld32 (&rmsg->chunk.rbuf->n_live_rmsg_chunks) > 0);
  assert (os_atomic_ld32 (&chunk->rbuf->n_live_rmsg_chunks) > 0);
  assert (chunk->rbuf->rbufpool->current == chunk->rbuf);
  if (os_atomic_sub32_nv (&rmsg->refcount, RMSG_REFCOUNT_UNCOMMITTED_BIAS) == 0)
    nn_rmsg_free (rmsg);
  else
  {
    /* Other references exist, so either stored in defrag, reorder
       and/or delivery queue */
    DDS_LOG(DDS_LC_RADMIN, "rmsg_commit(%p) => keep\n", (void *) rmsg);
    commit_rmsg_chunk (chunk);
  }
}

static void nn_rmsg_addbias (struct nn_rmsg *rmsg)
{
  /* Note: only the receive thread that owns the receive pool may
     increase the reference count, and only while it is still
     uncommitted.

     However, other threads (e.g., delivery threads) may have been
     triggered already, so the increment must be done atomically. */
  DDS_LOG(DDS_LC_RADMIN, "rmsg_addbias(%p)\n", (void *) rmsg);
  ASSERT_RBUFPOOL_OWNER (rmsg->chunk.rbuf->rbufpool);
  ASSERT_RMSG_UNCOMMITTED (rmsg);
  os_atomic_add32 (&rmsg->refcount, RMSG_REFCOUNT_RDATA_BIAS);
}

static void nn_rmsg_rmbias_and_adjust (struct nn_rmsg *rmsg, int adjust)
{
  /* This can happen to any rmsg referenced by an sample still
     progressing through the pipeline, but only by the receive
     thread.  Can't require it to be uncommitted. */
  uint32_t sub;
  DDS_LOG(DDS_LC_RADMIN, "rmsg_rmbias_and_adjust(%p, %d)\n", (void *) rmsg, adjust);
  ASSERT_RBUFPOOL_OWNER (rmsg->chunk.rbuf->rbufpool);
  assert (adjust >= 0);
  assert ((uint32_t) adjust < RMSG_REFCOUNT_RDATA_BIAS);
  sub = RMSG_REFCOUNT_RDATA_BIAS - (uint32_t) adjust;
  assert (os_atomic_ld32 (&rmsg->refcount) >= sub);
  if (os_atomic_sub32_nv (&rmsg->refcount, sub) == 0)
    nn_rmsg_free (rmsg);
}

static void nn_rmsg_rmbias_anythread (struct nn_rmsg *rmsg)
{
  /* For removing garbage when freeing a nn_defrag. */
  uint32_t sub = RMSG_REFCOUNT_RDATA_BIAS;
  DDS_LOG(DDS_LC_RADMIN, "rmsg_rmbias_anythread(%p)\n", (void *) rmsg);
  assert (os_atomic_ld32 (&rmsg->refcount) >= sub);
  if (os_atomic_sub32_nv (&rmsg->refcount, sub) == 0)
    nn_rmsg_free (rmsg);
}
static void nn_rmsg_unref (struct nn_rmsg *rmsg)
{
  DDS_LOG(DDS_LC_RADMIN, "rmsg_unref(%p)\n", (void *) rmsg);
  assert (os_atomic_ld32 (&rmsg->refcount) > 0);
  if (os_atomic_dec32_ov (&rmsg->refcount) == 1)
    nn_rmsg_free (rmsg);
}

void *nn_rmsg_alloc (struct nn_rmsg *rmsg, uint32_t size)
{
  struct nn_rmsg_chunk *chunk = rmsg->lastchunk;
  struct nn_rbuf *rbuf = chunk->rbuf;
  uint32_t size8 = align8uint32 (size);
  void *ptr;
  DDS_LOG(DDS_LC_RADMIN, "rmsg_alloc(%p, %u => %u)\n", (void *) rmsg, size, size8);
  ASSERT_RBUFPOOL_OWNER (rbuf->rbufpool);
  ASSERT_RMSG_UNCOMMITTED (rmsg);
  assert ((chunk->size % 8) == 0);
  assert (size8 <= rbuf->max_rmsg_size);

  if (chunk->size + size8 > rbuf->max_rmsg_size)
  {
    struct nn_rbufpool *rbufpool = rbuf->rbufpool;
    struct nn_rmsg_chunk *newchunk;
    DDS_LOG(DDS_LC_RADMIN, "rmsg_alloc(%p, %u) limit hit - new chunk\n", (void *) rmsg, size);
    commit_rmsg_chunk (chunk);
    newchunk = nn_rbuf_alloc (rbufpool);
    if (newchunk == NULL)
    {
      DDS_WARNING ("nn_rmsg_alloc: can't allocate more memory (%u bytes) ... giving up\n", size);
      return NULL;
    }
    init_rmsg_chunk (newchunk, rbufpool->current);
    rmsg->lastchunk = chunk->next = newchunk;
    chunk = newchunk;
  }

  ptr = chunk->u.payload + chunk->size;
  chunk->size += size8;
  DDS_LOG(DDS_LC_RADMIN, "rmsg_alloc(%p, %u) = %p\n", (void *) rmsg, size, ptr);
#if USE_VALGRIND
  if (chunk == &rmsg->chunk) {
    VALGRIND_MEMPOOL_CHANGE (rbuf->rbufpool, rmsg, rmsg, offsetof (struct nn_rmsg, chunk.u.payload) + chunk->size);
  } else {
    VALGRIND_MEMPOOL_CHANGE (rbuf->rbufpool, chunk, chunk, offsetof (struct nn_rmsg_chunk, u.payload) + chunk->size);
  }
#endif
  return ptr;
}

/* RDATA --------------------------------------- */

struct nn_rdata *nn_rdata_new (struct nn_rmsg *rmsg, uint32_t start, uint32_t endp1, uint32_t submsg_offset, uint32_t payload_offset)
{
  struct nn_rdata *d;
  if ((d = nn_rmsg_alloc (rmsg, sizeof (*d))) == NULL)
    return NULL;
  d->rmsg = rmsg;
  d->nextfrag = NULL;
  d->min = start;
  d->maxp1 = endp1;
  d->submsg_zoff = (uint16_t) NN_OFF_TO_ZOFF (submsg_offset);
  d->payload_zoff = (uint16_t) NN_OFF_TO_ZOFF (payload_offset);
#ifndef NDEBUG
  os_atomic_st32 (&d->refcount_bias_added, 0);
#endif
  DDS_LOG(DDS_LC_RADMIN, "rdata_new(%p, bytes [%u,%u), submsg @ %u, payload @ %u) = %p\n", (void *) rmsg, start, endp1, NN_RDATA_SUBMSG_OFF (d), NN_RDATA_PAYLOAD_OFF (d), (void *) d);
  return d;
}

static void nn_rdata_addbias (struct nn_rdata *rdata)
{
  DDS_LOG(DDS_LC_RADMIN, "rdata_addbias(%p)\n", (void *) rdata);
#ifndef NDEBUG
  ASSERT_RBUFPOOL_OWNER (rdata->rmsg->chunk.rbuf->rbufpool);
  if (os_atomic_inc32_nv (&rdata->refcount_bias_added) != 1)
    abort ();
#endif
  nn_rmsg_addbias (rdata->rmsg);
}

static void nn_rdata_rmbias_and_adjust (struct nn_rdata *rdata, int adjust)
{
  DDS_LOG(DDS_LC_RADMIN, "rdata_rmbias_and_adjust(%p, %d)\n", (void *) rdata, adjust);
#ifndef NDEBUG
  if (os_atomic_dec32_ov (&rdata->refcount_bias_added) != 1)
    abort ();
#endif
  nn_rmsg_rmbias_and_adjust (rdata->rmsg, adjust);
}

static void nn_rdata_rmbias_anythread (struct nn_rdata *rdata)
{
  DDS_LOG(DDS_LC_RADMIN, "rdata_rmbias_anythread(%p)\n", (void *) rdata);
#ifndef NDEBUG
  if (os_atomic_dec32_ov (&rdata->refcount_bias_added) != 1)
    abort ();
#endif
  nn_rmsg_rmbias_anythread (rdata->rmsg);
}

static void nn_rdata_unref (struct nn_rdata *rdata)
{
  DDS_LOG(DDS_LC_RADMIN, "rdata_rdata_unref(%p)\n", (void *) rdata);
  nn_rmsg_unref (rdata->rmsg);
}

/* DEFRAG --------------------------------------------------------------

   Defragmentation happens separately from reordering, the reason
   being that defragmentation really is best done only once, and
   besides it simplifies reordering because it only ever has to deal
   with whole messages.

   The defragmeter accepts both rdatas that are fragments of samples
   and rdatas that are complete samples.  The unfragmented ones are
   returned immediately for further processing, in the format also
   used for fragmented samples.  Any rdata stored in the defrag index
   as well as unfragmented ones returned immediately are accounted for
   in rmsg::refcount.

   Defragmenting one sample is done using an interval tree where the
   minima and maxima are given by byte indexes of the received
   framgents.  Consecutive frags get chained in one interval, to keep
   the tree small even in the worst case.

   These intervals are represented using defrag_iv, and the fragment
   chain for an interval is built using the nextfrag links in the
   rdata.

   The defragmenter can defragment multiple samples in parallel (even
   though a writer normally produces a single fragment chain only,
   things may be different when packets get lost and/or
   (transient-local) data is resent).

   Each sample is represented using an rsample.  Each contains the
   root of an interval tree of fragments with a cached pointer to the
   last known interval (because we expect the data to arrive in-order
   and like to avoid searching).  The rsamples are stored in a tree
   indexed on sequence number, which itself caches the last sample it
   is currently defragmenting, again to avoid searching.

   The memory for an rsample is later re-used by the reordering
   mechanism.  Hence the union.  For that use, see REORDER.

   Partial and complete overlap of fragments is acceptable, but may
   result in a fragment chain containing fragments that do not add any
   bytes of information.  Those should be skipped by the deserializer.
   If the sender decides to suddenly change the fragmentation for a
   message, we happily keep processing them, even though there is no
   good reason for the sender to do so and the likelihood of such
   messy fragment chains increases significantly.

   Once done defragmenting, the tree consists of a root node only,
   which points to a list of fragments, in-order (but for the caveat
   above).

   Memory used for the storage of interval nodes while defragmenting
   is afterward re-used for chaining samples.  An unfragmented message
   will have a new sample chain allocated for this purpose, a
   fragmented message will have at least one interval allocated to it
   and thus have sufficient space for the chain node.

   FIXME: These AVL trees are overkill.  Either switch to parent-less
   red-black trees (they have better performance anyway and only need
   a single bit of state) or to splay trees (must have a parent
   because they can degenerate to linear structures, unless the number
   of intervals in the tree is limited, which probably is a good idea
   anyway). */

struct nn_defrag_iv {
  ut_avlNode_t avlnode; /* for nn_rsample.defrag::fragtree */
  uint32_t min, maxp1;
  struct nn_rdata *first;
  struct nn_rdata *last;
};

struct nn_rsample {
  union {
    struct nn_rsample_defrag {
      ut_avlNode_t avlnode; /* for nn_defrag::sampletree */
      ut_avlTree_t fragtree;
      struct nn_defrag_iv *lastfrag;
      struct nn_rsample_info *sampleinfo;
      seqno_t seq;
    } defrag;
    struct nn_rsample_reorder {
      ut_avlNode_t avlnode;       /* for nn_reorder::sampleivtree, if head of a chain */
      struct nn_rsample_chain sc; /* this interval's samples, covering ... */
      seqno_t min, maxp1;        /* ... seq nos: [min,maxp1), but possibly with holes in it */
      uint32_t n_samples;        /* so this is the actual length of the chain */
    } reorder;
  } u;
};

struct nn_defrag {
  ut_avlTree_t sampletree;
  struct nn_rsample *max_sample; /* = max(sampletree) */
  uint32_t n_samples;
  uint32_t max_samples;
  enum nn_defrag_drop_mode drop_mode;
};

static int compare_uint32 (const void *va, const void *vb);
static int compare_seqno (const void *va, const void *vb);

static const ut_avlTreedef_t defrag_sampletree_treedef = UT_AVL_TREEDEF_INITIALIZER (offsetof (struct nn_rsample, u.defrag.avlnode), offsetof (struct nn_rsample, u.defrag.seq), compare_seqno, 0);
static const ut_avlTreedef_t rsample_defrag_fragtree_treedef = UT_AVL_TREEDEF_INITIALIZER (offsetof (struct nn_defrag_iv, avlnode), offsetof (struct nn_defrag_iv, min), compare_uint32, 0);

static int compare_uint32 (const void *va, const void *vb)
{
  uint32_t a = *((const uint32_t *) va);
  uint32_t b = *((const uint32_t *) vb);
  return (a == b) ? 0 : (a < b) ? -1 : 1;
}

static int compare_seqno (const void *va, const void *vb)
{
  seqno_t a = *((const seqno_t *) va);
  seqno_t b = *((const seqno_t *) vb);
  return (a == b) ? 0 : (a < b) ? -1 : 1;
}

struct nn_defrag *nn_defrag_new (enum nn_defrag_drop_mode drop_mode, uint32_t max_samples)
{
  struct nn_defrag *d;
  assert (max_samples >= 1);
  if ((d = os_malloc (sizeof (*d))) == NULL)
    return NULL;
  ut_avlInit (&defrag_sampletree_treedef, &d->sampletree);
  d->drop_mode = drop_mode;
  d->max_samples = max_samples;
  d->n_samples = 0;
  d->max_sample = NULL;
  return d;
}

void nn_fragchain_adjust_refcount (struct nn_rdata *frag, int adjust)
{
  struct nn_rdata *frag1;
  DDS_LOG(DDS_LC_RADMIN, "fragchain_adjust_refcount(%p, %d)\n", (void *) frag, adjust);
  while (frag)
  {
    frag1 = frag->nextfrag;
    nn_rdata_rmbias_and_adjust (frag, adjust);
    frag = frag1;
  }
}

static void nn_fragchain_rmbias_anythread (struct nn_rdata *frag, UNUSED_ARG (int adjust))
{
  struct nn_rdata *frag1;
  DDS_LOG(DDS_LC_RADMIN, "fragchain_rmbias_anythread(%p)\n", (void *) frag);
  while (frag)
  {
    frag1 = frag->nextfrag;
    nn_rdata_rmbias_anythread (frag);
    frag = frag1;
  }
}

static void defrag_rsample_drop (struct nn_defrag *defrag, struct nn_rsample *rsample, void (*fragchain_free) (struct nn_rdata *frag, int adjust))
{
  /* Can't reference rsample after the first fragchain_free, because
     we don't know which rdata/rmsg provides the storage for the
     rsample and therefore can't increment the reference count.

     So we need to walk the fragments while guaranteeing strict
     "forward progress" in the memory accesses, which this particular
     inorder treewalk does provide. */
  ut_avlIter_t iter;
  struct nn_defrag_iv *iv;
  DDS_LOG(DDS_LC_RADMIN, "  defrag_rsample_drop (%p, %p)\n", (void *) defrag, (void *) rsample);
  ut_avlDelete (&defrag_sampletree_treedef, &defrag->sampletree, rsample);
  assert (defrag->n_samples > 0);
  defrag->n_samples--;
  for (iv = ut_avlIterFirst (&rsample_defrag_fragtree_treedef, &rsample->u.defrag.fragtree, &iter); iv; iv = ut_avlIterNext (&iter))
    fragchain_free (iv->first, 0);
}

void nn_defrag_free (struct nn_defrag *defrag)
{
  struct nn_rsample *s;
  s = ut_avlFindMin (&defrag_sampletree_treedef, &defrag->sampletree);
  while (s)
  {
    DDS_LOG(DDS_LC_RADMIN, "defrag_free(%p, sample %p seq %"PRId64")\n", (void *) defrag, (void *) s, s->u.defrag.seq);
    defrag_rsample_drop (defrag, s, nn_fragchain_rmbias_anythread);
    s = ut_avlFindMin (&defrag_sampletree_treedef, &defrag->sampletree);
  }
  assert (defrag->n_samples == 0);
  os_free (defrag);
}

static int defrag_try_merge_with_succ (struct nn_rsample_defrag *sample, struct nn_defrag_iv *node)
{
  struct nn_defrag_iv *succ;

  DDS_LOG(DDS_LC_RADMIN, "  defrag_try_merge_with_succ(%p [%u..%u)):\n",
                 (void *) node, node->min, node->maxp1);
  if (node == sample->lastfrag)
  {
    /* there is no interval following node */
    DDS_LOG(DDS_LC_RADMIN, "  node is lastfrag\n");
    return 0;
  }

  succ = ut_avlFindSucc (&rsample_defrag_fragtree_treedef, &sample->fragtree, node);
  assert (succ != NULL);
  DDS_LOG(DDS_LC_RADMIN, "  succ is %p [%u..%u)\n", (void *) succ, succ->min, succ->maxp1);
  if (succ->min > node->maxp1)
  {
    DDS_LOG(DDS_LC_RADMIN, "  gap between node and succ\n");
    return 0;
  }
  else
  {
    uint32_t succ_maxp1 = succ->maxp1;

    /* no longer a gap between node & succ => succ will be removed
       from the interval tree and therefore node will become the
       last interval if succ currently is */
    ut_avlDelete (&rsample_defrag_fragtree_treedef, &sample->fragtree, succ);
    if (sample->lastfrag == succ)
    {
      DDS_LOG(DDS_LC_RADMIN, "  succ is lastfrag\n");
      sample->lastfrag = node;
    }

    /* If succ's chain contains data beyond the frag we just
       received, append it to node (but do note that this doesn't
       guarantee that each fragment in the chain adds data!) and
       throw succ away.

       Do the same if succ's frag chain is completely contained in
       node, even though it wastes memory & cpu time (the latter,
       eventually): because the rsample we use may be dependent on the
       references to rmsgs of the rdata in succ, freeing it may cause
       the rsample to be freed as well. */
    if (node->maxp1 < succ_maxp1)
      DDS_LOG(DDS_LC_RADMIN, "  succ adds data to node\n");
    else
      DDS_LOG(DDS_LC_RADMIN, "  succ is contained in node\n");

    node->last->nextfrag = succ->first;
    node->last = succ->last;
    node->maxp1 = succ_maxp1;

    /* if the new fragment contains data beyond succ it may even
       allow merging with succ-succ */
    return node->maxp1 > succ_maxp1;
  }
}

static void defrag_rsample_addiv (struct nn_rsample_defrag *sample, struct nn_rdata *rdata, ut_avlIPath_t *path)
{
  struct nn_defrag_iv *newiv;
  if ((newiv = nn_rmsg_alloc (rdata->rmsg, sizeof (*newiv))) == NULL)
    return;
  rdata->nextfrag = NULL;
  newiv->first = newiv->last = rdata;
  newiv->min = rdata->min;
  newiv->maxp1 = rdata->maxp1;
  nn_rdata_addbias (rdata);
  ut_avlInsertIPath (&rsample_defrag_fragtree_treedef, &sample->fragtree, newiv, path);
  if (sample->lastfrag == NULL || rdata->min > sample->lastfrag->min)
    sample->lastfrag = newiv;
}

static void rsample_init_common (UNUSED_ARG (struct nn_rsample *rsample), UNUSED_ARG (struct nn_rdata *rdata), UNUSED_ARG (const struct nn_rsample_info *sampleinfo))
{
}

static struct nn_rsample *defrag_rsample_new (struct nn_rdata *rdata, const struct nn_rsample_info *sampleinfo)
{
  struct nn_rsample *rsample;
  struct nn_rsample_defrag *dfsample;
  ut_avlIPath_t ivpath;

  if ((rsample = nn_rmsg_alloc (rdata->rmsg, sizeof (*rsample))) == NULL)
    return NULL;
  rsample_init_common (rsample, rdata, sampleinfo);
  dfsample = &rsample->u.defrag;
  dfsample->lastfrag = NULL;
  dfsample->seq = sampleinfo->seq;
  if ((dfsample->sampleinfo = nn_rmsg_alloc (rdata->rmsg, sizeof (*dfsample->sampleinfo))) == NULL)
    return NULL;
  *dfsample->sampleinfo = *sampleinfo;

  ut_avlInit (&rsample_defrag_fragtree_treedef, &dfsample->fragtree);

  /* add sentinel if rdata is not the first fragment of the message */
  if (rdata->min > 0)
  {
    struct nn_defrag_iv *sentinel;
    if ((sentinel = nn_rmsg_alloc (rdata->rmsg, sizeof (*sentinel))) == NULL)
      return NULL;
    sentinel->first = sentinel->last = NULL;
    sentinel->min = sentinel->maxp1 = 0;
    ut_avlLookupIPath (&rsample_defrag_fragtree_treedef, &dfsample->fragtree, &sentinel->min, &ivpath);
    ut_avlInsertIPath (&rsample_defrag_fragtree_treedef, &dfsample->fragtree, sentinel, &ivpath);
  }

  /* add an interval for the first received fragment */
  ut_avlLookupIPath (&rsample_defrag_fragtree_treedef, &dfsample->fragtree, &rdata->min, &ivpath);
  defrag_rsample_addiv (dfsample, rdata, &ivpath);
  return rsample;
}

static struct nn_rsample *reorder_rsample_new (struct nn_rdata *rdata, const struct nn_rsample_info *sampleinfo)
{
  /* Implements:

       defrag_rsample_new ; rsample_convert_defrag_to_reorder

     It is simple enough to warrant having an extra function. Note the
     discrepancy between defrag_rsample_new which fully initializes
     the rsample, including the AVL node headers, and this function,
     which doesn't do so. */
  struct nn_rsample *rsample;
  struct nn_rsample_reorder *s;
  struct nn_rsample_chain_elem *sce;

  if ((rsample = nn_rmsg_alloc (rdata->rmsg, sizeof (*rsample))) == NULL)
    return NULL;
  rsample_init_common (rsample, rdata, sampleinfo);

  if ((sce = nn_rmsg_alloc (rdata->rmsg, sizeof (*sce))) == NULL)
    return NULL;
  sce->fragchain = rdata;
  sce->next = NULL;
  if ((sce->sampleinfo = nn_rmsg_alloc (rdata->rmsg, sizeof (*sce->sampleinfo))) == NULL)
    return NULL;
  *sce->sampleinfo = *sampleinfo;
  rdata->nextfrag = NULL;
  nn_rdata_addbias (rdata);

  s = &rsample->u.reorder;
  s->min = sampleinfo->seq;
  s->maxp1 = sampleinfo->seq + 1;
  s->n_samples = 1;
  s->sc.first = s->sc.last = sce;
  return rsample;
}

static int is_complete (const struct nn_rsample_defrag *sample)
{
  /* Returns: NULL if 'sample' is incomplete, else 'sample'. Complete:
     one interval covering all bytes. One interval because of the
     greedy coalescing in add_fragment(). There is at least one
     interval if we get here. */
  const struct nn_defrag_iv *iv = ut_avlRoot (&rsample_defrag_fragtree_treedef, &sample->fragtree);
  assert (iv != NULL);
  if (iv->min == 0 && iv->maxp1 >= sample->sampleinfo->size)
  {
    /* Accept fragments containing data beyond the end of the sample,
       only to filter them out (or not, as the case may be) at a later
       stage. Dropping them before the defragmeter leaves us with
       samples that will never be completed; dropping them in the
       defragmenter would be feasible by discarding all fragments of
       that sample collected so far. */
    assert (ut_avlIsSingleton (&sample->fragtree));
    return 1;
  }
  else
  {
    return 0;
  }
}

static void rsample_convert_defrag_to_reorder (struct nn_rsample *sample)
{
  /* Converts an rsample as stored in defrag to one as stored in a
     reorder admin. Have to be careful with the ordering, or at least
     somewhat, and the easy way out uses a few local variables -- any
     self-respecting compiler will optimise them away, and any
     self-respecting CPU would need to copy them via registers anyway
     because it uses a load-store architecture. */
  struct nn_defrag_iv *iv = ut_avlRootNonEmpty (&rsample_defrag_fragtree_treedef, &sample->u.defrag.fragtree);
  struct nn_rdata *fragchain = iv->first;
  struct nn_rsample_info *sampleinfo = sample->u.defrag.sampleinfo;
  struct nn_rsample_chain_elem *sce;
  seqno_t seq = sample->u.defrag.seq;

  /* re-use memory fragment interval node for sample chain */
  sce = (struct nn_rsample_chain_elem *) ut_avlRootNonEmpty (&rsample_defrag_fragtree_treedef, &sample->u.defrag.fragtree);
  sce->fragchain = fragchain;
  sce->next = NULL;
  sce->sampleinfo = sampleinfo;

  sample->u.reorder.sc.first = sample->u.reorder.sc.last = sce;
  sample->u.reorder.min = seq;
  sample->u.reorder.maxp1 = seq + 1;
  sample->u.reorder.n_samples = 1;
}

static struct nn_rsample *defrag_add_fragment (struct nn_rsample *sample, struct nn_rdata *rdata, const struct nn_rsample_info *sampleinfo)
{
  struct nn_rsample_defrag *dfsample = &sample->u.defrag;
  struct nn_defrag_iv *predeq, *succ;
  const uint32_t min = rdata->min;
  const uint32_t maxp1 = rdata->maxp1;

  /* min, max are byte offsets; contents has max-min+1 bytes; it all
     concerns the message pointer to by sample */
  assert (min < maxp1);
  /* and it must concern this message */
  assert (dfsample);
  assert (dfsample->seq == sampleinfo->seq);
  /* there must be a last fragment */
  assert (dfsample->lastfrag);
  /* relatively expensive test: lastfrag, tree must be consistent */
  assert (dfsample->lastfrag == ut_avlFindMax (&rsample_defrag_fragtree_treedef, &dfsample->fragtree));

  DDS_LOG(DDS_LC_RADMIN, "  lastfrag %p [%u..%u)\n",
                 (void *) dfsample->lastfrag,
                 dfsample->lastfrag->min, dfsample->lastfrag->maxp1);

  /* Interval tree is sorted on min offset; each key is unique:
     otherwise one would be wholly contained in another. */
  if (min >= dfsample->lastfrag->min)
  {
    /* Assumed normal case: fragment appends data */
    predeq = dfsample->lastfrag;
    DDS_LOG(DDS_LC_RADMIN, "  fast path: predeq = lastfrag\n");
  }
  else
  {
    /* Slow path: find preceding fragment by tree search */
    predeq = ut_avlLookupPredEq (&rsample_defrag_fragtree_treedef, &dfsample->fragtree, &min);
    assert (predeq);
    DDS_LOG(DDS_LC_RADMIN, "  slow path: predeq = lookup %u => %p [%u..%u)\n",
                   min, (void *) predeq, predeq->min, predeq->maxp1);
  }

  /* we have a sentinel interval of [0,0) until we receive a packet
     that contains the first byte of the message, that is, there
     should always be predeq */
  assert (predeq != NULL);

  if (predeq->maxp1 >= maxp1)
  {
    /* new is contained in predeq, discard new; rdata did not cause
       completion of a sample */
    DDS_LOG(DDS_LC_RADMIN, "  new contained in predeq\n");
    return NULL;
  }
  else if (min <= predeq->maxp1)
  {
    /* new extends predeq, add it to the chain (necessarily at the
       end); this may close the gap to the successor of predeq; predeq
       need not have a fragment chain yet (it may be the sentinel) */
    DDS_LOG(DDS_LC_RADMIN, "  grow predeq with new\n");
    nn_rdata_addbias (rdata);
    rdata->nextfrag = NULL;
    if (predeq->first)
      predeq->last->nextfrag = rdata;
    else
    {
      /* 'Tis the sentinel => rewrite the sample info so we
         eventually always use the sample info contributed by the
         first fragment */
      predeq->first = rdata;
      *dfsample->sampleinfo = *sampleinfo;
    }
    predeq->last = rdata;
    predeq->maxp1 = maxp1;
    /* it may now be possible to merge with the successor */
    while (defrag_try_merge_with_succ (dfsample, predeq))
      ;
    return is_complete (dfsample) ? sample : NULL;
  }
  else if (predeq != dfsample->lastfrag && /* if predeq is last frag, there is no succ */
           (succ = ut_avlFindSucc (&rsample_defrag_fragtree_treedef, &dfsample->fragtree, predeq)) != NULL &&
           succ->min <= maxp1)
  {
    /* extends succ (at the low end; no guarantee each individual
       fragment in the chain adds value); but doesn't overlap with
       predeq so the tree structure doesn't change even though the key
       does change */
    DDS_LOG(DDS_LC_RADMIN, "  extending succ %p [%u..%u) at head\n",
                   (void *) succ, succ->min, succ->maxp1);
    nn_rdata_addbias (rdata);
    rdata->nextfrag = succ->first;
    succ->first = rdata;
    succ->min = min;
    /* new one may cover all of succ & more, in which case we must
       update the max of succ & see if we can merge it with
       succ-succ */
    if (maxp1 > succ->maxp1)
    {
      DDS_LOG(DDS_LC_RADMIN, "  extending succ at end as well\n");
      succ->maxp1 = maxp1;
      while (defrag_try_merge_with_succ (dfsample, succ))
        ;
    }
    assert (!is_complete (dfsample));
    return NULL;
  }
  else
  {
    /* doesn't extend either predeq at the end or succ at the head =>
       new interval; rdata did not cause completion of sample */
    ut_avlIPath_t path;
    DDS_LOG(DDS_LC_RADMIN, "  new interval\n");
    if (ut_avlLookupIPath (&rsample_defrag_fragtree_treedef, &dfsample->fragtree, &min, &path))
      assert (0);
    defrag_rsample_addiv (dfsample, rdata, &path);
    return NULL;
  }
}

static int nn_rdata_is_fragment (const struct nn_rdata *rdata, const struct nn_rsample_info *sampleinfo)
{
  /* sanity check: min, maxp1 must be within bounds */
  assert (rdata->min <= rdata->maxp1);
  assert (rdata->maxp1 <= sampleinfo->size);
  return !(rdata->min == 0 && rdata->maxp1 == sampleinfo->size);
}

static int defrag_limit_samples (struct nn_defrag *defrag, seqno_t seq, seqno_t *max_seq)
{
  struct nn_rsample *sample_to_drop = NULL;
  if (defrag->n_samples < defrag->max_samples)
    return 1;
  /* max_samples >= 1 => some sample present => max_sample != NULL */
  assert (defrag->max_sample != NULL);
  DDS_LOG(DDS_LC_RADMIN, "  max samples reached\n");
  switch (defrag->drop_mode)
  {
    case NN_DEFRAG_DROP_LATEST:
      DDS_LOG(DDS_LC_RADMIN, "  drop mode = DROP_LATEST\n");
      if (seq > defrag->max_sample->u.defrag.seq)
      {
        DDS_LOG(DDS_LC_RADMIN, "  new sample is new latest => discarding it\n");
        return 0;
      }
      sample_to_drop = defrag->max_sample;
      break;
    case NN_DEFRAG_DROP_OLDEST:
      DDS_LOG(DDS_LC_RADMIN, "  drop mode = DROP_OLDEST\n");
      sample_to_drop = ut_avlFindMin (&defrag_sampletree_treedef, &defrag->sampletree);
      assert (sample_to_drop);
      if (seq < sample_to_drop->u.defrag.seq)
      {
        DDS_LOG(DDS_LC_RADMIN, "  new sample is new oldest => discarding it\n");
        return 0;
      }
      break;
  }
  assert (sample_to_drop != NULL);
  defrag_rsample_drop (defrag, sample_to_drop, nn_fragchain_adjust_refcount);
  if (sample_to_drop == defrag->max_sample)
  {
    defrag->max_sample = ut_avlFindMax (&defrag_sampletree_treedef, &defrag->sampletree);
    *max_seq = defrag->max_sample ? defrag->max_sample->u.defrag.seq : 0;
    DDS_LOG(DDS_LC_RADMIN, "  updating max_sample: now %p %"PRId64"\n",
                   (void *) defrag->max_sample,
                   defrag->max_sample ? defrag->max_sample->u.defrag.seq : 0);
  }
  return 1;
}

struct nn_rsample *nn_defrag_rsample (struct nn_defrag *defrag, struct nn_rdata *rdata, const struct nn_rsample_info *sampleinfo)
{
  /* Takes an rdata, records it in defrag if needed and returns an
     rdata chain representing a complete message ready for further
     processing if 'rdata' is complete or caused a message to become
     complete.

     On return 'rdata' is either: (a) stored in defrag and the rmsg
     refcount is biased; (b) refcount is biased and sample returned
     immediately because it wasn't actually a fragment; or (c) no
     effect on refcount & and not stored because it did not add any
     information.

     on entry:

     - rdata not refcounted, chaining fields need not be initialized.

     - sampleinfo fully initialised if first frag, else just seq,
       fragsize and size; will be copied onto memory allocated from
       the receive buffer

     return: all rdatas referenced in the chain returned by this
     function have been accounted for in the refcount of their rmsgs
     by adding BIAS to the refcount. */
  struct nn_rsample *sample, *result;
  seqno_t max_seq;
  ut_avlIPath_t path;

  assert (defrag->n_samples <= defrag->max_samples);

  /* not a fragment => always complete, so refcount rdata, turn into a
     valid chain behind a valid msginfo and return it. */
  if (!nn_rdata_is_fragment (rdata, sampleinfo))
    return reorder_rsample_new (rdata, sampleinfo);

  /* max_seq is used for the fast path, and is 0 when there is no
     last message in 'defrag'. max_seq and max_sample must be
     consistent. Max_sample must be consistent with tree */
  assert (defrag->max_sample == ut_avlFindMax (&defrag_sampletree_treedef, &defrag->sampletree));
  max_seq = defrag->max_sample ? defrag->max_sample->u.defrag.seq : 0;
  DDS_LOG(DDS_LC_RADMIN, "defrag_rsample(%p, %p [%u..%u) msg %p, %p seq %"PRId64" size %u) max_seq %p %"PRId64":\n",
          (void *) defrag, (void *) rdata, rdata->min, rdata->maxp1, (void *) rdata->rmsg,
          (void *) sampleinfo, sampleinfo->seq, sampleinfo->size,
          (void *) defrag->max_sample, max_seq);
  /* fast path: rdata is part of message with the highest sequence
     number we're currently defragmenting, or is beyond that */
  if (sampleinfo->seq == max_seq)
  {
    DDS_LOG(DDS_LC_RADMIN, "  add fragment to max_sample\n");
    result = defrag_add_fragment (defrag->max_sample, rdata, sampleinfo);
  }
  else if (!defrag_limit_samples (defrag, sampleinfo->seq, &max_seq))
  {
    DDS_LOG(DDS_LC_RADMIN, "  discarding sample\n");
    result = NULL;
  }
  else if (sampleinfo->seq > max_seq)
  {
    /* a node with a key greater than the maximum always is the right
       child of the old maximum node */
    /* FIXME: MERGE THIS ONE WITH THE NEXT */
    DDS_LOG(DDS_LC_RADMIN, "  new max sample\n");
    ut_avlLookupIPath (&defrag_sampletree_treedef, &defrag->sampletree, &sampleinfo->seq, &path);
    if ((sample = defrag_rsample_new (rdata, sampleinfo)) == NULL)
      return NULL;
    ut_avlInsertIPath (&defrag_sampletree_treedef, &defrag->sampletree, sample, &path);
    defrag->max_sample = sample;
    defrag->n_samples++;
    result = NULL;
  }
  else if ((sample = ut_avlLookupIPath (&defrag_sampletree_treedef, &defrag->sampletree, &sampleinfo->seq, &path)) == NULL)
  {
    /* a new sequence number, but smaller than the maximum */
    DDS_LOG(DDS_LC_RADMIN, "  new sample less than max\n");
    assert (sampleinfo->seq < max_seq);
    if ((sample = defrag_rsample_new (rdata, sampleinfo)) == NULL)
      return NULL;
    ut_avlInsertIPath (&defrag_sampletree_treedef, &defrag->sampletree, sample, &path);
    defrag->n_samples++;
    result = NULL;
  }
  else
  {
    /* adds (or, as the case may be, doesn't add) to a known message */
    DDS_LOG(DDS_LC_RADMIN, "  add fragment to %p\n", (void *) sample);
    result = defrag_add_fragment (sample, rdata, sampleinfo);
  }

  if (result != NULL)
  {
    /* Once completed, remove from defrag sample tree and convert to
       reorder format. If it is the sample with the maximum sequence in
       the tree, an update of max_sample is required. */
    DDS_LOG(DDS_LC_RADMIN, "  complete\n");
    ut_avlDelete (&defrag_sampletree_treedef, &defrag->sampletree, result);
    assert (defrag->n_samples > 0);
    defrag->n_samples--;
    if (result == defrag->max_sample)
    {
      defrag->max_sample = ut_avlFindMax (&defrag_sampletree_treedef, &defrag->sampletree);
      DDS_LOG(DDS_LC_RADMIN, "  updating max_sample: now %p %"PRId64"\n",
              (void *) defrag->max_sample,
              defrag->max_sample ? defrag->max_sample->u.defrag.seq : 0);
    }
    rsample_convert_defrag_to_reorder (result);
  }

  assert (defrag->max_sample == ut_avlFindMax (&defrag_sampletree_treedef, &defrag->sampletree));
  return result;
}

void nn_defrag_notegap (struct nn_defrag *defrag, seqno_t min, seqno_t maxp1)
{
  /* All sequence numbers in [min,maxp1) are unavailable so any
     fragments in that range must be discarded.  Used both for
     Hearbeats (by setting min=1) and for Gaps. */
  struct nn_rsample *s = ut_avlLookupSuccEq (&defrag_sampletree_treedef, &defrag->sampletree, &min);
  while (s && s->u.defrag.seq < maxp1)
  {
    struct nn_rsample *s1 = ut_avlFindSucc (&defrag_sampletree_treedef, &defrag->sampletree, s);
    defrag_rsample_drop (defrag, s, nn_fragchain_adjust_refcount);
    s = s1;
  }
  defrag->max_sample = ut_avlFindMax (&defrag_sampletree_treedef, &defrag->sampletree);
}

int nn_defrag_nackmap (struct nn_defrag *defrag, seqno_t seq, uint32_t maxfragnum, struct nn_fragment_number_set *map, uint32_t maxsz)
{
  struct nn_rsample *s;
  struct nn_defrag_iv *iv;
  uint32_t i, fragsz, nfrags;
  assert (maxsz <= 256);
  s = ut_avlLookup (&defrag_sampletree_treedef, &defrag->sampletree, &seq);
  if (s == NULL)
  {
    if (maxfragnum == UINT32_MAX)
    {
      /* If neither the caller nor the defragmenter knows anything about the sample, say so */
      return -1;
    }
    else
    {
      /* If caller says fragments [0..maxfragnum] should be there, but
         we do not have a record of it, we can still generate a proper
         nackmap */
      if (maxfragnum + 1 > maxsz)
        map->numbits = maxsz;
      else
        map->numbits = maxfragnum + 1;
      map->bitmap_base = 0;
      nn_bitset_one (map->numbits, map->bits);
      return (int) map->numbits;
    }
  }

  /* Limit maxfragnum to actual sample size, so that the caller can
     get accurate info without knowing maxfragnum.  MAXFRAGNUM is
     0-based, so at most nfrags-1. */
  fragsz = s->u.defrag.sampleinfo->fragsize;
  nfrags = (s->u.defrag.sampleinfo->size + fragsz - 1) / fragsz;
  if (maxfragnum >= nfrags)
    maxfragnum = nfrags - 1;

  /* Determine bitmap start & size */
  {
    /* We always have an interval starting at 0, which is empty if we
       are missing the first fragment. */
    struct nn_defrag_iv *liv = s->u.defrag.lastfrag;
    nn_fragment_number_t map_end;
    iv = ut_avlFindMin (&rsample_defrag_fragtree_treedef, &s->u.defrag.fragtree);
    assert (iv != NULL);
    /* iv is first interval, iv->maxp1 is first byte beyond that =>
       divide by fragsz to get first missing fragment */
    map->bitmap_base = iv->maxp1 / fragsz;
    /* if last interval ends before the last published fragment and it
       isn't because the last fragment is shorter, bitmap runs to
       maxfragnum; else it can end where the last interval starts,
       i.e., (liv->min - 1) is the last byte missing of all that has
       been published so far */
    if (liv->maxp1 < (maxfragnum + 1) * fragsz && liv->maxp1 < s->u.defrag.sampleinfo->size)
      map_end = maxfragnum;
    else if (liv->min > 0)
      map_end = (liv->min - 1) / fragsz;
    else
      map_end = 0;
    /* if all data is available, iv == liv and map_end <
       map->bitmap_base, but there is nothing to request in that
       case. */
    map->numbits = (map_end < map->bitmap_base) ? 0 : map_end - map->bitmap_base + 1;
    iv = ut_avlFindSucc (&rsample_defrag_fragtree_treedef, &s->u.defrag.fragtree, iv);
  }

  /* Clear bitmap, then set bits for gaps in available fragments */
  if (map->numbits > maxsz)
    map->numbits = maxsz;
  nn_bitset_zero (map->numbits, map->bits);
  i = map->bitmap_base;
  while (iv && i < map->bitmap_base + map->numbits)
  {
    /* iv->min is the next available byte, therefore the first
       fragment we don't need to request a retransmission of */
    uint32_t bound = iv->min / fragsz;
    if ((iv->min % fragsz) != 0)
    {
      /* this is actually disallowed by the spec ... it can only occur
         when fragments are not always the same size for a single
         sample; but if & when it happens, simply request a fragment
         extra to cover everything up to iv->min. */
      ++bound;
    }
    for (; i < map->bitmap_base + map->numbits && i < bound; i++)
    {
      unsigned x = (unsigned) (i - map->bitmap_base);
      nn_bitset_set (map->numbits, map->bits, x);
    }
    /* next sequence of fragments to request retranmsission of starts
       at fragment containing maxp1 (because we don't have that byte
       yet), and runs until the next interval begins */
    i = iv->maxp1 / fragsz;
    iv = ut_avlFindSucc (&rsample_defrag_fragtree_treedef, &s->u.defrag.fragtree, iv);
  }
  /* and set bits for missing fragments beyond the highest interval */
  for (; i < map->bitmap_base + map->numbits; i++)
  {
    unsigned x = (unsigned) (i - map->bitmap_base);
    nn_bitset_set (map->numbits, map->bits, x);
  }
  return (int) map->numbits;
}

/* REORDER -------------------------------------------------------------

   The reorder index tracks out-of-order messages as non-overlapping,
   non-consecutive intervals of sequence numbers, with each interval
   pointing to a chain of rsamples (rsample_chain{,_elem}).  The
   maximum number of samples stored by the radmin is max_samples
   (setting it to 2**32-1 effectively makes it unlimited, by you're
   then you're probably into TB territority as you need at least an
   rmsg, rdata, sampleinfo, rsample, and a rsample_chain_elem, which
   adds up to quite a few bytes).

   The policy is to prefer the lowest sequence numbers, as those need
   to be delivered before the higher ones can be, and also because one
   radmin tracks only a single sequence.  Historical data uses a
   per-reader radmin.

   Each reliable proxy writer has a reorder admin for reordering
   messages, the "primary" reorder admin.  For the primary one, it is
   possible to store indexing data in memory originally allocated
   memory for defragmenting, as the defragmenter is done with it and
   this admin is the only one indexing the sample.

   Each out-of-sync proxy-writer--reader match also has an reorder
   instance, a "secondary" reorder admin, but those can't re-use
   memory like the proxy-writer's can, because there can be any number
   of them.  Before inserting in one of these, the sample must first
   be replicated using reorder_rsample_dup(), which fortunately is an
   extremely cheap operation.

   A sample either goes to the primary one (which may store it, reject
   it, or return it and subsequent samples immediately) [CASE I], or
   it goes to any number of secondary ones [CASE II].

   The reorder_rsample function may require updates to the reference
   counts of the rmsgs referenced by the rdatas in the sample it was
   called with (and _only_ to those of that particular sample, as
   others underwent all this processing before).  The
   "refcount_adjust" in/out parameter is updated to reflect the
   required change.

   A complicating factor is that after storing a sample in a reorder
   admin it potentially becomes part of a chain of samples, and may be
   located anywhere within that chain.  When that happens, the rsample
   parameter provided to reorder_rsample becomes useless for adjusting
   the reference counts as required.

   The initial reference count as it comes out of defragmentation is
   always BIAS-per-rdata, which means all rmgs referenced by the
   sample have refcount = BIAS if there is only ever a single sample
   in each rmsg.  (If multiple data submessages have been packed into
   a single message, they'll all contribute to the refcount.)

   The reference count adjustment is incremented by reorder_rsample
   whenever it stores or forwards the sample, and left unchanged when
   it rejects it (old samples & duplicates).  The initial reference
   needs to be accounted for as well, and so:

   - In [CASE I]: accept (or forward): +1 for accepting it, -BIAS for
     the initial reference, for a net change of 1-BIAS.  Reject: 0 for
     rejecting it, still -BIAS for the initial reference, for a net
     change of -BIAS.

   - In [CASE 2], each reorder admin gets its own copy of the sample,
     and therefore the sample that came out of defragmentation is
     unchanged, and may thus be used, regardless of the adjustment
     required.

     Accept by M out N: +M for accepting, 0 for the N-M rejects, -BIAS
     for the initial reference.  For a net change of M-BIAS.

   So in both cases, the adjustment needed is the number of reorder
   admins that accepted it, less BIAS for the initial reference.  We
   can't use the original sample because of [CASE I], so we adjust
   based on the fragment chain instead of the sample.  Example code is
   in the overview comment at the top of this file. */

struct nn_reorder {
  ut_avlTree_t sampleivtree;
  struct nn_rsample *max_sampleiv; /* = max(sampleivtree) */
  seqno_t next_seq;
  enum nn_reorder_mode mode;
  uint32_t max_samples;
  uint32_t n_samples;
};

static const ut_avlTreedef_t reorder_sampleivtree_treedef =
  UT_AVL_TREEDEF_INITIALIZER (offsetof (struct nn_rsample, u.reorder.avlnode), offsetof (struct nn_rsample, u.reorder.min), compare_seqno, 0);

struct nn_reorder *nn_reorder_new (enum nn_reorder_mode mode, uint32_t max_samples)
{
  struct nn_reorder *r;
  if ((r = os_malloc (sizeof (*r))) == NULL)
    return NULL;
  ut_avlInit (&reorder_sampleivtree_treedef, &r->sampleivtree);
  r->max_sampleiv = NULL;
  r->next_seq = 1;
  r->mode = mode;
  r->max_samples = max_samples;
  r->n_samples = 0;
  return r;
}

void nn_fragchain_unref (struct nn_rdata *frag)
{
  struct nn_rdata *frag1;
  while (frag)
  {
    frag1 = frag->nextfrag;
    nn_rdata_unref (frag);
    frag = frag1;
  }
}

void nn_reorder_free (struct nn_reorder *r)
{
  struct nn_rsample *iv;
  struct nn_rsample_chain_elem *sce;
  /* FXIME: instead of findmin/delete, a treewalk can be used. */
  iv = ut_avlFindMin (&reorder_sampleivtree_treedef, &r->sampleivtree);
  while (iv)
  {
    ut_avlDelete (&reorder_sampleivtree_treedef, &r->sampleivtree, iv);
    sce = iv->u.reorder.sc.first;
    while (sce)
    {
      struct nn_rsample_chain_elem *sce1 = sce->next;
      nn_fragchain_unref (sce->fragchain);
      sce = sce1;
    }
    iv = ut_avlFindMin (&reorder_sampleivtree_treedef, &r->sampleivtree);
  }
  os_free (r);
}

static void reorder_add_rsampleiv (struct nn_reorder *reorder, struct nn_rsample *rsample)
{
  ut_avlIPath_t path;
  if (ut_avlLookupIPath (&reorder_sampleivtree_treedef, &reorder->sampleivtree, &rsample->u.reorder.min, &path) != NULL)
    assert (0);
  ut_avlInsertIPath (&reorder_sampleivtree_treedef, &reorder->sampleivtree, rsample, &path);
}

#ifndef NDEBUG
static int rsample_is_singleton (const struct nn_rsample_reorder *s)
{
  assert (s->min < s->maxp1);
  if (s->n_samples != 1)
    return 0;
  assert (s->min + 1 == s->maxp1);
  assert (s->min + s->n_samples <= s->maxp1);
  assert (s->sc.first != NULL);
  assert (s->sc.first == s->sc.last);
  assert (s->sc.first->next == NULL);
  return 1;
}
#endif

static void append_rsample_interval (struct nn_rsample *a, struct nn_rsample *b)
{
  a->u.reorder.sc.last->next = b->u.reorder.sc.first;
  a->u.reorder.sc.last = b->u.reorder.sc.last;
  a->u.reorder.maxp1 = b->u.reorder.maxp1;
  a->u.reorder.n_samples += b->u.reorder.n_samples;
}

static int reorder_try_append_and_discard (struct nn_reorder *reorder, struct nn_rsample *appendto, struct nn_rsample *todiscard)
{
  if (todiscard == NULL)
  {
    DDS_LOG(DDS_LC_RADMIN, "  try_append_and_discard: fail: todiscard = NULL\n");
    return 0;
  }
  else if (appendto->u.reorder.maxp1 < todiscard->u.reorder.min)
  {
    DDS_LOG(DDS_LC_RADMIN, "  try_append_and_discard: fail: appendto = [%"PRId64",%"PRId64") @ %p, "
            "todiscard = [%"PRId64",%"PRId64") @ %p - gap\n",
            appendto->u.reorder.min, appendto->u.reorder.maxp1, (void *) appendto,
            todiscard->u.reorder.min, todiscard->u.reorder.maxp1, (void *) todiscard);
    return 0;
  }
  else
  {
    DDS_LOG(DDS_LC_RADMIN, "  try_append_and_discard: success: appendto = [%"PRId64",%"PRId64") @ %p, "
            "todiscard = [%"PRId64",%"PRId64") @ %p\n",
            appendto->u.reorder.min, appendto->u.reorder.maxp1, (void *) appendto,
            todiscard->u.reorder.min, todiscard->u.reorder.maxp1, (void *) todiscard);
    assert (todiscard->u.reorder.min == appendto->u.reorder.maxp1);
    ut_avlDelete (&reorder_sampleivtree_treedef, &reorder->sampleivtree, todiscard);
    append_rsample_interval (appendto, todiscard);
    DDS_LOG(DDS_LC_RADMIN, "  try_append_and_discard: max_sampleiv needs update? %s\n",
            (todiscard == reorder->max_sampleiv) ? "yes" : "no");
    /* Inform caller whether reorder->max must be updated -- the
       expected thing to do is to update it to appendto here, but that
       fails if appendto isn't actually in the tree.  And that happens
       to be the fast path where the sample that comes in has the
       sequence number we expected. */
    return todiscard == reorder->max_sampleiv;
  }
}

struct nn_rsample *nn_reorder_rsample_dup (struct nn_rmsg *rmsg, struct nn_rsample *rsampleiv)
{
  /* Duplicates the rsampleiv without updating any reference counts:
     that is left to the caller, as they do not need to be updated if
     the duplicate ultimately doesn't get used.

     The rmsg is the one to allocate from, and must be the one
     currently being processed (one can only allocate memory from an
     uncommitted rmsg) and must be referenced by an rdata in
     rsampleiv. */
  struct nn_rsample *rsampleiv_new;
  struct nn_rsample_chain_elem *sce;
  assert (rsample_is_singleton (&rsampleiv->u.reorder));
#ifndef NDEBUG
  {
    struct nn_rdata *d = rsampleiv->u.reorder.sc.first->fragchain;
    while (d && d->rmsg != rmsg)
      d = d->nextfrag;
    assert (d != NULL);
  }
#endif
  if ((rsampleiv_new = nn_rmsg_alloc (rmsg, sizeof (*rsampleiv_new))) == NULL)
    return NULL;
  if ((sce = nn_rmsg_alloc (rmsg, sizeof (*sce))) == NULL)
    return NULL;
  sce->fragchain = rsampleiv->u.reorder.sc.first->fragchain;
  sce->next = NULL;
  sce->sampleinfo = rsampleiv->u.reorder.sc.first->sampleinfo;
  *rsampleiv_new = *rsampleiv;
  rsampleiv_new->u.reorder.sc.first = rsampleiv_new->u.reorder.sc.last = sce;
  return rsampleiv_new;
}

struct nn_rdata *nn_rsample_fragchain (struct nn_rsample *rsample)
{
  assert (rsample_is_singleton (&rsample->u.reorder));
  return rsample->u.reorder.sc.first->fragchain;
}

static char reorder_mode_as_char (const struct nn_reorder *reorder)
{
  switch (reorder->mode)
  {
    case NN_REORDER_MODE_NORMAL: return 'R';
    case NN_REORDER_MODE_MONOTONICALLY_INCREASING: return 'U';
    case NN_REORDER_MODE_ALWAYS_DELIVER: return 'A';
  }
  assert (0);
  return '?';
}

static void delete_last_sample (struct nn_reorder *reorder)
{
  struct nn_rsample_reorder *last = &reorder->max_sampleiv->u.reorder;
  struct nn_rdata *fragchain;

  /* This just removes it, it doesn't adjust the count. It is not
     supposed to be called on an radmin with only one sample. */
  assert (reorder->n_samples > 0);
  assert (reorder->max_sampleiv != NULL);

  if (last->sc.first == last->sc.last)
  {
    /* Last sample is in an interval of its own - delete it, and
       recalc max_sampleiv. */
    DDS_LOG(DDS_LC_RADMIN, "  delete_last_sample: in singleton interval\n");
    fragchain = last->sc.first->fragchain;
    ut_avlDelete (&reorder_sampleivtree_treedef, &reorder->sampleivtree, reorder->max_sampleiv);
    reorder->max_sampleiv = ut_avlFindMax (&reorder_sampleivtree_treedef, &reorder->sampleivtree);
    /* No harm done if it the sampleivtree is empty, except that we
       chose not to allow it */
    assert (reorder->max_sampleiv != NULL);
  }
  else
  {
    /* Last sample is to be removed from the final interval.  Which
       requires scanning the sample chain because it is a
       singly-linked list (so you might not want max_samples set very
       large!).  Can't be a singleton list, so might as well chop off
       one evaluation of the loop condition. */
    struct nn_rsample_chain_elem *e, *pe;
    DDS_LOG(DDS_LC_RADMIN, "  delete_last_sample: scanning last interval [%"PRId64"..%"PRId64")\n",
            last->min, last->maxp1);
    assert (last->n_samples >= 1);
    assert (last->min + last->n_samples <= last->maxp1);
    e = last->sc.first;
    do {
      pe = e;
      e = e->next;
    } while (e != last->sc.last);
    fragchain = e->fragchain;
    pe->next = NULL;
    assert (pe->sampleinfo->seq + 1 < last->maxp1);
    last->sc.last = pe;
    last->maxp1--;
    last->n_samples--;
  }

  nn_fragchain_unref (fragchain);
}

nn_reorder_result_t nn_reorder_rsample (struct nn_rsample_chain *sc, struct nn_reorder *reorder, struct nn_rsample *rsampleiv, int *refcount_adjust, int delivery_queue_full_p)
{
  /* Adds an rsample (represented as an interval) to the reorder admin
     and returns the chain of consecutive samples ready for delivery
     because of the insertion.  Consequently, if it returns a sample
     chain, the sample referenced by rsampleiv is the first in the
     chain.

     refcount_adjust is incremented if the sample is not discarded. */
  struct nn_rsample_reorder *s = &rsampleiv->u.reorder;

  DDS_LOG(DDS_LC_RADMIN, "reorder_sample(%p %c, %"PRId64" @ %p) expecting %"PRId64":\n", (void *) reorder, reorder_mode_as_char (reorder), rsampleiv->u.reorder.min, (void *) rsampleiv, reorder->next_seq);

  /* Incoming rsample must be a singleton */
  assert (rsample_is_singleton (s));

  /* Reorder must not contain samples with sequence numbers <= next
     seq; max must be set iff the reorder is non-empty. */
#ifndef NDEBUG
  {
    struct nn_rsample *min = ut_avlFindMin (&reorder_sampleivtree_treedef, &reorder->sampleivtree);
    if (min)
      DDS_LOG(DDS_LC_RADMIN, "  min = %"PRId64" @ %p\n", min->u.reorder.min, (void *) min);
    assert (min == NULL || reorder->next_seq < min->u.reorder.min);
    assert ((reorder->max_sampleiv == NULL && min == NULL) ||
            (reorder->max_sampleiv != NULL && min != NULL));
  }
#endif
  assert ((!!ut_avlIsEmpty (&reorder->sampleivtree)) == (reorder->max_sampleiv == NULL));
  assert (reorder->max_sampleiv == NULL || reorder->max_sampleiv == ut_avlFindMax (&reorder_sampleivtree_treedef, &reorder->sampleivtree));
  assert (reorder->n_samples <= reorder->max_samples);
  if (reorder->max_sampleiv)
    DDS_LOG(DDS_LC_RADMIN, "  max = [%"PRId64",%"PRId64") @ %p\n", reorder->max_sampleiv->u.reorder.min, reorder->max_sampleiv->u.reorder.maxp1, (void *) reorder->max_sampleiv);

  if (s->min == reorder->next_seq ||
      (s->min > reorder->next_seq && reorder->mode == NN_REORDER_MODE_MONOTONICALLY_INCREASING) ||
      reorder->mode == NN_REORDER_MODE_ALWAYS_DELIVER)
  {
    /* Can deliver at least one sample, but that appends samples to
       the delivery queue.  If delivery_queue_full_p is set, the delivery
       queue has hit its maximum length, so appending to it isn't such
       a great idea.  Therefore, we simply reject the sample.  (We
       have to, we can't have a deliverable sample in the reorder
       admin, or things go wrong very quickly.) */
    if (delivery_queue_full_p)
    {
      DDS_LOG(DDS_LC_RADMIN, "  discarding deliverable sample: delivery queue is full\n");
      return NN_REORDER_REJECT;
    }

    /* 's' is next sample to be delivered; maybe we can append the
       first interval in the tree to it.  We can avoid all processing
       if the index is empty, which is the normal case.  Unreliable
       out-of-order either ends up here or in discard.)  */
    if (reorder->max_sampleiv != NULL)
    {
      struct nn_rsample *min = ut_avlFindMin (&reorder_sampleivtree_treedef, &reorder->sampleivtree);
      DDS_LOG(DDS_LC_RADMIN, "  try append_and_discard\n");
      if (reorder_try_append_and_discard (reorder, rsampleiv, min))
        reorder->max_sampleiv = NULL;
    }
    reorder->next_seq = s->maxp1;
    *sc = rsampleiv->u.reorder.sc;
    (*refcount_adjust)++;
    DDS_LOG(DDS_LC_RADMIN, "  return [%"PRId64",%"PRId64")\n", s->min, s->maxp1);

    /* Adjust reorder->n_samples, new sample is not counted yet */
    assert (s->maxp1 - s->min >= 1);
    assert (s->maxp1 - s->min <= (int) INT32_MAX);
    assert (s->min + s->n_samples <= s->maxp1);
    assert (reorder->n_samples >= s->n_samples - 1);
    reorder->n_samples -= s->n_samples - 1;
    return (nn_reorder_result_t) s->n_samples;
  }
  else if (s->min < reorder->next_seq)
  {
    /* we've moved beyond this one: discard it; no need to adjust
       n_samples */
    DDS_LOG(DDS_LC_RADMIN, "  discard: too old\n");
    return NN_REORDER_TOO_OLD; /* don't want refcount increment */
  }
  else if (ut_avlIsEmpty (&reorder->sampleivtree))
  {
    /* else, if nothing's stored simply add this one, max_samples = 0
       is technically allowed, and potentially useful, so check for
       it */
    assert (reorder->n_samples == 0);
    DDS_LOG(DDS_LC_RADMIN, "  adding to empty store\n");
    if (reorder->max_samples == 0)
    {
      DDS_LOG(DDS_LC_RADMIN, "  NOT - max_samples hit\n");
      return NN_REORDER_REJECT;
    }
    else
    {
      reorder_add_rsampleiv (reorder, rsampleiv);
      reorder->max_sampleiv = rsampleiv;
      reorder->n_samples++;
    }
  }
  else if (((void) assert (reorder->max_sampleiv != NULL)), (s->min == reorder->max_sampleiv->u.reorder.maxp1))
  {
    /* (sampleivtree not empty) <=> (max_sampleiv is non-NULL), for which there is an assert at the beginning but compilers and static analyzers don't all quite get that ... the somewhat crazy assert shuts up Clang's static analyzer */
    if (delivery_queue_full_p)
    {
      /* growing last inteval will not be accepted when this flag is set */
      DDS_LOG(DDS_LC_RADMIN, "  discarding sample: only accepting delayed samples due to backlog in delivery queue\n");
      return NN_REORDER_REJECT;
    }

    /* grow the last interval, if we're still accepting samples */
    DDS_LOG(DDS_LC_RADMIN, "  growing last interval\n");
    if (reorder->n_samples < reorder->max_samples)
    {
      append_rsample_interval (reorder->max_sampleiv, rsampleiv);
      reorder->n_samples++;
    }
    else
    {
       DDS_LOG(DDS_LC_RADMIN, "  discarding sample: max_samples reached and sample at end\n");
      return NN_REORDER_REJECT;
    }
  }
  else if (s->min > reorder->max_sampleiv->u.reorder.maxp1)
  {
    if (delivery_queue_full_p)
    {
      /* new interval at the end will not be accepted when this flag is set */
      DDS_LOG(DDS_LC_RADMIN, "  discarding sample: only accepting delayed samples due to backlog in delivery queue\n");
      return NN_REORDER_REJECT;
    }
    if (reorder->n_samples < reorder->max_samples)
    {
      DDS_LOG(DDS_LC_RADMIN, "  new interval at end\n");
      reorder_add_rsampleiv (reorder, rsampleiv);
      reorder->max_sampleiv = rsampleiv;
      reorder->n_samples++;
    }
    else
    {
      DDS_LOG(DDS_LC_RADMIN, "  discarding sample: max_samples reached and sample at end\n");
      return NN_REORDER_REJECT;
    }
  }
  else
  {
    /* lookup interval predeq=[m,n) s.t. m <= s->min and
       immsucc=[m',n') s.t. m' = s->maxp1:

       - if m <= s->min < n we discard it (duplicate)
       - if n=s->min we can append s to predeq
       - if immsucc exists we can prepend s to immsucc
       - and possibly join predeq, s, and immsucc */
    struct nn_rsample *predeq, *immsucc;
    DDS_LOG(DDS_LC_RADMIN, "  hard case ...\n");

    if (config.late_ack_mode && delivery_queue_full_p)
    {
      DDS_LOG(DDS_LC_RADMIN, "  discarding sample: delivery queue full\n");
      return NN_REORDER_REJECT;
    }

    predeq = ut_avlLookupPredEq (&reorder_sampleivtree_treedef, &reorder->sampleivtree, &s->min);
    if (predeq)
      DDS_LOG(DDS_LC_RADMIN, "  predeq = [%"PRId64",%"PRId64") @ %p\n",
              predeq->u.reorder.min, predeq->u.reorder.maxp1, (void *) predeq);
    else
      DDS_LOG(DDS_LC_RADMIN, "  predeq = null\n");
    if (predeq && s->min >= predeq->u.reorder.min && s->min < predeq->u.reorder.maxp1)
    {
      /* contained in predeq */
      DDS_LOG(DDS_LC_RADMIN, "  discard: contained in predeq\n");
      return NN_REORDER_REJECT;
    }

    immsucc = ut_avlLookup (&reorder_sampleivtree_treedef, &reorder->sampleivtree, &s->maxp1);
    if (immsucc)
      DDS_LOG(DDS_LC_RADMIN, "  immsucc = [%"PRId64",%"PRId64") @ %p\n",
              immsucc->u.reorder.min, immsucc->u.reorder.maxp1, (void *) immsucc);
    else
      DDS_LOG(DDS_LC_RADMIN, "  immsucc = null\n");
    if (predeq && s->min == predeq->u.reorder.maxp1)
    {
      /* grow predeq at end, and maybe append immsucc as well */
      DDS_LOG(DDS_LC_RADMIN, "  growing predeq at end ...\n");
      append_rsample_interval (predeq, rsampleiv);
      if (reorder_try_append_and_discard (reorder, predeq, immsucc))
        reorder->max_sampleiv = predeq;
    }
    else if (immsucc)
    {
      /* no predecessor, grow immsucc at head, which _does_ alter the
         key of the node in the tree, but _doesn't_ change the tree's
         structure. */
      DDS_LOG(DDS_LC_RADMIN, "  growing immsucc at head\n");
      s->sc.last->next = immsucc->u.reorder.sc.first;
      immsucc->u.reorder.sc.first = s->sc.first;
      immsucc->u.reorder.min = s->min;
      immsucc->u.reorder.n_samples += s->n_samples;

      /* delete_last_sample may eventually decide to delete the last
         sample contained in immsucc without checking whether immsucc
         were allocated dependent on that sample.  That in turn would
         cause sampleivtree to point to freed memory (either freed as
         in free(), or freed as in available for reuse, and hence the
         result may be a silent corruption of the interval tree).

         We do know that rsampleiv will remain live, that it is not
         dependent on the last sample (because we're growing immsucc
         at the head), and that we don't otherwise need it anymore.
         Therefore, we can swap rsampleiv in for immsucc and avoid the
         case above. */
      rsampleiv->u.reorder = immsucc->u.reorder;
      ut_avlSwapNode (&reorder_sampleivtree_treedef, &reorder->sampleivtree, immsucc, rsampleiv);
      if (immsucc == reorder->max_sampleiv)
        reorder->max_sampleiv = rsampleiv;
    }
    else
    {
      /* neither extends predeq nor immsucc */
      DDS_LOG(DDS_LC_RADMIN, "  new interval\n");
      reorder_add_rsampleiv (reorder, rsampleiv);
    }

    /* do not let radmin grow beyond max_samples; now that we've
       inserted it (and possibly have grown the radmin beyond its max
       size), we no longer risk deleting the interval that the new
       sample belongs to when deleting the last sample. */
    if (reorder->n_samples < reorder->max_samples)
      reorder->n_samples++;
    else
    {
      delete_last_sample (reorder);
    }
  }

  (*refcount_adjust)++;
  return NN_REORDER_ACCEPT;
}

static struct nn_rsample *coalesce_intervals_touching_range (struct nn_reorder *reorder, seqno_t min, seqno_t maxp1, int *valuable)
{
  struct nn_rsample *s, *t;
  *valuable = 0;
  /* Find first (lowest m) interval [m,n) s.t. n >= min && m <= maxp1 */
  s = ut_avlLookupPredEq (&reorder_sampleivtree_treedef, &reorder->sampleivtree, &min);
  if (s && s->u.reorder.maxp1 >= min)
  {
    /* m <= min && n >= min (note: pred of s [m',n') necessarily has n' < m) */
#ifndef NDEBUG
    struct nn_rsample *q = ut_avlFindPred (&reorder_sampleivtree_treedef, &reorder->sampleivtree, s);
    assert (q == NULL || q->u.reorder.maxp1 < min);
#endif
  }
  else
  {
    /* No good, but the first (if s = NULL) or the next one (if s !=
       NULL) may still have m <= maxp1 (m > min is implied now).  If
       not, no such interval.  */
    s = ut_avlFindSucc (&reorder_sampleivtree_treedef, &reorder->sampleivtree, s);
    if (!(s && s->u.reorder.min <= maxp1))
      return NULL;
  }
  /* Append successors [m',n') s.t. m' <= maxp1 to s */
  assert (s->u.reorder.min + s->u.reorder.n_samples <= s->u.reorder.maxp1);
  while ((t = ut_avlFindSucc (&reorder_sampleivtree_treedef, &reorder->sampleivtree, s)) != NULL && t->u.reorder.min <= maxp1)
  {
    ut_avlDelete (&reorder_sampleivtree_treedef, &reorder->sampleivtree, t);
    assert (t->u.reorder.min + t->u.reorder.n_samples <= t->u.reorder.maxp1);
    append_rsample_interval (s, t);
    *valuable = 1;
  }
  /* If needed, grow range to [min,maxp1) */
  if (min < s->u.reorder.min)
  {
    *valuable = 1;
    s->u.reorder.min = min;
  }
  if (maxp1 > s->u.reorder.maxp1)
  {
    *valuable = 1;
    s->u.reorder.maxp1 = maxp1;
  }
  return s;
}

struct nn_rdata *nn_rdata_newgap (struct nn_rmsg *rmsg)
{
  struct nn_rdata *d;
  if ((d = nn_rdata_new (rmsg, 0, 0, 0, 0)) == NULL)
    return NULL;
  nn_rdata_addbias (d);
  return d;
}

static int reorder_insert_gap (struct nn_reorder *reorder, struct nn_rdata *rdata, seqno_t min, seqno_t maxp1)
{
  struct nn_rsample_chain_elem *sce;
  struct nn_rsample *s;
  ut_avlIPath_t path;
  if (ut_avlLookupIPath (&reorder_sampleivtree_treedef, &reorder->sampleivtree, &min, &path) != NULL)
    assert (0);
  if ((sce = nn_rmsg_alloc (rdata->rmsg, sizeof (*sce))) == NULL)
    return 0;
  sce->fragchain = rdata;
  sce->next = NULL;
  sce->sampleinfo = NULL;
  if ((s = nn_rmsg_alloc (rdata->rmsg, sizeof (*s))) == NULL)
    return 0;
  s->u.reorder.sc.first = s->u.reorder.sc.last = sce;
  s->u.reorder.min = min;
  s->u.reorder.maxp1 = maxp1;
  s->u.reorder.n_samples = 1;
  ut_avlInsertIPath (&reorder_sampleivtree_treedef, &reorder->sampleivtree, s, &path);
  return 1;
}

nn_reorder_result_t nn_reorder_gap (struct nn_rsample_chain *sc, struct nn_reorder *reorder, struct nn_rdata *rdata, seqno_t min, seqno_t maxp1, int *refcount_adjust)
{
  /* All sequence numbers in [min,maxp1) are unavailable so any
     fragments in that range must be discarded.  Used both for
     Hearbeats (by setting min=1) and for Gaps.

       Case I: maxp1 <= next_seq.  No effect whatsoever.

     Otherwise:

       Case II: min <= next_seq.  All samples we have with sequence
         numbers less than maxp1 plus those following it consecutively
         are returned, and next_seq is updated to max(maxp1, highest
         returned sequence number+1)

     Else:

       Case III: Causes coalescing of intervals overlapping with
         [min,maxp1) or consecutive to it, possibly extending
         intervals to min on the lower bound or maxp1 on the upper
         one, or if there are no such intervals, the creation of a
         [min,maxp1) interval without any samples.

     NOTE: must not store anything (i.e. modify rdata,
     refcount_adjust) if gap causes data to be delivered: altnerative
     path for out-of-order delivery if all readers of a reliable
     proxy-writer are unrelibale depends on it. */
  struct nn_rsample *coalesced;
  int valuable;

  DDS_LOG(DDS_LC_RADMIN, "reorder_gap(%p %c, [%"PRId64",%"PRId64") data %p) expecting %"PRId64":\n",
                 (void *) reorder, reorder_mode_as_char (reorder),
                 min, maxp1, (void *) rdata, reorder->next_seq);

  if (maxp1 <= reorder->next_seq)
  {
    DDS_LOG(DDS_LC_RADMIN, "  too old\n");
    return NN_REORDER_TOO_OLD;
  }
  if (reorder->mode != NN_REORDER_MODE_NORMAL)
  {
    DDS_LOG(DDS_LC_RADMIN, "  special mode => don't care\n");
    return NN_REORDER_REJECT;
  }

  /* Coalesce all intervals [m,n) with n >= min or m <= maxp1 */
  if ((coalesced = coalesce_intervals_touching_range (reorder, min, maxp1, &valuable)) == NULL)
  {
    nn_reorder_result_t res;
    DDS_LOG(DDS_LC_RADMIN, "  coalesced = null\n");
    if (min <= reorder->next_seq)
    {
      DDS_LOG(DDS_LC_RADMIN, "  next expected: %"PRId64"\n", maxp1);
      reorder->next_seq = maxp1;
      res = NN_REORDER_ACCEPT;
    }
    else if (reorder->n_samples == reorder->max_samples &&
             (reorder->max_sampleiv == NULL || min > reorder->max_sampleiv->u.reorder.maxp1))
    {
      /* n_samples = max_samples => (max_sampleiv = NULL <=> max_samples = 0) */
      DDS_LOG(DDS_LC_RADMIN, "  discarding gap: max_samples reached and gap at end\n");
      res = NN_REORDER_REJECT;
    }
    else if (!reorder_insert_gap (reorder, rdata, min, maxp1))
    {
      DDS_LOG(DDS_LC_RADMIN, "  store gap failed: no memory\n");
      res = NN_REORDER_REJECT;
    }
    else
    {
      DDS_LOG(DDS_LC_RADMIN, "  storing gap\n");
      res = NN_REORDER_ACCEPT;
      /* do not let radmin grow beyond max_samples; there is a small
         possibility that we insert it & delete it immediately
         afterward. */
      if (reorder->n_samples < reorder->max_samples)
        reorder->n_samples++;
      else
        delete_last_sample (reorder);
      (*refcount_adjust)++;
    }
    reorder->max_sampleiv = ut_avlFindMax (&reorder_sampleivtree_treedef, &reorder->sampleivtree);
    return res;
  }
  else if (coalesced->u.reorder.min <= reorder->next_seq)
  {
    DDS_LOG(DDS_LC_RADMIN, "  coalesced = [%"PRId64",%"PRId64") @ %p containing %d samples\n",
            coalesced->u.reorder.min, coalesced->u.reorder.maxp1,
            (void *) coalesced, coalesced->u.reorder.n_samples);
    ut_avlDelete (&reorder_sampleivtree_treedef, &reorder->sampleivtree, coalesced);
    if (coalesced->u.reorder.min <= reorder->next_seq)
      assert (min <= reorder->next_seq);
    reorder->next_seq = coalesced->u.reorder.maxp1;
    reorder->max_sampleiv = ut_avlFindMax (&reorder_sampleivtree_treedef, &reorder->sampleivtree);
    DDS_LOG(DDS_LC_RADMIN, "  next expected: %"PRId64"\n", reorder->next_seq);
    *sc = coalesced->u.reorder.sc;

    /* Adjust n_samples */
    assert (coalesced->u.reorder.min + coalesced->u.reorder.n_samples <= coalesced->u.reorder.maxp1);
    assert (reorder->n_samples >= coalesced->u.reorder.n_samples);
    reorder->n_samples -= coalesced->u.reorder.n_samples;
    return (nn_reorder_result_t) coalesced->u.reorder.n_samples;
  }
  else
  {
    DDS_LOG(DDS_LC_RADMIN, "  coalesced = [%"PRId64",%"PRId64") @ %p - that is all\n",
            coalesced->u.reorder.min, coalesced->u.reorder.maxp1, (void *) coalesced);
    reorder->max_sampleiv = ut_avlFindMax (&reorder_sampleivtree_treedef, &reorder->sampleivtree);
    return valuable ? NN_REORDER_ACCEPT : NN_REORDER_REJECT;
  }
}

int nn_reorder_wantsample (struct nn_reorder *reorder, seqno_t seq)
{
  struct nn_rsample *s;
  if (seq < reorder->next_seq)
    /* trivially not interesting */
    return 0;
  /* Find interval that contains seq, if we know seq.  We are
     interested if seq is outside this interval (if any). */
  s = ut_avlLookupPredEq (&reorder_sampleivtree_treedef, &reorder->sampleivtree, &seq);
  return (s == NULL || s->u.reorder.maxp1 <= seq);
}

unsigned nn_reorder_nackmap (struct nn_reorder *reorder, seqno_t base, seqno_t maxseq, struct nn_sequence_number_set *map, uint32_t maxsz, int notail)
{
  struct nn_rsample *iv;
  seqno_t i;

  /* reorder->next_seq-1 is the last one we delivered, so the last one
     we ack; maxseq is the latest sample we know exists.  Valid bitmap
     lengths are 1 .. 256, so maxsz must be within that range, except
     that we allow length-0 bitmaps here as well.  Map->numbits is
     bounded by max(based on sequence numbers, maxsz). */
  assert (maxsz <= 256);
  /* not much point in requesting more data than we're willing to store
     (it would be ok if we knew we'd be able to keep up) */
  if (maxsz > reorder->max_samples)
    maxsz = reorder->max_samples;
#if 0
  /* this is what it used to be, where the reorder buffer is with
     delivery */
  base = reorder->next_seq;
#else
  if (base > reorder->next_seq)
  {
    DDS_ERROR("nn_reorder_nackmap: incorrect base sequence number supplied (%"PRId64" > %"PRId64")\n", base, reorder->next_seq);
    base = reorder->next_seq;
  }
#endif
  if (maxseq + 1 < base)
  {
    DDS_ERROR("nn_reorder_nackmap: incorrect max sequence number supplied (maxseq %"PRId64" base %"PRId64")\n", maxseq, base);
    maxseq = base - 1;
  }

  map->bitmap_base = toSN (base);
  if (maxseq + 1 - base > maxsz)
    map->numbits = maxsz;
  else
    map->numbits = (uint32_t) (maxseq + 1 - base);
  nn_bitset_zero (map->numbits, map->bits);

  if ((iv = ut_avlFindMin (&reorder_sampleivtree_treedef, &reorder->sampleivtree)) != NULL)
    assert (iv->u.reorder.min > base);
  i = base;
  while (iv && i < base + map->numbits)
  {
    for (; i < base + map->numbits && i < iv->u.reorder.min; i++)
    {
      unsigned x = (unsigned) (i - base);
      nn_bitset_set (map->numbits, map->bits, x);
    }
    i = iv->u.reorder.maxp1;
    iv = ut_avlFindSucc (&reorder_sampleivtree_treedef, &reorder->sampleivtree, iv);
  }
  if (notail && i < base + map->numbits)
    map->numbits = (unsigned) (i - base);
  else
  {
    for (; i < base + map->numbits; i++)
    {
      unsigned x = (unsigned) (i - base);
      nn_bitset_set (map->numbits, map->bits, x);
    }
  }
  return map->numbits;
}

seqno_t nn_reorder_next_seq (const struct nn_reorder *reorder)
{
  return reorder->next_seq;
}

/* DQUEUE -------------------------------------------------------------- */

struct nn_dqueue {
  os_mutex lock;
  os_cond cond;
  nn_dqueue_handler_t handler;
  void *handler_arg;

  struct nn_rsample_chain sc;

  struct thread_state1 *ts;
  char *name;
  uint32_t max_samples;
  os_atomic_uint32_t nof_samples;
};

enum dqueue_elem_kind {
  DQEK_DATA,
  DQEK_GAP,
  DQEK_BUBBLE
};

enum nn_dqueue_bubble_kind {
  NN_DQBK_STOP, /* _not_ os_malloc()ed! */
  NN_DQBK_CALLBACK,
  NN_DQBK_RDGUID
};

struct nn_dqueue_bubble {
  /* sample_chain_elem must be first: and is used to link it into the
     queue, with the sampleinfo pointing to itself, but mangled */
  struct nn_rsample_chain_elem sce;

  enum nn_dqueue_bubble_kind kind;
  union {
    /* stop */
    struct {
      nn_dqueue_callback_t cb;
      void *arg;
    } cb;
    struct {
      nn_guid_t rdguid;
      uint32_t count;
    } rdguid;
  } u;
};

static enum dqueue_elem_kind dqueue_elem_kind (const struct nn_rsample_chain_elem *e)
{
  if (e->sampleinfo == NULL)
    return DQEK_GAP;
  else if ((char *) e->sampleinfo != (char *) e)
    return DQEK_DATA;
  else
    return DQEK_BUBBLE;
}

static uint32_t dqueue_thread (struct nn_dqueue *q)
{
  struct thread_state1 *self = lookup_thread_state ();
  nn_mtime_t next_thread_cputime = { 0 };
  int keepgoing = 1;
  nn_guid_t rdguid, *prdguid = NULL;
  uint32_t rdguid_count = 0;

  os_mutexLock (&q->lock);
  while (keepgoing)
  {
    struct nn_rsample_chain sc;

    LOG_THREAD_CPUTIME (next_thread_cputime);

    if (q->sc.first == NULL)
      os_condWait (&q->cond, &q->lock);
    sc = q->sc;
    q->sc.first = q->sc.last = NULL;
    os_mutexUnlock (&q->lock);

    while (sc.first)
    {
      struct nn_rsample_chain_elem *e = sc.first;
      int ret;
      sc.first = e->next;
      if (os_atomic_dec32_ov (&q->nof_samples) == 1) {
        os_condBroadcast (&q->cond);
      }
      thread_state_awake (self);
      switch (dqueue_elem_kind (e))
      {
        case DQEK_DATA:
          ret = q->handler (e->sampleinfo, e->fragchain, prdguid, q->handler_arg);
          (void) ret; /* eliminate set-but-not-used in NDEBUG case */
          assert (ret == 0); /* so every handler will return 0 */
          /* FALLS THROUGH */
        case DQEK_GAP:
          nn_fragchain_unref (e->fragchain);
          if (rdguid_count > 0)
          {
            if (--rdguid_count == 0)
              prdguid = NULL;
          }
          break;

        case DQEK_BUBBLE:
          {
            struct nn_dqueue_bubble *b = (struct nn_dqueue_bubble *) e->sampleinfo;
            if (b->kind == NN_DQBK_STOP)
            {
              /* Stuff enqueued behind the bubble will still be
                 processed, we do want to drain the queue.  Nothing
                 may be queued anymore once we queue the stop bubble,
                 so q->sc.first should be empty.  If it isn't
                 ... dqueue_free fail an assertion.  STOP bubble
                 doesn't get malloced, and hence not freed. */
              keepgoing = 0;
            }
            else
            {
              switch (b->kind)
              {
                case NN_DQBK_STOP:
                  abort ();
                case NN_DQBK_CALLBACK:
                  b->u.cb.cb (b->u.cb.arg);
                  break;
                case NN_DQBK_RDGUID:
                  rdguid = b->u.rdguid.rdguid;
                  rdguid_count = b->u.rdguid.count;
                  prdguid = &rdguid;
                  break;
              }
              os_free (b);
            }
            break;
          }
      }
      thread_state_asleep (self);
    }

    os_mutexLock (&q->lock);
  }
  os_mutexUnlock (&q->lock);
  return 0;
}

struct nn_dqueue *nn_dqueue_new (const char *name, uint32_t max_samples, nn_dqueue_handler_t handler, void *arg)
{
  struct nn_dqueue *q;
  char *thrname;
  size_t thrnamesz;

  if ((q = os_malloc (sizeof (*q))) == NULL)
    goto fail_q;
  if ((q->name = os_strdup (name)) == NULL)
    goto fail_name;
  q->max_samples = max_samples;
  os_atomic_st32 (&q->nof_samples, 0);
  q->handler = handler;
  q->handler_arg = arg;
  q->sc.first = q->sc.last = NULL;

  os_mutexInit (&q->lock);
  os_condInit (&q->cond, &q->lock);

  thrnamesz = 3 + strlen (name) + 1;
  if ((thrname = os_malloc (thrnamesz)) == NULL)
    goto fail_thrname;
  snprintf (thrname, thrnamesz, "dq.%s", name);
  if ((q->ts = create_thread (thrname, (uint32_t (*) (void *)) dqueue_thread, q)) == NULL)
    goto fail_thread;
  os_free (thrname);
  return q;

 fail_thread:
  os_free (thrname);
 fail_thrname:
  os_condDestroy (&q->cond);
  os_mutexDestroy (&q->lock);
  os_free (q->name);
 fail_name:
  os_free (q);
 fail_q:
  return NULL;
}

static int nn_dqueue_enqueue_locked (struct nn_dqueue *q, struct nn_rsample_chain *sc)
{
  int must_signal;
  if (q->sc.first == NULL)
  {
    must_signal = 1;
    q->sc = *sc;
  }
  else
  {
    must_signal = 0;
    q->sc.last->next = sc->first;
    q->sc.last = sc->last;
  }
  return must_signal;
}

void nn_dqueue_enqueue (struct nn_dqueue *q, struct nn_rsample_chain *sc, nn_reorder_result_t rres)
{
  assert (rres > 0);
  assert (sc->first);
  assert (sc->last->next == NULL);
  os_mutexLock (&q->lock);
  os_atomic_add32 (&q->nof_samples, (uint32_t) rres);
  if (nn_dqueue_enqueue_locked (q, sc))
    os_condBroadcast (&q->cond);
  os_mutexUnlock (&q->lock);
}

static int nn_dqueue_enqueue_bubble_locked (struct nn_dqueue *q, struct nn_dqueue_bubble *b)
{
  struct nn_rsample_chain sc;
  b->sce.next = NULL;
  b->sce.fragchain = NULL;
  b->sce.sampleinfo = (struct nn_rsample_info *) b;
  sc.first = sc.last = &b->sce;
  return nn_dqueue_enqueue_locked (q, &sc);
}

static void nn_dqueue_enqueue_bubble (struct nn_dqueue *q, struct nn_dqueue_bubble *b)
{
  os_mutexLock (&q->lock);
  os_atomic_inc32 (&q->nof_samples);
  if (nn_dqueue_enqueue_bubble_locked (q, b))
    os_condBroadcast (&q->cond);
  os_mutexUnlock (&q->lock);
}

void nn_dqueue_enqueue_callback (struct nn_dqueue *q, nn_dqueue_callback_t cb, void *arg)
{
  struct nn_dqueue_bubble *b;
  b = os_malloc (sizeof (*b));
  b->kind = NN_DQBK_CALLBACK;
  b->u.cb.cb = cb;
  b->u.cb.arg = arg;
  nn_dqueue_enqueue_bubble (q, b);
}

void nn_dqueue_enqueue1 (struct nn_dqueue *q, const nn_guid_t *rdguid, struct nn_rsample_chain *sc, nn_reorder_result_t rres)
{
  struct nn_dqueue_bubble *b;

  b = os_malloc (sizeof (*b));
  b->kind = NN_DQBK_RDGUID;
  b->u.rdguid.rdguid = *rdguid;
  b->u.rdguid.count = (uint32_t) rres;

  assert (rres > 0);
  assert (rdguid != NULL);
  assert (sc->first);
  assert (sc->last->next == NULL);
  os_mutexLock (&q->lock);
  os_atomic_add32 (&q->nof_samples, 1 + (uint32_t) rres);
  if (nn_dqueue_enqueue_bubble_locked (q, b))
    os_condBroadcast (&q->cond);
  nn_dqueue_enqueue_locked (q, sc);
  os_mutexUnlock (&q->lock);
}

int nn_dqueue_is_full (struct nn_dqueue *q)
{
  /* Reading nof_samples exactly once. It IS a 32-bit int, so at
     worst we get an old value. That mean: we think it is full when
     it is not, in which case we discard the sample and rely on a
     retransmit; or we think it is not full when it is. But if we
     don't mind the occasional extra sample in the queue (we don't),
     and survive the occasional decision to not queue when it
     could've been queued (we do), it should be ok. */
  const uint32_t count = os_atomic_ld32 (&q->nof_samples);
  return (count >= q->max_samples);
}

void nn_dqueue_wait_until_empty_if_full (struct nn_dqueue *q)
{
  const uint32_t count = os_atomic_ld32 (&q->nof_samples);
  if (count >= q->max_samples)
  {
    os_mutexLock (&q->lock);
    while (os_atomic_ld32 (&q->nof_samples) > 0)
      os_condWait (&q->cond, &q->lock);
    os_mutexUnlock (&q->lock);
  }
}

void nn_dqueue_free (struct nn_dqueue *q)
{
  /* There must not be any thread enqueueing things anymore at this
     point.  The stop bubble is special in that it does _not_ get
     malloced or freed, but instead lives on the stack for a little
     while.  It would be a shame to fail in free() due to a lack of
     heap space, would it not? */
  struct nn_dqueue_bubble b;
  b.kind = NN_DQBK_STOP;
  nn_dqueue_enqueue_bubble (q, &b);

  join_thread (q->ts);
  assert (q->sc.first == NULL);
  os_condDestroy (&q->cond);
  os_mutexDestroy (&q->lock);
  os_free (q->name);
  os_free (q);
}

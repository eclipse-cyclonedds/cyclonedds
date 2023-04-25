// Copyright(c) 2006 to 2022 ZettaScale Technology and others
//
// This program and the accompanying materials are made available under the
// terms of the Eclipse Public License v. 2.0 which is available at
// http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
// v. 1.0 which is available at
// http://www.eclipse.org/org/documents/edl-v10.php.
//
// SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

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

#include "dds/ddsrt/heap.h"
#include "dds/ddsrt/threads.h"
#include "dds/ddsrt/sync.h"
#include "dds/ddsrt/string.h"
#include "dds/ddsrt/avl.h"
#include "dds/ddsi/ddsi_log.h"
#include "dds/ddsi/ddsi_plist.h"
#include "dds/ddsi/ddsi_unused.h"
#include "dds/ddsi/ddsi_domaingv.h" /* for mattr, cattr */
#include "ddsi__protocol.h"
#include "ddsi__log.h"
#include "ddsi__misc.h"
#include "ddsi__radmin.h"
#include "ddsi__bitset.h"
#include "ddsi__thread.h"

/* OVERVIEW ------------------------------------------------------------

   The receive path of DDSI has any number of receive threads that
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

   ddsi_rbufpool

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

   ddsi_rbuf

                Largish buffer for receiving several UDP packets and
                for storing partially decoded and indexing information
                directly following the packet.

   ddsi_rmsg

                One message in an rbuf; the layout for one message is
                rmsg, raw udp packet, decoder stuff mixed with rdata,
                defragmentation and message reordering state.  One
                rbuf can contain many messages.

   ddsi_rdata

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
       rbpool = ddsi_rbufpool_new (1MB, 128kB)
       ...

       while ...
         rmsg = ddsi_rmsg_new (rbpool)
         actualsize = recvfrom (rmsg.payload, 64kB)
         ddsi_rmsg_setsize (rmsg, actualsize)
         process (rmsg)
         ddsi_rmsg_commit (rmsg)

       ... ensure no references to any buffer in rbpool exist ...
       ddsi_rbufpool_free (rbpool)
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
       sample = ddsi_defrag_rsample (pwr->defrag, rdata, &sampleinfo)
       if sample
         fragchain = ddsi_rsample_fragchain (sample)
         refcount_adjust = 0;

         if send-to-proxy-writer-reorder
           if ddsi_reorder_rsample (&sc, pwr->reorder, sample, &refcount_adjust)
              == DELIVER
             deliver-to-group (pwr, sc)
         else
           for (m in out-of-sync-reader-matches)
             sample' = ddsi_reorder_rsample_dup (rmsg, sample)
             if ddsi_reorder_rsample (&sc, m->reorder, sample, &refcount_adjust)
                == DELIVER
               deliver-to-reader (m->reader, sc)

         ddsi_fragchain_adjust_refcount (fragchain, refcount_adjust)
       fi
     rof

   Where deliver-to-x() must of course decrement refcounts after
   delivery when done, using ddsi_fragchain_unref().  See also REORDER
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
       gap = ddsi_rdata_newgap (rmsg);
       if ddsi_reorder_gap (&sc, reorder, gap, a, b+1, &refcount_adjust)
         deliver-to-group (pwr, sc)
       for (m in out-of-sync-reader-matches)
         if ddsi_reorder_gap (&sc, m->reorder, gap, a, b+1, &refcount_adjust)
           deliver-to-reader (m->reader, sc)
       ddsi_fragchain_adjust_refcount (gap, refcount_adjust)

   Note that a Gap always gets processed both by the primary and by
   the secondary reorder admins.  This is because it covers a range.

   A heartbeat is similar, except that a heartbeat [a,b] results in a
   gap [1,a-1]. */

/* RBUFPOOL ------------------------------------------------------------ */

struct ddsi_rbufpool {
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
  ddsrt_mutex_t lock;
  struct ddsi_rbuf *current;
  uint32_t rbuf_size;
  uint32_t max_rmsg_size;
  const struct ddsrt_log_cfg *logcfg;
  bool trace;
#ifndef NDEBUG
  /* Thread that owns this pool, so we can check that no other thread
     is calling functions only the owner may use. */
  ddsrt_thread_t owner_tid;
#endif
};

static struct ddsi_rbuf *ddsi_rbuf_alloc_new (struct ddsi_rbufpool *rbp);
static void ddsi_rbuf_release (struct ddsi_rbuf *rbuf);

#define TRACE_CFG(obj, logcfg, ...) ((obj)->trace ? (void) DDS_CLOG (DDS_LC_RADMIN, (logcfg), __VA_ARGS__) : (void) 0)
#define TRACE(obj, ...)             TRACE_CFG ((obj), (obj)->logcfg, __VA_ARGS__)
#define RBPTRACE(...)               TRACE_CFG (rbp, rbp->logcfg, __VA_ARGS__)
#define RBUFTRACE(...)              TRACE_CFG (rbuf, rbuf->rbufpool->logcfg, __VA_ARGS__)
#define RMSGTRACE(...)              TRACE_CFG (rmsg, rmsg->chunk.rbuf->rbufpool->logcfg, __VA_ARGS__)
#define RDATATRACE(rdata, ...)      TRACE_CFG ((rdata)->rmsg, (rdata)->rmsg->chunk.rbuf->rbufpool->logcfg, __VA_ARGS__)

static uint32_t align_rmsg (uint32_t x)
{
  x += (uint32_t) DDSI_ALIGNOF_RMSG - 1;
  x -= x % (uint32_t) DDSI_ALIGNOF_RMSG;
  return x;
}

#ifndef NDEBUG
#define ASSERT_RBUFPOOL_OWNER(rbp) (assert (ddsrt_thread_equal (ddsrt_thread_self (), (rbp)->owner_tid)))
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
     ddsi_rmsg and ddsi_rmsg_chunk, but the difference in size is
     negligible really.  So in the interest of simplicity, we always
     allocate for the worst case, and may waste a few bytes here or
     there. */
  return
    max_uint32 ((uint32_t) (offsetof (struct ddsi_rmsg, chunk) + sizeof (struct ddsi_rmsg_chunk)),
                (uint32_t) sizeof (struct ddsi_rmsg_chunk))
    + max_rmsg_size;
}

struct ddsi_rbufpool *ddsi_rbufpool_new (const struct ddsrt_log_cfg *logcfg, uint32_t rbuf_size, uint32_t max_rmsg_size)
{
  struct ddsi_rbufpool *rbp;

  assert (max_rmsg_size > 0);

  /* raise rbuf_size to minimum possible considering max_rmsg_size, there is
     no reason to bother the user with the small difference between the two
     when he tries to configure things, and the crash is horrible when
     rbuf_size is too small */
  if (rbuf_size < max_rmsg_size_w_hdr (max_rmsg_size))
    rbuf_size = max_rmsg_size_w_hdr (max_rmsg_size);

  if ((rbp = ddsrt_malloc (sizeof (*rbp))) == NULL)
    goto fail_rbp;
#ifndef NDEBUG
  rbp->owner_tid = ddsrt_thread_self ();
#endif

  ddsrt_mutex_init (&rbp->lock);

  rbp->rbuf_size = rbuf_size;
  rbp->max_rmsg_size = max_rmsg_size;
  rbp->logcfg = logcfg;
  rbp->trace = (logcfg->c.mask & DDS_LC_RADMIN) != 0;

#if USE_VALGRIND
  VALGRIND_CREATE_MEMPOOL (rbp, 0, 0);
#endif

  if ((rbp->current = ddsi_rbuf_alloc_new (rbp)) == NULL)
    goto fail_rbuf;
  return rbp;

 fail_rbuf:
#if USE_VALGRIND
  VALGRIND_DESTROY_MEMPOOL (rbp);
#endif
  ddsrt_mutex_destroy (&rbp->lock);
  ddsrt_free (rbp);
 fail_rbp:
  return NULL;
}

void ddsi_rbufpool_setowner (UNUSED_ARG_NDEBUG (struct ddsi_rbufpool *rbp), UNUSED_ARG_NDEBUG (ddsrt_thread_t tid))
{
#ifndef NDEBUG
  rbp->owner_tid = tid;
#endif
}

void ddsi_rbufpool_free (struct ddsi_rbufpool *rbp)
{
#if 0
  /* Anyone may free it: I want to be able to stop the receive
     threads, then stop all other asynchronous processing, then clear
     out the buffers.  That's is the only way to verify that the
     reference counts are all 0, as they should be. */
  ASSERT_RBUFPOOL_OWNER (rbp);
#endif
  ddsi_rbuf_release (rbp->current);
#if USE_VALGRIND
  VALGRIND_DESTROY_MEMPOOL (rbp);
#endif
  ddsrt_mutex_destroy (&rbp->lock);
  ddsrt_free (rbp);
}

/* RBUF ---------------------------------------------------------------- */

struct ddsi_rbuf {
  ddsrt_atomic_uint32_t n_live_rmsg_chunks;
  uint32_t size;
  uint32_t max_rmsg_size;
  struct ddsi_rbufpool *rbufpool;
  bool trace;

  /* Allocating sequentially, releasing in random order, not bothering
     to reuse memory as soon as it becomes available again. I think
     this will have to change eventually, but this is the easiest
     approach.  Changes would be confined rmsg_new and rmsg_free. */
  unsigned char *freeptr;

  /* to ensure reasonable alignment of raw[] */
  union {
    int64_t l;
    double d;
    void *p;
  } u;

  /* raw data array, ddsi_rbuf::size bytes long in reality */
  unsigned char raw[];
};

static struct ddsi_rbuf *ddsi_rbuf_alloc_new (struct ddsi_rbufpool *rbp)
{
  struct ddsi_rbuf *rb;
  ASSERT_RBUFPOOL_OWNER (rbp);

  if ((rb = ddsrt_malloc (sizeof (struct ddsi_rbuf) + rbp->rbuf_size)) == NULL)
    return NULL;
#if USE_VALGRIND
  VALGRIND_MAKE_MEM_NOACCESS (rb->raw, rbp->rbuf_size);
#endif

  rb->rbufpool = rbp;
  ddsrt_atomic_st32 (&rb->n_live_rmsg_chunks, 1);
  rb->size = rbp->rbuf_size;
  rb->max_rmsg_size = rbp->max_rmsg_size;
  rb->freeptr = rb->raw;
  rb->trace = rbp->trace;
  RBPTRACE ("rbuf_alloc_new(%p) = %p\n", (void *) rbp, (void *) rb);
  return rb;
}

static struct ddsi_rbuf *ddsi_rbuf_new (struct ddsi_rbufpool *rbp)
{
  struct ddsi_rbuf *rb;
  assert (rbp->current);
  ASSERT_RBUFPOOL_OWNER (rbp);
  if ((rb = ddsi_rbuf_alloc_new (rbp)) != NULL)
  {
    ddsrt_mutex_lock (&rbp->lock);
    ddsi_rbuf_release (rbp->current);
    rbp->current = rb;
    ddsrt_mutex_unlock (&rbp->lock);
  }
  return rb;
}

static void ddsi_rbuf_release (struct ddsi_rbuf *rbuf)
{
  struct ddsi_rbufpool *rbp = rbuf->rbufpool;
  RBPTRACE ("rbuf_release(%p) pool %p current %p\n", (void *) rbuf, (void *) rbp, (void *) rbp->current);
  if (ddsrt_atomic_dec32_ov (&rbuf->n_live_rmsg_chunks) == 1)
  {
    RBPTRACE ("rbuf_release(%p) free\n", (void *) rbuf);
    ddsrt_free (rbuf);
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
#define ASSERT_RMSG_UNCOMMITTED(rmsg) (assert (ddsrt_atomic_ld32 (&(rmsg)->refcount) >= RMSG_REFCOUNT_UNCOMMITTED_BIAS))
#else
#define ASSERT_RMSG_UNCOMMITTED(rmsg) ((void) 0)
#endif

static void *ddsi_rbuf_alloc (struct ddsi_rbufpool *rbp)
{
  /* Note: only one thread calls ddsi_rmsg_new on a pool */
  uint32_t asize = max_rmsg_size_w_hdr (rbp->max_rmsg_size);
  struct ddsi_rbuf *rb;
  RBPTRACE ("rmsg_rbuf_alloc(%p, %"PRIu32")\n", (void *) rbp, asize);
  ASSERT_RBUFPOOL_OWNER (rbp);
  rb = rbp->current;
  assert (rb != NULL);
  assert (rb->freeptr >= rb->raw);
  assert (rb->freeptr <= rb->raw + rb->size);

  if ((uint32_t) (rb->raw + rb->size - rb->freeptr) < asize)
  {
    /* not enough space left for new rmsg */
    if ((rb = ddsi_rbuf_new (rbp)) == NULL)
      return NULL;

    /* a new one should have plenty of space */
    assert ((uint32_t) (rb->raw + rb->size - rb->freeptr) >= asize);
  }

  RBPTRACE ("rmsg_rbuf_alloc(%p, %"PRIu32") = %p\n", (void *) rbp, asize, (void *) rb->freeptr);
#if USE_VALGRIND
  VALGRIND_MEMPOOL_ALLOC (rbp, rb->freeptr, asize);
#endif
  return rb->freeptr;
}

static void init_rmsg_chunk (struct ddsi_rmsg_chunk *chunk, struct ddsi_rbuf *rbuf)
{
  chunk->rbuf = rbuf;
  chunk->next = NULL;
  chunk->u.size = 0;
  ddsrt_atomic_inc32 (&rbuf->n_live_rmsg_chunks);
}

struct ddsi_rmsg *ddsi_rmsg_new (struct ddsi_rbufpool *rbp)
{
  /* Note: only one thread calls ddsi_rmsg_new on a pool */
  struct ddsi_rmsg *rmsg;
  RBPTRACE ("rmsg_new(%p)\n", (void *) rbp);

  rmsg = ddsi_rbuf_alloc (rbp);
  if (rmsg == NULL)
    return NULL;

  /* Reference to this rmsg, undone by rmsg_commit(). */
  ddsrt_atomic_st32 (&rmsg->refcount, RMSG_REFCOUNT_UNCOMMITTED_BIAS);
  /* Initial chunk */
  init_rmsg_chunk (&rmsg->chunk, rbp->current);
  rmsg->trace = rbp->trace;
  rmsg->lastchunk = &rmsg->chunk;
  /* Incrementing freeptr happens in commit(), so that discarding the
     message is really simple. */
  RBPTRACE ("rmsg_new(%p) = %p\n", (void *) rbp, (void *) rmsg);
  return rmsg;
}

void ddsi_rmsg_setsize (struct ddsi_rmsg *rmsg, uint32_t size)
{
  uint32_t size8P = align_rmsg (size);
  RMSGTRACE ("rmsg_setsize(%p, %"PRIu32" => %"PRIu32")\n", (void *) rmsg, size, size8P);
  ASSERT_RBUFPOOL_OWNER (rmsg->chunk.rbuf->rbufpool);
  ASSERT_RMSG_UNCOMMITTED (rmsg);
  assert (ddsrt_atomic_ld32 (&rmsg->refcount) == RMSG_REFCOUNT_UNCOMMITTED_BIAS);
  assert (rmsg->chunk.u.size == 0);
  assert (size8P <= rmsg->chunk.rbuf->max_rmsg_size);
  assert (rmsg->lastchunk == &rmsg->chunk);
  rmsg->chunk.u.size = size8P;
#if USE_VALGRIND
  VALGRIND_MEMPOOL_CHANGE (rmsg->chunk.rbuf->rbufpool, rmsg, rmsg, offsetof (struct ddsi_rmsg, chunk.u.payload) + rmsg->chunk.size);
#endif
}

void ddsi_rmsg_free (struct ddsi_rmsg *rmsg)
{
  /* Note: any thread may call rmsg_free.

     FIXME: note that we could optimise by moving rbuf->freeptr back
     in (the likely to be fairly normal) case free space follows this
     rmsg.  Except that that would require synchronising new() and
     free() which we don't do currently.  And ideally, you'd use
     compare-and-swap for this. */
  struct ddsi_rmsg_chunk *c;
  RMSGTRACE ("rmsg_free(%p)\n", (void *) rmsg);
  assert (ddsrt_atomic_ld32 (&rmsg->refcount) == 0);
  c = &rmsg->chunk;
  while (c)
  {
    struct ddsi_rbuf *rbuf = c->rbuf;
    struct ddsi_rmsg_chunk *c1 = c->next;
#if USE_VALGRIND
    if (c == &rmsg->chunk) {
      VALGRIND_MEMPOOL_FREE (rbuf->rbufpool, rmsg);
    } else {
      VALGRIND_MEMPOOL_FREE (rbuf->rbufpool, c);
    }
#endif
    assert (ddsrt_atomic_ld32 (&rbuf->n_live_rmsg_chunks) > 0);
    ddsi_rbuf_release (rbuf);
    c = c1;
  }
}

static void commit_rmsg_chunk (struct ddsi_rmsg_chunk *chunk)
{
  struct ddsi_rbuf *rbuf = chunk->rbuf;
  RBUFTRACE ("commit_rmsg_chunk(%p)\n", (void *) chunk);
  rbuf->freeptr = (unsigned char *) (chunk + 1) + chunk->u.size;
}

void ddsi_rmsg_commit (struct ddsi_rmsg *rmsg)
{
  /* Note: only one thread calls rmsg_commit -- the one that created
     it in the first place.

     If there are no outstanding references, we can simply reuse the
     memory.  This happens, e.g., when the message is invalid, doesn't
     contain anything processed asynchronously, or the scheduling
     happens to be such that any asynchronous activities have
     completed before we got to commit. */
  struct ddsi_rmsg_chunk *chunk = rmsg->lastchunk;
  RMSGTRACE ("rmsg_commit(%p) refcount 0x%"PRIx32" last-chunk-size %"PRIu32"\n",
             (void *) rmsg, rmsg->refcount.v, chunk->u.size);
  ASSERT_RBUFPOOL_OWNER (chunk->rbuf->rbufpool);
  ASSERT_RMSG_UNCOMMITTED (rmsg);
  assert (chunk->u.size <= chunk->rbuf->max_rmsg_size);
  assert ((chunk->u.size % DDSI_ALIGNOF_RMSG) == 0);
  assert (ddsrt_atomic_ld32 (&rmsg->refcount) >= RMSG_REFCOUNT_UNCOMMITTED_BIAS);
  assert (ddsrt_atomic_ld32 (&rmsg->chunk.rbuf->n_live_rmsg_chunks) > 0);
  assert (ddsrt_atomic_ld32 (&chunk->rbuf->n_live_rmsg_chunks) > 0);
  assert (chunk->rbuf->rbufpool->current == chunk->rbuf);
  if (ddsrt_atomic_sub32_nv (&rmsg->refcount, RMSG_REFCOUNT_UNCOMMITTED_BIAS) == 0)
    ddsi_rmsg_free (rmsg);
  else
  {
    /* Other references exist, so either stored in defrag, reorder
       and/or delivery queue */
    RMSGTRACE ("rmsg_commit(%p) => keep\n", (void *) rmsg);
    commit_rmsg_chunk (chunk);
  }
}

static void ddsi_rmsg_addbias (struct ddsi_rmsg *rmsg)
{
  /* Note: only the receive thread that owns the receive pool may
     increase the reference count, and only while it is still
     uncommitted.

     However, other threads (e.g., delivery threads) may have been
     triggered already, so the increment must be done atomically. */
  RMSGTRACE ("rmsg_addbias(%p)\n", (void *) rmsg);
  ASSERT_RBUFPOOL_OWNER (rmsg->chunk.rbuf->rbufpool);
  ASSERT_RMSG_UNCOMMITTED (rmsg);
  ddsrt_atomic_add32 (&rmsg->refcount, RMSG_REFCOUNT_RDATA_BIAS);
}

static void ddsi_rmsg_rmbias_and_adjust (struct ddsi_rmsg *rmsg, int adjust)
{
  /* This can happen to any rmsg referenced by an sample still
     progressing through the pipeline, but only by a receive
     thread.  Can't require it to be uncommitted. */
  uint32_t sub;
  RMSGTRACE ("rmsg_rmbias_and_adjust(%p, %d)\n", (void *) rmsg, adjust);
  assert (adjust >= 0);
  assert ((uint32_t) adjust < RMSG_REFCOUNT_RDATA_BIAS);
  sub = RMSG_REFCOUNT_RDATA_BIAS - (uint32_t) adjust;
  assert (ddsrt_atomic_ld32 (&rmsg->refcount) >= sub);
  if (ddsrt_atomic_sub32_nv (&rmsg->refcount, sub) == 0)
    ddsi_rmsg_free (rmsg);
}

static void ddsi_rmsg_unref (struct ddsi_rmsg *rmsg)
{
  RMSGTRACE ("rmsg_unref(%p)\n", (void *) rmsg);
  assert (ddsrt_atomic_ld32 (&rmsg->refcount) > 0);
  if (ddsrt_atomic_dec32_ov (&rmsg->refcount) == 1)
    ddsi_rmsg_free (rmsg);
}

void *ddsi_rmsg_alloc (struct ddsi_rmsg *rmsg, uint32_t size)
{
  struct ddsi_rmsg_chunk *chunk = rmsg->lastchunk;
  struct ddsi_rbuf *rbuf = chunk->rbuf;
  uint32_t size8P = align_rmsg (size);
  void *ptr;
  RMSGTRACE ("rmsg_alloc(%p, %"PRIu32" => %"PRIu32")\n", (void *) rmsg, size, size8P);
  ASSERT_RBUFPOOL_OWNER (rbuf->rbufpool);
  ASSERT_RMSG_UNCOMMITTED (rmsg);
  assert ((chunk->u.size % DDSI_ALIGNOF_RMSG) == 0);
  assert (size8P <= rbuf->max_rmsg_size);

  if (chunk->u.size + size8P > rbuf->max_rmsg_size)
  {
    struct ddsi_rbufpool *rbp = rbuf->rbufpool;
    struct ddsi_rmsg_chunk *newchunk;
    RMSGTRACE ("rmsg_alloc(%p, %"PRIu32") limit hit - new chunk\n", (void *) rmsg, size);
    commit_rmsg_chunk (chunk);
    newchunk = ddsi_rbuf_alloc (rbp);
    if (newchunk == NULL)
    {
      DDS_CWARNING (rbp->logcfg, "ddsi_rmsg_alloc: can't allocate more memory (%"PRIu32" bytes) ... giving up\n", size);
      return NULL;
    }
    init_rmsg_chunk (newchunk, rbp->current);
    rmsg->lastchunk = chunk->next = newchunk;
    chunk = newchunk;
  }

  ptr = (unsigned char *) (chunk + 1) + chunk->u.size;
  chunk->u.size += size8P;
  RMSGTRACE ("rmsg_alloc(%p, %"PRIu32") = %p\n", (void *) rmsg, size, ptr);
#if USE_VALGRIND
  if (chunk == &rmsg->chunk) {
    VALGRIND_MEMPOOL_CHANGE (rbuf->rbufpool, rmsg, rmsg, offsetof (struct ddsi_rmsg, chunk.u.payload) + chunk->size);
  } else {
    VALGRIND_MEMPOOL_CHANGE (rbuf->rbufpool, chunk, chunk, offsetof (struct ddsi_rmsg_chunk, u.payload) + chunk->size);
  }
#endif
  return ptr;
}

/* RDATA --------------------------------------- */

struct ddsi_rdata *ddsi_rdata_new (struct ddsi_rmsg *rmsg, uint32_t start, uint32_t endp1, uint32_t submsg_offset, uint32_t payload_offset, uint32_t keyhash_offset)
{
  struct ddsi_rdata *d;
  if ((d = ddsi_rmsg_alloc (rmsg, sizeof (*d))) == NULL)
    return NULL;
  d->rmsg = rmsg;
  d->nextfrag = NULL;
  d->min = start;
  d->maxp1 = endp1;
  d->submsg_zoff = (uint16_t) DDSI_OFF_TO_ZOFF (submsg_offset);
  d->payload_zoff = (uint16_t) DDSI_OFF_TO_ZOFF (payload_offset);
  d->keyhash_zoff = (uint16_t) DDSI_OFF_TO_ZOFF (keyhash_offset);
#ifndef NDEBUG
  ddsrt_atomic_st32 (&d->refcount_bias_added, 0);
#endif
  RMSGTRACE ("rdata_new(%p, bytes [%"PRIu32",%"PRIu32"), submsg @ %u, payload @ %u) = %p\n",
             (void *) rmsg, start, endp1, DDSI_RDATA_SUBMSG_OFF (d), DDSI_RDATA_PAYLOAD_OFF (d), (void *) d);
  return d;
}

static void ddsi_rdata_addbias (struct ddsi_rdata *rdata)
{
  struct ddsi_rmsg *rmsg = rdata->rmsg;
  RMSGTRACE ("rdata_addbias(%p)\n", (void *) rdata);
#ifndef NDEBUG
  ASSERT_RBUFPOOL_OWNER (rmsg->chunk.rbuf->rbufpool);
  if (ddsrt_atomic_inc32_nv (&rdata->refcount_bias_added) != 1)
    abort ();
#endif
  ddsi_rmsg_addbias (rmsg);
}

static void ddsi_rdata_rmbias_and_adjust (struct ddsi_rdata *rdata, int adjust)
{
  struct ddsi_rmsg *rmsg = rdata->rmsg;
  RMSGTRACE ("rdata_rmbias_and_adjust(%p, %d)\n", (void *) rdata, adjust);
#ifndef NDEBUG
  if (ddsrt_atomic_dec32_ov (&rdata->refcount_bias_added) != 1)
    abort ();
#endif
  ddsi_rmsg_rmbias_and_adjust (rmsg, adjust);
}

static void ddsi_rdata_unref (struct ddsi_rdata *rdata)
{
  struct ddsi_rmsg *rmsg = rdata->rmsg;
  RMSGTRACE ("rdata_rdata_unref(%p)\n", (void *) rdata);
  ddsi_rmsg_unref (rmsg);
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

struct ddsi_defrag_iv {
  ddsrt_avl_node_t avlnode; /* for ddsi_rsample.defrag::fragtree */
  uint32_t min, maxp1;
  struct ddsi_rdata *first;
  struct ddsi_rdata *last;
};

struct ddsi_rsample {
  union {
    struct ddsi_rsample_defrag {
      ddsrt_avl_node_t avlnode; /* for ddsi_defrag::sampletree */
      ddsrt_avl_tree_t fragtree;
      struct ddsi_defrag_iv *lastfrag;
      struct ddsi_rsample_info *sampleinfo;
      ddsi_seqno_t seq;
    } defrag;
    struct ddsi_rsample_reorder {
      ddsrt_avl_node_t avlnode;       /* for ddsi_reorder::sampleivtree, if head of a chain */
      struct ddsi_rsample_chain sc; /* this interval's samples, covering ... */
      ddsi_seqno_t min, maxp1;        /* ... seq nos: [min,maxp1), but possibly with holes in it */
      uint32_t n_samples;        /* so this is the actual length of the chain */
    } reorder;
  } u;
};

struct ddsi_defrag {
  ddsrt_avl_tree_t sampletree;
  struct ddsi_rsample *max_sample; /* = max(sampletree) */
  uint32_t n_samples;
  uint32_t max_samples;
  enum ddsi_defrag_drop_mode drop_mode;
  uint64_t discarded_bytes;
  const struct ddsrt_log_cfg *logcfg;
  bool trace;
};

static int compare_uint32 (const void *va, const void *vb);
static int compare_seqno (const void *va, const void *vb);

static const ddsrt_avl_treedef_t defrag_sampletree_treedef = DDSRT_AVL_TREEDEF_INITIALIZER (offsetof (struct ddsi_rsample, u.defrag.avlnode), offsetof (struct ddsi_rsample, u.defrag.seq), compare_seqno, 0);
static const ddsrt_avl_treedef_t rsample_defrag_fragtree_treedef = DDSRT_AVL_TREEDEF_INITIALIZER (offsetof (struct ddsi_defrag_iv, avlnode), offsetof (struct ddsi_defrag_iv, min), compare_uint32, 0);

static int compare_uint32 (const void *va, const void *vb)
{
  uint32_t a = *((const uint32_t *) va);
  uint32_t b = *((const uint32_t *) vb);
  return (a == b) ? 0 : (a < b) ? -1 : 1;
}

static int compare_seqno (const void *va, const void *vb)
{
  ddsi_seqno_t a = *((const ddsi_seqno_t *) va);
  ddsi_seqno_t b = *((const ddsi_seqno_t *) vb);
  return (a == b) ? 0 : (a < b) ? -1 : 1;
}

struct ddsi_defrag *ddsi_defrag_new (const struct ddsrt_log_cfg *logcfg, enum ddsi_defrag_drop_mode drop_mode, uint32_t max_samples)
{
  struct ddsi_defrag *d;
  assert (max_samples >= 1);
  if ((d = ddsrt_malloc (sizeof (*d))) == NULL)
    return NULL;
  ddsrt_avl_init (&defrag_sampletree_treedef, &d->sampletree);
  d->drop_mode = drop_mode;
  d->max_samples = max_samples;
  d->n_samples = 0;
  d->max_sample = NULL;
  d->discarded_bytes = 0;
  d->logcfg = logcfg;
  d->trace = (logcfg->c.mask & DDS_LC_RADMIN) != 0;
  return d;
}

void ddsi_defrag_stats (struct ddsi_defrag *defrag, uint64_t *discarded_bytes)
{
  *discarded_bytes = defrag->discarded_bytes;
}

void ddsi_fragchain_adjust_refcount (struct ddsi_rdata *frag, int adjust)
{
  RDATATRACE (frag, "fragchain_adjust_refcount(%p, %d)\n", (void *) frag, adjust);
  while (frag)
  {
    struct ddsi_rdata * const frag1 = frag->nextfrag;
    ddsi_rdata_rmbias_and_adjust (frag, adjust);
    frag = frag1;
  }
}

static void ddsi_fragchain_rmbias (struct ddsi_rdata *frag)
{
  ddsi_fragchain_adjust_refcount (frag, 0);
}

static void defrag_rsample_drop (struct ddsi_defrag *defrag, struct ddsi_rsample *rsample)
{
  /* Can't reference rsample after the first fragchain_free, because
     we don't know which rdata/rmsg provides the storage for the
     rsample and therefore can't increment the reference count.

     So we need to walk the fragments while guaranteeing strict
     "forward progress" in the memory accesses, which this particular
     inorder treewalk does provide. */
  ddsrt_avl_iter_t iter;
  struct ddsi_defrag_iv *iv;
  TRACE (defrag, "  defrag_rsample_drop (%p, %p)\n", (void *) defrag, (void *) rsample);
  ddsrt_avl_delete (&defrag_sampletree_treedef, &defrag->sampletree, rsample);
  assert (defrag->n_samples > 0);
  defrag->n_samples--;
  for (iv = ddsrt_avl_iter_first (&rsample_defrag_fragtree_treedef, &rsample->u.defrag.fragtree, &iter); iv; iv = ddsrt_avl_iter_next (&iter))
  {
    if (iv->first)
      /* if the first fragment is missing, a sentinel "iv" is inserted with an empty chain */
      ddsi_fragchain_rmbias (iv->first);
  }
}

void ddsi_defrag_free (struct ddsi_defrag *defrag)
{
  struct ddsi_rsample *s;
  s = ddsrt_avl_find_min (&defrag_sampletree_treedef, &defrag->sampletree);
  while (s)
  {
    TRACE (defrag, "defrag_free(%p, sample %p seq %"PRIu64")\n", (void *) defrag, (void *) s, s->u.defrag.seq);
    defrag_rsample_drop (defrag, s);
    s = ddsrt_avl_find_min (&defrag_sampletree_treedef, &defrag->sampletree);
  }
  assert (defrag->n_samples == 0);
  ddsrt_free (defrag);
}

static int defrag_try_merge_with_succ (const struct ddsi_defrag *defrag, struct ddsi_rsample_defrag *sample, struct ddsi_defrag_iv *node)
{
  struct ddsi_defrag_iv *succ;

  TRACE (defrag, "  defrag_try_merge_with_succ(%p [%"PRIu32"..%"PRIu32")):\n", (void *) node, node->min, node->maxp1);
  if (node == sample->lastfrag)
  {
    /* there is no interval following node */
    TRACE (defrag, "  node is lastfrag\n");
    return 0;
  }

  succ = ddsrt_avl_find_succ (&rsample_defrag_fragtree_treedef, &sample->fragtree, node);
  assert (succ != NULL);
  TRACE (defrag, "  succ is %p [%"PRIu32"..%"PRIu32")\n", (void *) succ, succ->min, succ->maxp1);
  if (succ->min > node->maxp1)
  {
    TRACE (defrag, "  gap between node and succ\n");
    return 0;
  }
  else
  {
    uint32_t succ_maxp1 = succ->maxp1;

    /* no longer a gap between node & succ => succ will be removed
       from the interval tree and therefore node will become the
       last interval if succ currently is */
    ddsrt_avl_delete (&rsample_defrag_fragtree_treedef, &sample->fragtree, succ);
    if (sample->lastfrag == succ)
    {
      TRACE (defrag, "  succ is lastfrag\n");
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
      TRACE (defrag, "  succ adds data to node\n");
    else
      TRACE (defrag, "  succ is contained in node\n");

    node->last->nextfrag = succ->first;
    node->last = succ->last;
    node->maxp1 = succ_maxp1;

    /* if the new fragment contains data beyond succ it may even
       allow merging with succ-succ */
    return node->maxp1 > succ_maxp1;
  }
}

static void defrag_rsample_addiv (struct ddsi_rsample_defrag *sample, struct ddsi_rdata *rdata, ddsrt_avl_ipath_t *path)
{
  struct ddsi_defrag_iv *newiv;
  if ((newiv = ddsi_rmsg_alloc (rdata->rmsg, sizeof (*newiv))) == NULL)
    return;
  rdata->nextfrag = NULL;
  newiv->first = newiv->last = rdata;
  newiv->min = rdata->min;
  newiv->maxp1 = rdata->maxp1;
  ddsi_rdata_addbias (rdata);
  ddsrt_avl_insert_ipath (&rsample_defrag_fragtree_treedef, &sample->fragtree, newiv, path);
  if (sample->lastfrag == NULL || rdata->min > sample->lastfrag->min)
    sample->lastfrag = newiv;
}

static void rsample_init_common (UNUSED_ARG (struct ddsi_rsample *rsample), UNUSED_ARG (struct ddsi_rdata *rdata), UNUSED_ARG (const struct ddsi_rsample_info *sampleinfo))
{
}

static struct ddsi_rsample *defrag_rsample_new (struct ddsi_rdata *rdata, const struct ddsi_rsample_info *sampleinfo)
{
  struct ddsi_rsample *rsample;
  struct ddsi_rsample_defrag *dfsample;
  ddsrt_avl_ipath_t ivpath;

  if ((rsample = ddsi_rmsg_alloc (rdata->rmsg, sizeof (*rsample))) == NULL)
    return NULL;
  rsample_init_common (rsample, rdata, sampleinfo);
  dfsample = &rsample->u.defrag;
  dfsample->lastfrag = NULL;
  dfsample->seq = sampleinfo->seq;
  if ((dfsample->sampleinfo = ddsi_rmsg_alloc (rdata->rmsg, sizeof (*dfsample->sampleinfo))) == NULL)
    return NULL;
  *dfsample->sampleinfo = *sampleinfo;

  ddsrt_avl_init (&rsample_defrag_fragtree_treedef, &dfsample->fragtree);

  /* add sentinel if rdata is not the first fragment of the message */
  if (rdata->min > 0)
  {
    struct ddsi_defrag_iv *sentinel;
    if ((sentinel = ddsi_rmsg_alloc (rdata->rmsg, sizeof (*sentinel))) == NULL)
      return NULL;
    sentinel->first = sentinel->last = NULL;
    sentinel->min = sentinel->maxp1 = 0;
    ddsrt_avl_lookup_ipath (&rsample_defrag_fragtree_treedef, &dfsample->fragtree, &sentinel->min, &ivpath);
    ddsrt_avl_insert_ipath (&rsample_defrag_fragtree_treedef, &dfsample->fragtree, sentinel, &ivpath);
  }

  /* add an interval for the first received fragment */
  ddsrt_avl_lookup_ipath (&rsample_defrag_fragtree_treedef, &dfsample->fragtree, &rdata->min, &ivpath);
  defrag_rsample_addiv (dfsample, rdata, &ivpath);
  return rsample;
}

static struct ddsi_rsample *reorder_rsample_new (struct ddsi_rdata *rdata, const struct ddsi_rsample_info *sampleinfo)
{
  /* Implements:

       defrag_rsample_new ; rsample_convert_defrag_to_reorder

     It is simple enough to warrant having an extra function. Note the
     discrepancy between defrag_rsample_new which fully initializes
     the rsample, including the AVL node headers, and this function,
     which doesn't do so. */
  struct ddsi_rsample *rsample;
  struct ddsi_rsample_reorder *s;
  struct ddsi_rsample_chain_elem *sce;

  if ((rsample = ddsi_rmsg_alloc (rdata->rmsg, sizeof (*rsample))) == NULL)
    return NULL;
  rsample_init_common (rsample, rdata, sampleinfo);

  if ((sce = ddsi_rmsg_alloc (rdata->rmsg, sizeof (*sce))) == NULL)
    return NULL;
  sce->fragchain = rdata;
  sce->next = NULL;
  if ((sce->sampleinfo = ddsi_rmsg_alloc (rdata->rmsg, sizeof (*sce->sampleinfo))) == NULL)
    return NULL;
  *sce->sampleinfo = *sampleinfo;
  rdata->nextfrag = NULL;
  ddsi_rdata_addbias (rdata);

  s = &rsample->u.reorder;
  s->min = sampleinfo->seq;
  s->maxp1 = sampleinfo->seq + 1;
  s->n_samples = 1;
  s->sc.first = s->sc.last = sce;
  return rsample;
}

static int is_complete (const struct ddsi_rsample_defrag *sample)
{
  /* Returns: NULL if 'sample' is incomplete, else 'sample'. Complete:
     one interval covering all bytes. One interval because of the
     greedy coalescing in add_fragment(). There is at least one
     interval if we get here. */
  const struct ddsi_defrag_iv *iv = ddsrt_avl_root (&rsample_defrag_fragtree_treedef, &sample->fragtree);
  assert (iv != NULL);
  if (iv->min == 0 && iv->maxp1 >= sample->sampleinfo->size)
  {
    /* Accept fragments containing data beyond the end of the sample,
       only to filter them out (or not, as the case may be) at a later
       stage. Dropping them before the defragmeter leaves us with
       samples that will never be completed; dropping them in the
       defragmenter would be feasible by discarding all fragments of
       that sample collected so far. */
    assert (ddsrt_avl_is_singleton (&sample->fragtree));
    return 1;
  }
  else
  {
    return 0;
  }
}

static void rsample_convert_defrag_to_reorder (struct ddsi_rsample *sample)
{
  /* Converts an rsample as stored in defrag to one as stored in a
     reorder admin. Have to be careful with the ordering, or at least
     somewhat, and the easy way out uses a few local variables -- any
     self-respecting compiler will optimise them away, and any
     self-respecting CPU would need to copy them via registers anyway
     because it uses a load-store architecture. */
  struct ddsi_defrag_iv *iv = ddsrt_avl_root_non_empty (&rsample_defrag_fragtree_treedef, &sample->u.defrag.fragtree);
  struct ddsi_rdata *fragchain = iv->first;
  struct ddsi_rsample_info *sampleinfo = sample->u.defrag.sampleinfo;
  struct ddsi_rsample_chain_elem *sce;
  ddsi_seqno_t seq = sample->u.defrag.seq;

  /* re-use memory fragment interval node for sample chain */
  sce = (struct ddsi_rsample_chain_elem *) ddsrt_avl_root_non_empty (&rsample_defrag_fragtree_treedef, &sample->u.defrag.fragtree);
  sce->fragchain = fragchain;
  sce->next = NULL;
  sce->sampleinfo = sampleinfo;

  sample->u.reorder.sc.first = sample->u.reorder.sc.last = sce;
  sample->u.reorder.min = seq;
  sample->u.reorder.maxp1 = seq + 1;
  sample->u.reorder.n_samples = 1;
}

static struct ddsi_rsample *defrag_add_fragment (struct ddsi_defrag *defrag, struct ddsi_rsample *sample, struct ddsi_rdata *rdata, const struct ddsi_rsample_info *sampleinfo)
{
  struct ddsi_rsample_defrag *dfsample = &sample->u.defrag;
  struct ddsi_defrag_iv *predeq, *succ;
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
  assert (dfsample->lastfrag == ddsrt_avl_find_max (&rsample_defrag_fragtree_treedef, &dfsample->fragtree));

  TRACE (defrag, "  lastfrag %p [%"PRIu32"..%"PRIu32")\n", (void *) dfsample->lastfrag, dfsample->lastfrag->min, dfsample->lastfrag->maxp1);

  /* Interval tree is sorted on min offset; each key is unique:
     otherwise one would be wholly contained in another. */
  if (min >= dfsample->lastfrag->min)
  {
    /* Assumed normal case: fragment appends data */
    predeq = dfsample->lastfrag;
    TRACE (defrag, "  fast path: predeq = lastfrag\n");
  }
  else
  {
    /* Slow path: find preceding fragment by tree search */
    predeq = ddsrt_avl_lookup_pred_eq (&rsample_defrag_fragtree_treedef, &dfsample->fragtree, &min);
    assert (predeq);
    TRACE (defrag, "  slow path: predeq = lookup %"PRIu32" => %p [%"PRIu32"..%"PRIu32")\n", min, (void *) predeq, predeq->min, predeq->maxp1);
  }

  /* we have a sentinel interval of [0,0) until we receive a packet
     that contains the first byte of the message, that is, there
     should always be predeq */
  assert (predeq != NULL);

  if (predeq->maxp1 >= maxp1)
  {
    /* new is contained in predeq, discard new; rdata did not cause
       completion of a sample */
    TRACE (defrag, "  new contained in predeq\n");
    defrag->discarded_bytes += maxp1 - min;
    return NULL;
  }
  else if (min <= predeq->maxp1)
  {
    /* new extends predeq, add it to the chain (necessarily at the
       end); this may close the gap to the successor of predeq; predeq
       need not have a fragment chain yet (it may be the sentinel) */
    TRACE (defrag, "  grow predeq with new\n");
    ddsi_rdata_addbias (rdata);
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
    while (defrag_try_merge_with_succ (defrag, dfsample, predeq))
      ;
    return is_complete (dfsample) ? sample : NULL;
  }
  else if (predeq != dfsample->lastfrag && /* if predeq is last frag, there is no succ */
           (succ = ddsrt_avl_find_succ (&rsample_defrag_fragtree_treedef, &dfsample->fragtree, predeq)) != NULL &&
           succ->min <= maxp1)
  {
    /* extends succ (at the low end; no guarantee each individual
       fragment in the chain adds value); but doesn't overlap with
       predeq so the tree structure doesn't change even though the key
       does change */
    TRACE (defrag, "  extending succ %p [%"PRIu32"..%"PRIu32") at head\n", (void *) succ, succ->min, succ->maxp1);
    ddsi_rdata_addbias (rdata);
    rdata->nextfrag = succ->first;
    succ->first = rdata;
    succ->min = min;
    /* new one may cover all of succ & more, in which case we must
       update the max of succ & see if we can merge it with
       succ-succ */
    if (maxp1 > succ->maxp1)
    {
      TRACE (defrag, "  extending succ at end as well\n");
      succ->maxp1 = maxp1;
      while (defrag_try_merge_with_succ (defrag, dfsample, succ))
        ;
    }
    assert (!is_complete (dfsample));
    return NULL;
  }
  else
  {
    /* doesn't extend either predeq at the end or succ at the head =>
       new interval; rdata did not cause completion of sample */
    ddsrt_avl_ipath_t path;
    TRACE (defrag, "  new interval\n");
    if (ddsrt_avl_lookup_ipath (&rsample_defrag_fragtree_treedef, &dfsample->fragtree, &min, &path))
      assert (0);
    defrag_rsample_addiv (dfsample, rdata, &path);
    return NULL;
  }
}

static int ddsi_rdata_is_fragment (const struct ddsi_rdata *rdata, const struct ddsi_rsample_info *sampleinfo)
{
  /* sanity check: min, maxp1 must be within bounds */
  assert (rdata->min <= rdata->maxp1);
  assert (rdata->maxp1 <= sampleinfo->size);
  return !(rdata->min == 0 && rdata->maxp1 == sampleinfo->size);
}

static int defrag_limit_samples (struct ddsi_defrag *defrag, ddsi_seqno_t seq, ddsi_seqno_t *max_seq)
{
  struct ddsi_rsample *sample_to_drop = NULL;
  if (defrag->n_samples < defrag->max_samples)
    return 1;
  /* max_samples >= 1 => some sample present => max_sample != NULL */
  assert (defrag->max_sample != NULL);
  TRACE (defrag, "  max samples reached\n");
  switch (defrag->drop_mode)
  {
    case DDSI_DEFRAG_DROP_LATEST:
      TRACE (defrag, "  drop mode = DROP_LATEST\n");
      if (seq > defrag->max_sample->u.defrag.seq)
      {
        TRACE (defrag, "  new sample is new latest => discarding it\n");
        return 0;
      }
      sample_to_drop = defrag->max_sample;
      break;
    case DDSI_DEFRAG_DROP_OLDEST:
      TRACE (defrag, "  drop mode = DROP_OLDEST\n");
      sample_to_drop = ddsrt_avl_find_min (&defrag_sampletree_treedef, &defrag->sampletree);
      assert (sample_to_drop);
      if (seq < sample_to_drop->u.defrag.seq)
      {
        TRACE (defrag, "  new sample is new oldest => discarding it\n");
        return 0;
      }
      break;
  }
  assert (sample_to_drop != NULL);
  defrag_rsample_drop (defrag, sample_to_drop);
  if (sample_to_drop == defrag->max_sample)
  {
    defrag->max_sample = ddsrt_avl_find_max (&defrag_sampletree_treedef, &defrag->sampletree);
    *max_seq = defrag->max_sample ? defrag->max_sample->u.defrag.seq : 0;
    TRACE (defrag, "  updating max_sample: now %p %"PRIu64"\n",
           (void *) defrag->max_sample, defrag->max_sample ? defrag->max_sample->u.defrag.seq : 0);
  }
  return 1;
}

struct ddsi_rsample *ddsi_defrag_rsample (struct ddsi_defrag *defrag, struct ddsi_rdata *rdata, const struct ddsi_rsample_info *sampleinfo)
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
  struct ddsi_rsample *sample, *result;
  ddsi_seqno_t max_seq;
  ddsrt_avl_ipath_t path;

  assert (defrag->n_samples <= defrag->max_samples);

  /* not a fragment => always complete, so refcount rdata, turn into a
     valid chain behind a valid msginfo and return it. */
  if (!ddsi_rdata_is_fragment (rdata, sampleinfo))
    return reorder_rsample_new (rdata, sampleinfo);

  /* max_seq is used for the fast path, and is 0 when there is no
     last message in 'defrag'. max_seq and max_sample must be
     consistent. Max_sample must be consistent with tree */
  assert (defrag->max_sample == ddsrt_avl_find_max (&defrag_sampletree_treedef, &defrag->sampletree));
  max_seq = defrag->max_sample ? defrag->max_sample->u.defrag.seq : 0;
  TRACE (defrag, "defrag_rsample(%p, %p [%"PRIu32"..%"PRIu32") msg %p, %p seq %"PRIu64" size %"PRIu32") max_seq %p %"PRIu64":\n",
         (void *) defrag, (void *) rdata, rdata->min, rdata->maxp1, (void *) rdata->rmsg,
         (void *) sampleinfo, sampleinfo->seq, sampleinfo->size,
         (void *) defrag->max_sample, max_seq);
  /* fast path: rdata is part of message with the highest sequence
     number we're currently defragmenting, or is beyond that */
  if (sampleinfo->seq == max_seq)
  {
    TRACE (defrag, "  add fragment to max_sample\n");
    result = defrag_add_fragment (defrag, defrag->max_sample, rdata, sampleinfo);
  }
  else if (!defrag_limit_samples (defrag, sampleinfo->seq, &max_seq))
  {
    TRACE (defrag, "  discarding sample\n");
    result = NULL;
  }
  else if (sampleinfo->seq > max_seq)
  {
    /* a node with a key greater than the maximum always is the right
       child of the old maximum node */
    /* FIXME: MERGE THIS ONE WITH THE NEXT */
    TRACE (defrag, "  new max sample\n");
    ddsrt_avl_lookup_ipath (&defrag_sampletree_treedef, &defrag->sampletree, &sampleinfo->seq, &path);
    if ((sample = defrag_rsample_new (rdata, sampleinfo)) == NULL)
      return NULL;
    ddsrt_avl_insert_ipath (&defrag_sampletree_treedef, &defrag->sampletree, sample, &path);
    defrag->max_sample = sample;
    defrag->n_samples++;
    result = NULL;
  }
  else if ((sample = ddsrt_avl_lookup_ipath (&defrag_sampletree_treedef, &defrag->sampletree, &sampleinfo->seq, &path)) == NULL)
  {
    /* a new sequence number, but smaller than the maximum */
    TRACE (defrag, "  new sample less than max\n");
    assert (sampleinfo->seq < max_seq);
    if ((sample = defrag_rsample_new (rdata, sampleinfo)) == NULL)
      return NULL;
    ddsrt_avl_insert_ipath (&defrag_sampletree_treedef, &defrag->sampletree, sample, &path);
    defrag->n_samples++;
    result = NULL;
  }
  else
  {
    /* adds (or, as the case may be, doesn't add) to a known message */
    TRACE (defrag, "  add fragment to %p\n", (void *) sample);
    result = defrag_add_fragment (defrag, sample, rdata, sampleinfo);
  }

  if (result != NULL)
  {
    /* Once completed, remove from defrag sample tree and convert to
       reorder format. If it is the sample with the maximum sequence in
       the tree, an update of max_sample is required. */
    TRACE (defrag, "  complete\n");
    ddsrt_avl_delete (&defrag_sampletree_treedef, &defrag->sampletree, result);
    assert (defrag->n_samples > 0);
    defrag->n_samples--;
    if (result == defrag->max_sample)
    {
      defrag->max_sample = ddsrt_avl_find_max (&defrag_sampletree_treedef, &defrag->sampletree);
      TRACE (defrag, "  updating max_sample: now %p %"PRIu64"\n",
             (void *) defrag->max_sample, defrag->max_sample ? defrag->max_sample->u.defrag.seq : 0);
    }
    rsample_convert_defrag_to_reorder (result);
  }

  assert (defrag->max_sample == ddsrt_avl_find_max (&defrag_sampletree_treedef, &defrag->sampletree));
  return result;
}

void ddsi_defrag_notegap (struct ddsi_defrag *defrag, ddsi_seqno_t min, ddsi_seqno_t maxp1)
{
  /* All sequence numbers in [min,maxp1) are unavailable so any
     fragments in that range must be discarded.  Used both for
     Hearbeats (by setting min=1) and for Gaps. */
  struct ddsi_rsample *s = ddsrt_avl_lookup_succ_eq (&defrag_sampletree_treedef, &defrag->sampletree, &min);
  while (s && s->u.defrag.seq < maxp1)
  {
    struct ddsi_rsample *s1 = ddsrt_avl_find_succ (&defrag_sampletree_treedef, &defrag->sampletree, s);
    defrag_rsample_drop (defrag, s);
    s = s1;
  }
  defrag->max_sample = ddsrt_avl_find_max (&defrag_sampletree_treedef, &defrag->sampletree);
}

enum ddsi_defrag_nackmap_result ddsi_defrag_nackmap (struct ddsi_defrag *defrag, ddsi_seqno_t seq, uint32_t maxfragnum, struct ddsi_fragment_number_set_header *map, uint32_t *mapbits, uint32_t maxsz)
{
  struct ddsi_rsample *s;
  struct ddsi_defrag_iv *iv;
  uint32_t i, fragsz, nfrags;
  assert (maxsz <= 256);
  s = ddsrt_avl_lookup (&defrag_sampletree_treedef, &defrag->sampletree, &seq);
  if (s == NULL)
  {
    if (maxfragnum == UINT32_MAX)
    {
      /* If neither the caller nor the defragmenter knows anything about the sample, say so */
      return DDSI_DEFRAG_NACKMAP_UNKNOWN_SAMPLE;
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
      ddsi_bitset_one (map->numbits, mapbits);
      return DDSI_DEFRAG_NACKMAP_FRAGMENTS_MISSING;
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
    struct ddsi_defrag_iv *liv = s->u.defrag.lastfrag;
    ddsi_fragment_number_t map_end;
    iv = ddsrt_avl_find_min (&rsample_defrag_fragtree_treedef, &s->u.defrag.fragtree);
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
    if (map_end < map->bitmap_base)
      return DDSI_DEFRAG_NACKMAP_ALL_ADVERTISED_FRAGMENTS_KNOWN;
    map->numbits = map_end - map->bitmap_base + 1;
    iv = ddsrt_avl_find_succ (&rsample_defrag_fragtree_treedef, &s->u.defrag.fragtree, iv);
  }

  /* Clear bitmap, then set bits for gaps in available fragments */
  if (map->numbits > maxsz)
    map->numbits = maxsz;
  ddsi_bitset_zero (map->numbits, mapbits);
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
      ddsi_bitset_set (map->numbits, mapbits, x);
    }
    /* next sequence of fragments to request retranmsission of starts
       at fragment containing maxp1 (because we don't have that byte
       yet), and runs until the next interval begins */
    i = iv->maxp1 / fragsz;
    iv = ddsrt_avl_find_succ (&rsample_defrag_fragtree_treedef, &s->u.defrag.fragtree, iv);
  }
  /* and set bits for missing fragments beyond the highest interval */
  for (; i < map->bitmap_base + map->numbits; i++)
  {
    unsigned x = (unsigned) (i - map->bitmap_base);
    ddsi_bitset_set (map->numbits, mapbits, x);
  }
  return DDSI_DEFRAG_NACKMAP_FRAGMENTS_MISSING;
}

/* There is only one defrag per proxy writer. However for the Volatile Secure writer a filter
 * is applied to filter on the destination participant. Note that there will be one
 * builtin Volatile Secure reader for each local participant. When this local participant
 * is deleted the defrag buffer may still contain fragments for the associated reader.
 * The ddsi_defrag_prune is used to remove these fragments and should only be used when
 * the Volatile Secure reader is deleted.
 */
void ddsi_defrag_prune (struct ddsi_defrag *defrag, ddsi_guid_prefix_t *dst, ddsi_seqno_t min)
{
  struct ddsi_rsample *s = ddsrt_avl_lookup_succ_eq (&defrag_sampletree_treedef, &defrag->sampletree, &min);
  while (s)
  {
    struct ddsi_rsample *s1 = ddsrt_avl_find_succ (&defrag_sampletree_treedef, &defrag->sampletree, s);
    if (ddsi_guid_prefix_eq(&s->u.defrag.sampleinfo->rst->dst_guid_prefix, dst))
    {
      defrag_rsample_drop (defrag, s);
    }
    s = s1;
  }
  defrag->max_sample = ddsrt_avl_find_max (&defrag_sampletree_treedef, &defrag->sampletree);
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

struct ddsi_reorder {
  ddsrt_avl_tree_t sampleivtree;
  struct ddsi_rsample *max_sampleiv; /* = max(sampleivtree) */
  ddsi_seqno_t next_seq;
  enum ddsi_reorder_mode mode;
  uint32_t max_samples;
  uint32_t n_samples;
  uint64_t discarded_bytes;
  const struct ddsrt_log_cfg *logcfg;
  bool late_ack_mode;
  bool trace;
};

static const ddsrt_avl_treedef_t reorder_sampleivtree_treedef =
  DDSRT_AVL_TREEDEF_INITIALIZER (offsetof (struct ddsi_rsample, u.reorder.avlnode), offsetof (struct ddsi_rsample, u.reorder.min), compare_seqno, 0);

struct ddsi_reorder *ddsi_reorder_new (const struct ddsrt_log_cfg *logcfg, enum ddsi_reorder_mode mode, uint32_t max_samples, bool late_ack_mode)
{
  struct ddsi_reorder *r;
  if ((r = ddsrt_malloc (sizeof (*r))) == NULL)
    return NULL;
  ddsrt_avl_init (&reorder_sampleivtree_treedef, &r->sampleivtree);
  r->max_sampleiv = NULL;
  r->next_seq = 1;
  r->mode = mode;
  r->max_samples = max_samples;
  r->n_samples = 0;
  r->discarded_bytes = 0;
  r->late_ack_mode = late_ack_mode;
  r->logcfg = logcfg;
  r->trace = (logcfg->c.mask & DDS_LC_RADMIN) != 0;
  return r;
}

void ddsi_reorder_stats (struct ddsi_reorder *reorder, uint64_t *discarded_bytes)
{
  *discarded_bytes = reorder->discarded_bytes;
}

void ddsi_fragchain_unref (struct ddsi_rdata *frag)
{
  struct ddsi_rdata *frag1;
  while (frag)
  {
    frag1 = frag->nextfrag;
    ddsi_rdata_unref (frag);
    frag = frag1;
  }
}

void ddsi_reorder_free (struct ddsi_reorder *r)
{
  struct ddsi_rsample *iv;
  struct ddsi_rsample_chain_elem *sce;
  /* FXIME: instead of findmin/delete, a treewalk can be used. */
  iv = ddsrt_avl_find_min (&reorder_sampleivtree_treedef, &r->sampleivtree);
  while (iv)
  {
    ddsrt_avl_delete (&reorder_sampleivtree_treedef, &r->sampleivtree, iv);
    sce = iv->u.reorder.sc.first;
    while (sce)
    {
      struct ddsi_rsample_chain_elem *sce1 = sce->next;
      ddsi_fragchain_unref (sce->fragchain);
      sce = sce1;
    }
    iv = ddsrt_avl_find_min (&reorder_sampleivtree_treedef, &r->sampleivtree);
  }
  ddsrt_free (r);
}

static void reorder_add_rsampleiv (struct ddsi_reorder *reorder, struct ddsi_rsample *rsample)
{
  ddsrt_avl_ipath_t path;
  if (ddsrt_avl_lookup_ipath (&reorder_sampleivtree_treedef, &reorder->sampleivtree, &rsample->u.reorder.min, &path) != NULL)
    assert (0);
  ddsrt_avl_insert_ipath (&reorder_sampleivtree_treedef, &reorder->sampleivtree, rsample, &path);
}

#ifndef NDEBUG
static int rsample_is_singleton (const struct ddsi_rsample_reorder *s)
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

static void append_rsample_interval (struct ddsi_rsample *a, struct ddsi_rsample *b)
{
  a->u.reorder.sc.last->next = b->u.reorder.sc.first;
  a->u.reorder.sc.last = b->u.reorder.sc.last;
  a->u.reorder.maxp1 = b->u.reorder.maxp1;
  a->u.reorder.n_samples += b->u.reorder.n_samples;
}

static int reorder_try_append_and_discard (struct ddsi_reorder *reorder, struct ddsi_rsample *appendto, struct ddsi_rsample *todiscard)
{
  if (todiscard == NULL)
  {
    TRACE (reorder, "  try_append_and_discard: fail: todiscard = NULL\n");
    return 0;
  }
  else if (appendto->u.reorder.maxp1 < todiscard->u.reorder.min)
  {
    TRACE (reorder, "  try_append_and_discard: fail: appendto = [%"PRIu64",%"PRIu64") @ %p, "
           "todiscard = [%"PRIu64",%"PRIu64") @ %p - gap\n",
           appendto->u.reorder.min, appendto->u.reorder.maxp1, (void *) appendto,
           todiscard->u.reorder.min, todiscard->u.reorder.maxp1, (void *) todiscard);
    return 0;
  }
  else
  {
    TRACE (reorder, "  try_append_and_discard: success: appendto = [%"PRIu64",%"PRIu64") @ %p, "
           "todiscard = [%"PRIu64",%"PRIu64") @ %p\n",
           appendto->u.reorder.min, appendto->u.reorder.maxp1, (void *) appendto,
           todiscard->u.reorder.min, todiscard->u.reorder.maxp1, (void *) todiscard);
    assert (todiscard->u.reorder.min == appendto->u.reorder.maxp1);
    ddsrt_avl_delete (&reorder_sampleivtree_treedef, &reorder->sampleivtree, todiscard);
    append_rsample_interval (appendto, todiscard);
    TRACE (reorder, "  try_append_and_discard: max_sampleiv needs update? %s\n",
           (todiscard == reorder->max_sampleiv) ? "yes" : "no");
    /* Inform caller whether reorder->max must be updated -- the
       expected thing to do is to update it to appendto here, but that
       fails if appendto isn't actually in the tree.  And that happens
       to be the fast path where the sample that comes in has the
       sequence number we expected. */
    return todiscard == reorder->max_sampleiv;
  }
}

struct ddsi_rsample *ddsi_reorder_rsample_dup_first (struct ddsi_rmsg *rmsg, struct ddsi_rsample *rsampleiv)
{
  /* Duplicates the rsampleiv without updating any reference counts:
     that is left to the caller, as they do not need to be updated if
     the duplicate ultimately doesn't get used.

     The rmsg is the one to allocate from, and must be the one
     currently being processed (one can only allocate memory from an
     uncommitted rmsg) and must be referenced by an rdata in
     rsampleiv. */
  struct ddsi_rsample *rsampleiv_new;
  struct ddsi_rsample_chain_elem *sce;
#ifndef NDEBUG
  {
    struct ddsi_rdata *d = rsampleiv->u.reorder.sc.first->fragchain;
    while (d && d->rmsg != rmsg)
      d = d->nextfrag;
    assert (d != NULL);
  }
#endif
  if ((rsampleiv_new = ddsi_rmsg_alloc (rmsg, sizeof (*rsampleiv_new))) == NULL)
    return NULL;
  if ((sce = ddsi_rmsg_alloc (rmsg, sizeof (*sce))) == NULL)
    return NULL;
  sce->fragchain = rsampleiv->u.reorder.sc.first->fragchain;
  sce->next = NULL;
  sce->sampleinfo = rsampleiv->u.reorder.sc.first->sampleinfo;
  rsampleiv_new->u.reorder.min = rsampleiv->u.reorder.min;
  rsampleiv_new->u.reorder.maxp1 = rsampleiv_new->u.reorder.min + 1;
  rsampleiv_new->u.reorder.n_samples = 1;
  rsampleiv_new->u.reorder.sc.first = rsampleiv_new->u.reorder.sc.last = sce;
  return rsampleiv_new;
}

struct ddsi_rdata *ddsi_rsample_fragchain (struct ddsi_rsample *rsample)
{
  assert (rsample_is_singleton (&rsample->u.reorder));
  return rsample->u.reorder.sc.first->fragchain;
}

static char reorder_mode_as_char (const struct ddsi_reorder *reorder)
{
  switch (reorder->mode)
  {
    case DDSI_REORDER_MODE_NORMAL: return 'R';
    case DDSI_REORDER_MODE_MONOTONICALLY_INCREASING: return 'U';
    case DDSI_REORDER_MODE_ALWAYS_DELIVER: return 'A';
  }
  assert (0);
  return '?';
}

static void delete_last_sample (struct ddsi_reorder *reorder)
{
  struct ddsi_rsample_reorder *last = &reorder->max_sampleiv->u.reorder;
  struct ddsi_rdata *fragchain;

  /* This just removes it, it doesn't adjust the count. It is not
     supposed to be called on an radmin with only one sample. */
  assert (reorder->n_samples > 0);
  assert (reorder->max_sampleiv != NULL);

  if (last->sc.first == last->sc.last)
  {
    /* Last sample is in an interval of its own - delete it, and
       recalc max_sampleiv. */
    TRACE (reorder, "  delete_last_sample: in singleton interval\n");
    if (last->sc.first->sampleinfo)
      reorder->discarded_bytes += last->sc.first->sampleinfo->size;
    fragchain = last->sc.first->fragchain;
    ddsrt_avl_delete (&reorder_sampleivtree_treedef, &reorder->sampleivtree, reorder->max_sampleiv);
    reorder->max_sampleiv = ddsrt_avl_find_max (&reorder_sampleivtree_treedef, &reorder->sampleivtree);
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
    struct ddsi_rsample_chain_elem *e, *pe;
    TRACE (reorder, "  delete_last_sample: scanning last interval [%"PRIu64"..%"PRIu64")\n", last->min, last->maxp1);
    assert (last->n_samples >= 1);
    assert (last->min + last->n_samples <= last->maxp1);
    e = last->sc.first;
    do {
      pe = e;
      e = e->next;
    } while (e != last->sc.last);
    if (e->sampleinfo)
      reorder->discarded_bytes += e->sampleinfo->size;
    fragchain = e->fragchain;
    pe->next = NULL;
    assert (pe->sampleinfo == NULL || pe->sampleinfo->seq + 1 < last->maxp1);
    last->sc.last = pe;
    last->maxp1--;
    last->n_samples--;
  }

  ddsi_fragchain_unref (fragchain);
}

ddsi_reorder_result_t ddsi_reorder_rsample (struct ddsi_rsample_chain *sc, struct ddsi_reorder *reorder, struct ddsi_rsample *rsampleiv, int *refcount_adjust, int delivery_queue_full_p)
{
  /* Adds an rsample (represented as an interval) to the reorder admin
     and returns the chain of consecutive samples ready for delivery
     because of the insertion.  Consequently, if it returns a sample
     chain, the sample referenced by rsampleiv is the first in the
     chain.

     refcount_adjust is incremented if the sample is not discarded. */
  struct ddsi_rsample_reorder *s = &rsampleiv->u.reorder;

  TRACE (reorder, "reorder_sample(%p %c, %"PRIu64" @ %p) expecting %"PRIu64":\n",
         (void *) reorder, reorder_mode_as_char (reorder), rsampleiv->u.reorder.min,
         (void *) rsampleiv, reorder->next_seq);

  /* Incoming rsample must be a singleton */
  assert (rsample_is_singleton (s));

  /* Reorder must not contain samples with sequence numbers <= next
     seq; max must be set iff the reorder is non-empty. */
#ifndef NDEBUG
  {
    struct ddsi_rsample *min = ddsrt_avl_find_min (&reorder_sampleivtree_treedef, &reorder->sampleivtree);
    if (min)
      TRACE (reorder, "  min = %"PRIu64" @ %p\n", min->u.reorder.min, (void *) min);
    assert (min == NULL || reorder->next_seq < min->u.reorder.min);
    assert ((reorder->max_sampleiv == NULL && min == NULL) ||
            (reorder->max_sampleiv != NULL && min != NULL));
  }
#endif
  assert ((!!ddsrt_avl_is_empty (&reorder->sampleivtree)) == (reorder->max_sampleiv == NULL));
  assert (reorder->max_sampleiv == NULL || reorder->max_sampleiv == ddsrt_avl_find_max (&reorder_sampleivtree_treedef, &reorder->sampleivtree));
  assert (reorder->n_samples <= reorder->max_samples);
  if (reorder->max_sampleiv)
    TRACE (reorder, "  max = [%"PRIu64",%"PRIu64") @ %p\n", reorder->max_sampleiv->u.reorder.min,
           reorder->max_sampleiv->u.reorder.maxp1, (void *) reorder->max_sampleiv);

  if (s->min == reorder->next_seq ||
      (s->min > reorder->next_seq && reorder->mode == DDSI_REORDER_MODE_MONOTONICALLY_INCREASING) ||
      reorder->mode == DDSI_REORDER_MODE_ALWAYS_DELIVER)
  {
    /* Can deliver at least one sample, but that appends samples to
       the delivery queue.  If delivery_queue_full_p is set, the delivery
       queue has hit its maximum length, so appending to it isn't such
       a great idea.  Therefore, we simply reject the sample.  (We
       have to, we can't have a deliverable sample in the reorder
       admin, or things go wrong very quickly.) */
    if (delivery_queue_full_p)
    {
      TRACE (reorder, "  discarding deliverable sample: delivery queue is full\n");
      reorder->discarded_bytes += s->sc.first->sampleinfo->size;
      return DDSI_REORDER_REJECT;
    }

    /* 's' is next sample to be delivered; maybe we can append the
       first interval in the tree to it.  We can avoid all processing
       if the index is empty, which is the normal case.  Unreliable
       out-of-order either ends up here or in discard.)  */
    if (reorder->max_sampleiv != NULL)
    {
      struct ddsi_rsample *min = ddsrt_avl_find_min (&reorder_sampleivtree_treedef, &reorder->sampleivtree);
      TRACE (reorder, "  try append_and_discard\n");
      if (reorder_try_append_and_discard (reorder, rsampleiv, min))
        reorder->max_sampleiv = NULL;
    }
    reorder->next_seq = s->maxp1;
    *sc = rsampleiv->u.reorder.sc;
    (*refcount_adjust)++;
    TRACE (reorder, "  return [%"PRIu64",%"PRIu64")\n", s->min, s->maxp1);

    /* Adjust reorder->n_samples, new sample is not counted yet */
    assert (s->maxp1 - s->min >= 1);
    assert (s->maxp1 - s->min <= (int) INT32_MAX);
    assert (s->min + s->n_samples <= s->maxp1);
    assert (reorder->n_samples >= s->n_samples - 1);
    reorder->n_samples -= s->n_samples - 1;
    return (ddsi_reorder_result_t) s->n_samples;
  }
  else if (s->min < reorder->next_seq)
  {
    /* we've moved beyond this one: discard it; no need to adjust
       n_samples */
    TRACE (reorder, "  discard: too old\n");
    reorder->discarded_bytes += s->sc.first->sampleinfo->size;
    return DDSI_REORDER_TOO_OLD; /* don't want refcount increment */
  }
  else if (ddsrt_avl_is_empty (&reorder->sampleivtree))
  {
    /* else, if nothing's stored simply add this one, max_samples = 0
       is technically allowed, and potentially useful, so check for
       it */
    assert (reorder->n_samples == 0);
    TRACE (reorder, "  adding to empty store\n");
    if (reorder->max_samples == 0)
    {
      TRACE (reorder, "  NOT - max_samples hit\n");
      reorder->discarded_bytes += s->sc.first->sampleinfo->size;
      return DDSI_REORDER_REJECT;
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
      TRACE (reorder, "  discarding sample: only accepting delayed samples due to backlog in delivery queue\n");
      reorder->discarded_bytes += s->sc.first->sampleinfo->size;
      return DDSI_REORDER_REJECT;
    }

    /* grow the last interval, if we're still accepting samples */
    TRACE (reorder, "  growing last interval\n");
    if (reorder->n_samples < reorder->max_samples)
    {
      append_rsample_interval (reorder->max_sampleiv, rsampleiv);
      reorder->n_samples++;
    }
    else
    {
      TRACE (reorder, "  discarding sample: max_samples reached and sample at end\n");
      reorder->discarded_bytes += s->sc.first->sampleinfo->size;
      return DDSI_REORDER_REJECT;
    }
  }
  else if (s->min > reorder->max_sampleiv->u.reorder.maxp1)
  {
    if (delivery_queue_full_p)
    {
      /* new interval at the end will not be accepted when this flag is set */
      TRACE (reorder, "  discarding sample: only accepting delayed samples due to backlog in delivery queue\n");
      reorder->discarded_bytes += s->sc.first->sampleinfo->size;
      return DDSI_REORDER_REJECT;
    }
    if (reorder->n_samples < reorder->max_samples)
    {
      TRACE (reorder, "  new interval at end\n");
      reorder_add_rsampleiv (reorder, rsampleiv);
      reorder->max_sampleiv = rsampleiv;
      reorder->n_samples++;
    }
    else
    {
      TRACE (reorder, "  discarding sample: max_samples reached and sample at end\n");
      reorder->discarded_bytes += s->sc.first->sampleinfo->size;
      return DDSI_REORDER_REJECT;
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
    struct ddsi_rsample *predeq, *immsucc;
    TRACE (reorder, "  hard case ...\n");

    if (reorder->late_ack_mode && delivery_queue_full_p)
    {
      TRACE (reorder, "  discarding sample: delivery queue full\n");
      reorder->discarded_bytes += s->sc.first->sampleinfo->size;
      return DDSI_REORDER_REJECT;
    }

    predeq = ddsrt_avl_lookup_pred_eq (&reorder_sampleivtree_treedef, &reorder->sampleivtree, &s->min);
    if (predeq)
      TRACE (reorder, "  predeq = [%"PRIu64",%"PRIu64") @ %p\n",
             predeq->u.reorder.min, predeq->u.reorder.maxp1, (void *) predeq);
    else
      TRACE (reorder, "  predeq = null\n");
    if (predeq && s->min >= predeq->u.reorder.min && s->min < predeq->u.reorder.maxp1)
    {
      /* contained in predeq */
      TRACE (reorder, "  discard: contained in predeq\n");
      reorder->discarded_bytes += s->sc.first->sampleinfo->size;
      return DDSI_REORDER_REJECT;
    }

    immsucc = ddsrt_avl_lookup (&reorder_sampleivtree_treedef, &reorder->sampleivtree, &s->maxp1);
    if (immsucc)
      TRACE (reorder, "  immsucc = [%"PRIu64",%"PRIu64") @ %p\n",
             immsucc->u.reorder.min, immsucc->u.reorder.maxp1, (void *) immsucc);
    else
      TRACE (reorder, "  immsucc = null\n");
    if (predeq && s->min == predeq->u.reorder.maxp1)
    {
      /* grow predeq at end, and maybe append immsucc as well */
      TRACE (reorder, "  growing predeq at end ...\n");
      append_rsample_interval (predeq, rsampleiv);
      if (reorder_try_append_and_discard (reorder, predeq, immsucc))
        reorder->max_sampleiv = predeq;
    }
    else if (immsucc)
    {
      /* no predecessor, grow immsucc at head, which _does_ alter the
         key of the node in the tree, but _doesn't_ change the tree's
         structure. */
      TRACE (reorder, "  growing immsucc at head\n");
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
      ddsrt_avl_swap_node (&reorder_sampleivtree_treedef, &reorder->sampleivtree, immsucc, rsampleiv);
      if (immsucc == reorder->max_sampleiv)
        reorder->max_sampleiv = rsampleiv;
    }
    else
    {
      /* neither extends predeq nor immsucc */
      TRACE (reorder, "  new interval\n");
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
  return DDSI_REORDER_ACCEPT;
}

static struct ddsi_rsample *coalesce_intervals_touching_range (struct ddsi_reorder *reorder, ddsi_seqno_t min, ddsi_seqno_t maxp1, int *valuable)
{
  struct ddsi_rsample *s, *t;
  *valuable = 0;
  /* Find first (lowest m) interval [m,n) s.t. n >= min && m <= maxp1 */
  s = ddsrt_avl_lookup_pred_eq (&reorder_sampleivtree_treedef, &reorder->sampleivtree, &min);
  if (s && s->u.reorder.maxp1 >= min)
  {
    /* m <= min && n >= min (note: pred of s [m',n') necessarily has n' < m) */
#ifndef NDEBUG
    struct ddsi_rsample *q = ddsrt_avl_find_pred (&reorder_sampleivtree_treedef, &reorder->sampleivtree, s);
    assert (q == NULL || q->u.reorder.maxp1 < min);
#endif
  }
  else
  {
    /* No good, but the first (if s = NULL) or the next one (if s !=
       NULL) may still have m <= maxp1 (m > min is implied now).  If
       not, no such interval.  */
    s = ddsrt_avl_find_succ (&reorder_sampleivtree_treedef, &reorder->sampleivtree, s);
    if (!(s && s->u.reorder.min <= maxp1))
      return NULL;
  }
  /* Append successors [m',n') s.t. m' <= maxp1 to s */
  assert (s->u.reorder.min + s->u.reorder.n_samples <= s->u.reorder.maxp1);
  while ((t = ddsrt_avl_find_succ (&reorder_sampleivtree_treedef, &reorder->sampleivtree, s)) != NULL && t->u.reorder.min <= maxp1)
  {
    ddsrt_avl_delete (&reorder_sampleivtree_treedef, &reorder->sampleivtree, t);
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

struct ddsi_rdata *ddsi_rdata_newgap (struct ddsi_rmsg *rmsg)
{
  struct ddsi_rdata *d;
  if ((d = ddsi_rdata_new (rmsg, 0, 0, 0, 0, 0)) == NULL)
    return NULL;
  ddsi_rdata_addbias (d);
  return d;
}

static int reorder_insert_gap (struct ddsi_reorder *reorder, struct ddsi_rdata *rdata, ddsi_seqno_t min, ddsi_seqno_t maxp1)
{
  struct ddsi_rsample_chain_elem *sce;
  struct ddsi_rsample *s;
  ddsrt_avl_ipath_t path;
  if (ddsrt_avl_lookup_ipath (&reorder_sampleivtree_treedef, &reorder->sampleivtree, &min, &path) != NULL)
    assert (0);
  if ((sce = ddsi_rmsg_alloc (rdata->rmsg, sizeof (*sce))) == NULL)
    return 0;
  sce->fragchain = rdata;
  sce->next = NULL;
  sce->sampleinfo = NULL;
  if ((s = ddsi_rmsg_alloc (rdata->rmsg, sizeof (*s))) == NULL)
    return 0;
  s->u.reorder.sc.first = s->u.reorder.sc.last = sce;
  s->u.reorder.min = min;
  s->u.reorder.maxp1 = maxp1;
  s->u.reorder.n_samples = 1;
  ddsrt_avl_insert_ipath (&reorder_sampleivtree_treedef, &reorder->sampleivtree, s, &path);
  return 1;
}

ddsi_reorder_result_t ddsi_reorder_gap (struct ddsi_rsample_chain *sc, struct ddsi_reorder *reorder, struct ddsi_rdata *rdata, ddsi_seqno_t min, ddsi_seqno_t maxp1, int *refcount_adjust)
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
  struct ddsi_rsample *coalesced;
  int valuable;

  TRACE (reorder, "reorder_gap(%p %c, [%"PRIu64",%"PRIu64") data %p) expecting %"PRIu64":\n",
         (void *) reorder, reorder_mode_as_char (reorder),
         min, maxp1, (void *) rdata, reorder->next_seq);

  if (maxp1 <= reorder->next_seq)
  {
    TRACE (reorder, "  too old\n");
    return DDSI_REORDER_TOO_OLD;
  }
  if (reorder->mode != DDSI_REORDER_MODE_NORMAL)
  {
    TRACE (reorder, "  special mode => don't care\n");
    return DDSI_REORDER_REJECT;
  }

  /* Coalesce all intervals [m,n) with n >= min or m <= maxp1 */
  if ((coalesced = coalesce_intervals_touching_range (reorder, min, maxp1, &valuable)) == NULL)
  {
    ddsi_reorder_result_t res;
    TRACE (reorder, "  coalesced = null\n");
    if (min <= reorder->next_seq)
    {
      TRACE (reorder, "  next expected: %"PRIu64"\n", maxp1);
      reorder->next_seq = maxp1;
      res = DDSI_REORDER_ACCEPT;
    }
    else if (reorder->n_samples == reorder->max_samples &&
             (reorder->max_sampleiv == NULL || min > reorder->max_sampleiv->u.reorder.maxp1))
    {
      /* n_samples = max_samples => (max_sampleiv = NULL <=> max_samples = 0) */
      TRACE (reorder, "  discarding gap: max_samples reached and gap at end\n");
      res = DDSI_REORDER_REJECT;
    }
    else if (!reorder_insert_gap (reorder, rdata, min, maxp1))
    {
      TRACE (reorder, "  store gap failed: no memory\n");
      res = DDSI_REORDER_REJECT;
    }
    else
    {
      TRACE (reorder, "  storing gap\n");
      res = DDSI_REORDER_ACCEPT;
      /* do not let radmin grow beyond max_samples; there is a small
         possibility that we insert it & delete it immediately
         afterward. */
      if (reorder->n_samples < reorder->max_samples)
        reorder->n_samples++;
      else
        delete_last_sample (reorder);
      (*refcount_adjust)++;
    }
    reorder->max_sampleiv = ddsrt_avl_find_max (&reorder_sampleivtree_treedef, &reorder->sampleivtree);
    return res;
  }
  else if (coalesced->u.reorder.min <= reorder->next_seq)
  {
    TRACE (reorder, "  coalesced = [%"PRIu64",%"PRIu64") @ %p containing %"PRId32" samples\n",
           coalesced->u.reorder.min, coalesced->u.reorder.maxp1,
           (void *) coalesced, coalesced->u.reorder.n_samples);
    ddsrt_avl_delete (&reorder_sampleivtree_treedef, &reorder->sampleivtree, coalesced);
    if (coalesced->u.reorder.min <= reorder->next_seq)
      assert (min <= reorder->next_seq);
    reorder->next_seq = coalesced->u.reorder.maxp1;
    reorder->max_sampleiv = ddsrt_avl_find_max (&reorder_sampleivtree_treedef, &reorder->sampleivtree);
    TRACE (reorder, "  next expected: %"PRIu64"\n", reorder->next_seq);
    *sc = coalesced->u.reorder.sc;

    /* Adjust n_samples */
    assert (coalesced->u.reorder.min + coalesced->u.reorder.n_samples <= coalesced->u.reorder.maxp1);
    assert (reorder->n_samples >= coalesced->u.reorder.n_samples);
    reorder->n_samples -= coalesced->u.reorder.n_samples;
    return (ddsi_reorder_result_t) coalesced->u.reorder.n_samples;
  }
  else
  {
    TRACE (reorder, "  coalesced = [%"PRIu64",%"PRIu64") @ %p - that is all\n",
           coalesced->u.reorder.min, coalesced->u.reorder.maxp1, (void *) coalesced);
    reorder->max_sampleiv = ddsrt_avl_find_max (&reorder_sampleivtree_treedef, &reorder->sampleivtree);
    return valuable ? DDSI_REORDER_ACCEPT : DDSI_REORDER_REJECT;
  }
}

void ddsi_reorder_drop_upto (struct ddsi_reorder *reorder, ddsi_seqno_t maxp1)
{
  // ddsi_reorder_gap returns the chain of available samples starting with the first
  // sequence number in the gap interval and ending at the highest sequence number
  // >= maxp1 for which all sequence numbers starting from maxp1 are present.
  // Requiring that no samples are present beyond maxp1 means we're not dropping
  // too much.  That's good enough for the current purpose.
  assert (reorder->max_sampleiv == NULL || reorder->max_sampleiv->u.reorder.maxp1 <= maxp1);
  // gap won't be stored, so can safely be stack-allocated for the purpose of calling
  // ddsi_reorder_gap
  struct ddsi_rdata gap = {
    .rmsg = NULL, .nextfrag = NULL, .min = 0, .maxp1 = 0, .submsg_zoff = 0, .payload_zoff = 0
#ifndef NDEBUG
    , .refcount_bias_added = DDSRT_ATOMIC_UINT32_INIT (0)
#endif
  };
  struct ddsi_rsample_chain sc;
  int refc_adjust = 0;
  if (ddsi_reorder_gap (&sc, reorder, &gap, 1, maxp1, &refc_adjust) > 0)
  {
    while (sc.first)
    {
      struct ddsi_rsample_chain_elem *e = sc.first;
      sc.first = e->next;
      ddsi_fragchain_unref (e->fragchain);
    }
  }
  assert (refc_adjust == 0 && !ddsrt_atomic_ld32 (&gap.refcount_bias_added));
  assert (ddsi_reorder_next_seq (reorder) >= maxp1);
}

int ddsi_reorder_wantsample (const struct ddsi_reorder *reorder, ddsi_seqno_t seq)
{
  struct ddsi_rsample *s;
  if (seq < reorder->next_seq)
    /* trivially not interesting */
    return 0;
  /* Find interval that contains seq, if we know seq.  We are
     interested if seq is outside this interval (if any). */
  s = ddsrt_avl_lookup_pred_eq (&reorder_sampleivtree_treedef, &reorder->sampleivtree, &seq);
  return (s == NULL || s->u.reorder.maxp1 <= seq);
}

unsigned ddsi_reorder_nackmap (const struct ddsi_reorder *reorder, ddsi_seqno_t base, ddsi_seqno_t maxseq, struct ddsi_sequence_number_set_header *map, uint32_t *mapbits, uint32_t maxsz, int notail)
{
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
    DDS_CERROR (reorder->logcfg, "ddsi_reorder_nackmap: incorrect base sequence number supplied (%"PRIu64" > %"PRIu64")\n", base, reorder->next_seq);
    base = reorder->next_seq;
  }
#endif
  if (maxseq + 1 < base)
  {
    DDS_CERROR (reorder->logcfg, "ddsi_reorder_nackmap: incorrect max sequence number supplied (maxseq %"PRIu64" base %"PRIu64")\n", maxseq, base);
    maxseq = base - 1;
  }

  map->bitmap_base = ddsi_to_seqno (base);
  if (maxseq + 1 - base > maxsz)
    map->numbits = maxsz;
  else
    map->numbits = (uint32_t) (maxseq + 1 - base);
  ddsi_bitset_zero (map->numbits, mapbits);

  struct ddsi_rsample *iv = ddsrt_avl_find_min (&reorder_sampleivtree_treedef, &reorder->sampleivtree);
  assert (iv == NULL || iv->u.reorder.min > base);
  ddsi_seqno_t i = base;
  while (iv && i < base + map->numbits)
  {
    for (; i < base + map->numbits && i < iv->u.reorder.min; i++)
    {
      uint32_t x = (uint32_t) (i - base);
      ddsi_bitset_set (map->numbits, mapbits, x);
    }
    i = iv->u.reorder.maxp1;
    iv = ddsrt_avl_find_succ (&reorder_sampleivtree_treedef, &reorder->sampleivtree, iv);
  }
  if (notail && i < base + map->numbits)
    map->numbits = (uint32_t) (i - base);
  else
  {
    for (; i < base + map->numbits; i++)
    {
      uint32_t x = (uint32_t) (i - base);
      ddsi_bitset_set (map->numbits, mapbits, x);
    }
  }
  return map->numbits;
}

ddsi_seqno_t ddsi_reorder_next_seq (const struct ddsi_reorder *reorder)
{
  return reorder->next_seq;
}

void ddsi_reorder_set_next_seq (struct ddsi_reorder *reorder, ddsi_seqno_t seq)
{
  reorder->next_seq = seq;
}

/* DQUEUE -------------------------------------------------------------- */

struct ddsi_dqueue {
  ddsrt_mutex_t lock;
  ddsrt_cond_t cond;
  ddsi_dqueue_handler_t handler;
  void *handler_arg;

  struct ddsi_rsample_chain sc;

  struct ddsi_thread_state *thrst;
  struct ddsi_domaingv *gv;
  char *name;
  uint32_t max_samples;
  ddsrt_atomic_uint32_t nof_samples;
};

enum dqueue_elem_kind {
  DQEK_DATA,
  DQEK_GAP,
  DQEK_BUBBLE
};

enum ddsi_dqueue_bubble_kind {
  DDSI_DQBK_STOP, /* _not_ ddsrt_malloc()ed! */
  DDSI_DQBK_CALLBACK,
  DDSI_DQBK_RDGUID
};

struct ddsi_dqueue_bubble {
  /* sample_chain_elem must be first: and is used to link it into the
     queue, with the sampleinfo pointing to itself, but mangled */
  struct ddsi_rsample_chain_elem sce;

  enum ddsi_dqueue_bubble_kind kind;
  union {
    /* stop */
    struct {
      ddsi_dqueue_callback_t cb;
      void *arg;
    } cb;
    struct {
      ddsi_guid_t rdguid;
      uint32_t count;
    } rdguid;
  } u;
};

static enum dqueue_elem_kind dqueue_elem_kind (const struct ddsi_rsample_chain_elem *e)
{
  if (e->sampleinfo == NULL)
    return DQEK_GAP;
  else if ((char *) e->sampleinfo != (char *) e)
    return DQEK_DATA;
  else
    return DQEK_BUBBLE;
}

static uint32_t dqueue_thread (struct ddsi_dqueue *q)
{
  struct ddsi_thread_state * const thrst = ddsi_lookup_thread_state ();
#if DDSRT_HAVE_RUSAGE
  struct ddsi_domaingv const * const gv = ddsrt_atomic_ldvoidp (&thrst->gv);
#endif
  ddsrt_mtime_t next_thread_cputime = { 0 };
  int keepgoing = 1;
  ddsi_guid_t rdguid, *prdguid = NULL;
  uint32_t rdguid_count = 0;

  ddsrt_mutex_lock (&q->lock);
  while (keepgoing)
  {
    struct ddsi_rsample_chain sc;

    LOG_THREAD_CPUTIME (&gv->logconfig, next_thread_cputime);

    if (q->sc.first == NULL)
      ddsrt_cond_wait (&q->cond, &q->lock);
    sc = q->sc;
    q->sc.first = q->sc.last = NULL;
    ddsrt_mutex_unlock (&q->lock);

    ddsi_thread_state_awake_fixed_domain (thrst);
    while (sc.first)
    {
      struct ddsi_rsample_chain_elem *e = sc.first;
      int ret;
      sc.first = e->next;
      if (ddsrt_atomic_dec32_ov (&q->nof_samples) == 1) {
        ddsrt_cond_broadcast (&q->cond);
      }
      ddsi_thread_state_awake_to_awake_no_nest (thrst);
      switch (dqueue_elem_kind (e))
      {
        case DQEK_DATA:
          ret = q->handler (e->sampleinfo, e->fragchain, prdguid, q->handler_arg);
          (void) ret; /* eliminate set-but-not-used in NDEBUG case */
          assert (ret == 0); /* so every handler will return 0 */
          /* FALLS THROUGH */
        case DQEK_GAP:
          ddsi_fragchain_unref (e->fragchain);
          if (rdguid_count > 0)
          {
            if (--rdguid_count == 0)
              prdguid = NULL;
          }
          break;

        case DQEK_BUBBLE:
          {
            struct ddsi_dqueue_bubble *b = (struct ddsi_dqueue_bubble *) e->sampleinfo;
            if (b->kind == DDSI_DQBK_STOP)
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
                case DDSI_DQBK_STOP:
                  abort ();
                case DDSI_DQBK_CALLBACK:
                  b->u.cb.cb (b->u.cb.arg);
                  break;
                case DDSI_DQBK_RDGUID:
                  rdguid = b->u.rdguid.rdguid;
                  rdguid_count = b->u.rdguid.count;
                  prdguid = &rdguid;
                  break;
              }
              ddsrt_free (b);
            }
            break;
          }
      }
    }

    ddsi_thread_state_asleep (thrst);
    ddsrt_mutex_lock (&q->lock);
  }
  ddsrt_mutex_unlock (&q->lock);
  return 0;
}

struct ddsi_dqueue *ddsi_dqueue_new (const char *name, const struct ddsi_domaingv *gv, uint32_t max_samples, ddsi_dqueue_handler_t handler, void *arg)
{
  struct ddsi_dqueue *q;

  if ((q = ddsrt_malloc (sizeof (*q))) == NULL)
    goto fail_q;
  if ((q->name = ddsrt_strdup (name)) == NULL)
    goto fail_name;
  q->max_samples = max_samples;
  ddsrt_atomic_st32 (&q->nof_samples, 0);
  q->handler = handler;
  q->handler_arg = arg;
  q->sc.first = q->sc.last = NULL;
  q->gv = (struct ddsi_domaingv *) gv;
  q->thrst = NULL;

  ddsrt_mutex_init (&q->lock);
  ddsrt_cond_init (&q->cond);

  return q;
 fail_name:
  ddsrt_free (q);
 fail_q:
  return NULL;
}

bool ddsi_dqueue_start (struct ddsi_dqueue *q)
{
  char *thrname;
  size_t thrnamesz;
  thrnamesz = 3 + strlen (q->name) + 1;
  if ((thrname = ddsrt_malloc (thrnamesz)) == NULL)
    return false;
  (void) snprintf (thrname, thrnamesz, "dq.%s", q->name);
  dds_return_t ret = ddsi_create_thread (&q->thrst, q->gv, thrname, (uint32_t (*) (void *)) dqueue_thread, q);
  ddsrt_free (thrname);
  return ret == DDS_RETCODE_OK;
}

static int ddsi_dqueue_enqueue_locked (struct ddsi_dqueue *q, struct ddsi_rsample_chain *sc)
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

bool ddsi_dqueue_enqueue_deferred_wakeup (struct ddsi_dqueue *q, struct ddsi_rsample_chain *sc, ddsi_reorder_result_t rres)
{
  bool signal;
  assert (rres > 0);
  assert (sc->first);
  assert (sc->last->next == NULL);
  ddsrt_mutex_lock (&q->lock);
  ddsrt_atomic_add32 (&q->nof_samples, (uint32_t) rres);
  signal = ddsi_dqueue_enqueue_locked (q, sc);
  ddsrt_mutex_unlock (&q->lock);
  return signal;
}

void ddsi_dqueue_enqueue_trigger (struct ddsi_dqueue *q)
{
  ddsrt_mutex_lock (&q->lock);
  ddsrt_cond_broadcast (&q->cond);
  ddsrt_mutex_unlock (&q->lock);
}

void ddsi_dqueue_enqueue (struct ddsi_dqueue *q, struct ddsi_rsample_chain *sc, ddsi_reorder_result_t rres)
{
  assert (rres > 0);
  assert (sc->first);
  assert (sc->last->next == NULL);
  ddsrt_mutex_lock (&q->lock);
  ddsrt_atomic_add32 (&q->nof_samples, (uint32_t) rres);
  if (ddsi_dqueue_enqueue_locked (q, sc))
    ddsrt_cond_broadcast (&q->cond);
  ddsrt_mutex_unlock (&q->lock);
}

static int ddsi_dqueue_enqueue_bubble_locked (struct ddsi_dqueue *q, struct ddsi_dqueue_bubble *b)
{
  struct ddsi_rsample_chain sc;
  b->sce.next = NULL;
  b->sce.fragchain = NULL;
  b->sce.sampleinfo = (struct ddsi_rsample_info *) b;
  sc.first = sc.last = &b->sce;
  return ddsi_dqueue_enqueue_locked (q, &sc);
}

static void ddsi_dqueue_enqueue_bubble (struct ddsi_dqueue *q, struct ddsi_dqueue_bubble *b)
{
  ddsrt_mutex_lock (&q->lock);
  ddsrt_atomic_inc32 (&q->nof_samples);
  if (ddsi_dqueue_enqueue_bubble_locked (q, b))
    ddsrt_cond_broadcast (&q->cond);
  ddsrt_mutex_unlock (&q->lock);
}

void ddsi_dqueue_enqueue_callback (struct ddsi_dqueue *q, ddsi_dqueue_callback_t cb, void *arg)
{
  struct ddsi_dqueue_bubble *b;
  b = ddsrt_malloc (sizeof (*b));
  b->kind = DDSI_DQBK_CALLBACK;
  b->u.cb.cb = cb;
  b->u.cb.arg = arg;
  ddsi_dqueue_enqueue_bubble (q, b);
}

void ddsi_dqueue_enqueue1 (struct ddsi_dqueue *q, const ddsi_guid_t *rdguid, struct ddsi_rsample_chain *sc, ddsi_reorder_result_t rres)
{
  struct ddsi_dqueue_bubble *b;

  b = ddsrt_malloc (sizeof (*b));
  b->kind = DDSI_DQBK_RDGUID;
  b->u.rdguid.rdguid = *rdguid;
  b->u.rdguid.count = (uint32_t) rres;

  assert (rres > 0);
  assert (rdguid != NULL);
  assert (sc->first);
  assert (sc->last->next == NULL);
  ddsrt_mutex_lock (&q->lock);
  ddsrt_atomic_add32 (&q->nof_samples, 1 + (uint32_t) rres);
  if (ddsi_dqueue_enqueue_bubble_locked (q, b))
    ddsrt_cond_broadcast (&q->cond);
  (void) ddsi_dqueue_enqueue_locked (q, sc);
  ddsrt_mutex_unlock (&q->lock);
}

int ddsi_dqueue_is_full (struct ddsi_dqueue *q)
{
  /* Reading nof_samples exactly once. It IS a 32-bit int, so at
     worst we get an old value. That mean: we think it is full when
     it is not, in which case we discard the sample and rely on a
     retransmit; or we think it is not full when it is. But if we
     don't mind the occasional extra sample in the queue (we don't),
     and survive the occasional decision to not queue when it
     could've been queued (we do), it should be ok. */
  const uint32_t count = ddsrt_atomic_ld32 (&q->nof_samples);
  return (count >= q->max_samples);
}

void ddsi_dqueue_wait_until_empty_if_full (struct ddsi_dqueue *q)
{
  const uint32_t count = ddsrt_atomic_ld32 (&q->nof_samples);
  if (count >= q->max_samples)
  {
    ddsrt_mutex_lock (&q->lock);
    /* In case the wakeups are were all deferred */
    ddsrt_cond_broadcast (&q->cond);
    while (ddsrt_atomic_ld32 (&q->nof_samples) > 0)
      ddsrt_cond_wait (&q->cond, &q->lock);
    ddsrt_mutex_unlock (&q->lock);
  }
}

static void dqueue_free_remaining_elements (struct ddsi_dqueue *q)
{
  assert (q->thrst == NULL);
  while (q->sc.first)
  {
    struct ddsi_rsample_chain_elem *e = q->sc.first;
    q->sc.first = e->next;
    switch (dqueue_elem_kind (e))
    {
      case DQEK_DATA:
      case DQEK_GAP:
        ddsi_fragchain_unref (e->fragchain);
        break;
      case DQEK_BUBBLE: {
        struct ddsi_dqueue_bubble *b = (struct ddsi_dqueue_bubble *) e->sampleinfo;
        if (b->kind != DDSI_DQBK_STOP)
          ddsrt_free (b);
        break;
      }
    }
  }
}

void ddsi_dqueue_free (struct ddsi_dqueue *q)
{
  /* There must not be any thread enqueueing things anymore at this
     point.  The stop bubble is special in that it does _not_ get
     malloced or freed, but instead lives on the stack for a little
     while.  It would be a shame to fail in free() due to a lack of
     heap space, would it not? */
  if (q->thrst)
  {
    struct ddsi_dqueue_bubble b;
    b.kind = DDSI_DQBK_STOP;
    ddsi_dqueue_enqueue_bubble (q, &b);

    ddsi_join_thread (q->thrst);
    assert (q->sc.first == NULL);
  }
  else
  {
    dqueue_free_remaining_elements (q);
  }
  ddsrt_cond_destroy (&q->cond);
  ddsrt_mutex_destroy (&q->lock);
  ddsrt_free (q->name);
  ddsrt_free (q);
}

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
#include <assert.h>
#include <stdlib.h>
#include <string.h>

#ifndef _WIN32
#include <limits.h> /* for IOV_MAX */
#endif

#include "os/os.h"

#include "util/ut_avl.h"
#include "util/ut_thread_pool.h"

#include "ddsi/q_protocol.h"
#include "ddsi/q_xqos.h"
#include "ddsi/q_bswap.h"
#include "ddsi/q_rtps.h"
#include "ddsi/q_addrset.h"
#include "ddsi/q_error.h"
#include "ddsi/q_misc.h"
#include "ddsi/q_log.h"
#include "ddsi/q_unused.h"
#include "ddsi/q_xmsg.h"
#include "ddsi/q_config.h"
#include "ddsi/q_entity.h"
#include "ddsi/q_globals.h"
#include "ddsi/q_ephash.h"
#include "ddsi/q_freelist.h"
#include "ddsi/ddsi_serdata_default.h"

#define NN_XMSG_MAX_ALIGN 8
#define NN_XMSG_CHUNK_SIZE 128

struct nn_xmsgpool {
  struct nn_freelist freelist;
};

struct nn_xmsg_data {
  InfoSRC_t src;
  InfoDST_t dst;
  char payload[1]; /* of size maxsz */
};

struct nn_xmsg_chain_elem {
  struct nn_xmsg_chain_elem *older;
};

enum nn_xmsg_dstmode {
  NN_XMSG_DST_UNSET,
  NN_XMSG_DST_ONE,
  NN_XMSG_DST_ALL
};

struct nn_xmsg {
  struct nn_xmsgpool *pool;
  size_t maxsz;
  size_t sz;
  int have_params;
  struct ddsi_serdata *refd_payload;
  os_iovec_t refd_payload_iov;
  int64_t maxdelay;
#ifdef DDSI_INCLUDE_NETWORK_PARTITIONS
  uint32_t encoderid;
#endif

  /* Backref for late updating of available sequence numbers, and
     merging of retransmits. */
  enum nn_xmsg_kind kind;
  union {
    char control;
    struct {
      nn_guid_t wrguid;
      seqno_t wrseq;
      nn_fragment_number_t wrfragid;
      /* readerId encodes offset to destination readerId or 0 -- used
         only for rexmits, but more convenient to combine both into
         one struct in the union */
      unsigned readerId_off;
    } data;
  } kindspecific;

  enum nn_xmsg_dstmode dstmode;
  union {
    struct {
      nn_locator_t loc;  /* send just to this locator */
    } one;
    struct {
      struct addrset *as;       /* send to all addresses in set */
      struct addrset *as_group; /* send to one address in set */
    } all;
  } dstaddr;

  struct nn_xmsg_chain_elem link;
  struct nn_xmsg_data *data;
};

/* Worst-case: change of SRC [+1] but no DST, submessage [+1], ref'd
   payload [+1].  So 128 iovecs => at least ~40 submessages, so for
   very small ones still >1kB. */
#define NN_XMSG_MAX_SUBMESSAGE_IOVECS 3

#ifdef IOV_MAX
#if IOV_MAX > 0 && IOV_MAX < 256
#define NN_XMSG_MAX_MESSAGE_IOVECS IOV_MAX
#endif
#endif /* defined IOV_MAX */
#ifndef NN_XMSG_MAX_MESSAGE_IOVECS
#define NN_XMSG_MAX_MESSAGE_IOVECS 256
#endif

/* Used to keep them in order, but it now transpires that delayed
   updating of writer seq nos benefits from having them in the
   reverse order.  They are not being used for anything else, so
   we no longer maintain a pointer to both ends. */
struct nn_xmsg_chain {
  struct nn_xmsg_chain_elem *latest;
};

#ifdef DDSI_INCLUDE_BANDWIDTH_LIMITING
#define NN_BW_UNLIMITED (0)

struct nn_bw_limiter {
    uint32_t       bandwidth;   /*Config in bytes/s   (0 = UNLIMITED)*/
    int64_t        balance;
    nn_mtime_t      last_update;
};
#endif

///////////////////////////
typedef struct os_sem {
  os_mutex mtx;
  uint32_t value;
  os_cond cv;
} os_sem_t;

static os_result os_sem_init (os_sem_t * sem, uint32_t value)
{
  sem->value = value;
  os_mutexInit (&sem->mtx);
  os_condInit (&sem->cv, &sem->mtx);
  return os_resultSuccess;
}

static os_result os_sem_destroy (os_sem_t *sem)
{
  os_condDestroy (&sem->cv);
  os_mutexDestroy (&sem->mtx);
  return os_resultSuccess;
}

static os_result os_sem_post (os_sem_t *sem)
{
  os_mutexLock (&sem->mtx);
  if (sem->value++ == 0)
    os_condSignal (&sem->cv);
  os_mutexUnlock (&sem->mtx);
  return os_resultSuccess;
}

static os_result os_sem_wait (os_sem_t *sem)
{
  os_mutexLock (&sem->mtx);
  while (sem->value == 0)
    os_condWait (&sem->cv, &sem->mtx);
  os_mutexUnlock (&sem->mtx);
  return os_resultSuccess;
}
///////////////////////////

struct nn_xpack
{
  struct nn_xpack *sendq_next;
  bool async_mode;
  Header_t hdr;
  MsgLen_t msg_len;
  nn_guid_prefix_t *last_src;
  InfoDST_t *last_dst;
  int64_t maxdelay;
  unsigned packetid;
  os_atomic_uint32_t calls;
  uint32_t call_flags;
  ddsi_tran_conn_t conn;
  os_sem_t sem;
  size_t niov;
  os_iovec_t iov[NN_XMSG_MAX_MESSAGE_IOVECS];
  enum nn_xmsg_dstmode dstmode;

  union
  {
    nn_locator_t loc; /* send just to this locator */
    struct
    {
      struct addrset *as;        /* send to all addresses in set */
      struct addrset *as_group;  /* send to one address in set */
    } all;
  } dstaddr;

  struct nn_xmsg_chain included_msgs;

#ifdef DDSI_INCLUDE_BANDWIDTH_LIMITING
  struct nn_bw_limiter limiter;
#endif

#ifdef DDSI_INCLUDE_NETWORK_PARTITIONS
  uint32_t encoderId;
#endif /* DDSI_INCLUDE_NETWORK_PARTITIONS */

#ifdef DDSI_INCLUDE_ENCRYPTION
  /* each partion is associated with a SecurityPolicy, this codecset will serve */
  /* all of them, different cipher for each partition */
  q_securityEncoderSet codec;
  PT_InfoContainer_t SecurityHeader;
#endif /* DDSI_INCLUDE_ENCRYPTION */
};

static size_t align4u (size_t x)
{
  return (x + 3) & ~(size_t)3;
}

/* XMSGPOOL ------------------------------------------------------------

   Great expectations, but so far still wanting. */

static void nn_xmsg_realfree (struct nn_xmsg *m);

struct nn_xmsgpool *nn_xmsgpool_new (void)
{
  struct nn_xmsgpool *pool;
  pool = os_malloc (sizeof (*pool));
  nn_freelist_init (&pool->freelist, UINT32_MAX, offsetof (struct nn_xmsg, link.older));
  return pool;
}

static void nn_xmsg_realfree_wrap (void *elem)
{
  nn_xmsg_realfree (elem);
}

void nn_xmsgpool_free (struct nn_xmsgpool *pool)
{
  nn_freelist_fini (&pool->freelist, nn_xmsg_realfree_wrap);
  DDS_TRACE("xmsgpool_free(%p)\n", (void *)pool);
  os_free (pool);
}

/* XMSG ----------------------------------------------------------------

   All messages that are sent start out as xmsgs, which is a sequence
   of submessages potentially ending with a blob of serialized data.
   Such serialized data is given as a reference to part of a serdata.

   An xmsg can be queued for transmission, after which it must be
   forgotten by its creator.  The queue handler packs them into xpacks
   (see below), transmits them, and releases them.

   Currently, the message pool is fake, so 2 mallocs and frees are
   needed for each message, and additionally, it involves address set
   manipulations.  The latter is especially inefficiently dealt with
   in the xpack. */

static void nn_xmsg_reinit (struct nn_xmsg *m, enum nn_xmsg_kind kind)
{
  m->sz = 0;
  m->have_params = 0;
  m->refd_payload = NULL;
  m->dstmode = NN_XMSG_DST_UNSET;
  m->kind = kind;
  m->maxdelay = 0;
#ifdef DDSI_INCLUDE_NETWORK_PARTITIONS
  m->encoderid = 0;
#endif
  memset (&m->kindspecific, 0, sizeof (m->kindspecific));
}

static struct nn_xmsg *nn_xmsg_allocnew (struct nn_xmsgpool *pool, size_t expected_size, enum nn_xmsg_kind kind)
{
  struct nn_xmsg *m;
  struct nn_xmsg_data *d;

  if (expected_size == 0)
    expected_size = NN_XMSG_CHUNK_SIZE;

  if ((m = os_malloc (sizeof (*m))) == NULL)
    return NULL;

  m->pool = pool;
  m->maxsz = (expected_size + NN_XMSG_CHUNK_SIZE - 1) & (unsigned)-NN_XMSG_CHUNK_SIZE;

  if ((d = m->data = os_malloc (offsetof (struct nn_xmsg_data, payload) + m->maxsz)) == NULL)
  {
    os_free (m);
    return NULL;
  }
  d->src.smhdr.submessageId = SMID_INFO_SRC;
  d->src.smhdr.flags = (PLATFORM_IS_LITTLE_ENDIAN ? SMFLAG_ENDIANNESS : 0);
  d->src.smhdr.octetsToNextHeader = sizeof (d->src) - (offsetof (InfoSRC_t, smhdr.octetsToNextHeader) + 2);
  d->src.unused = 0;
  d->src.version.major = RTPS_MAJOR;
  d->src.version.minor = RTPS_MINOR;
  d->src.vendorid = NN_VENDORID_ECLIPSE;
  d->dst.smhdr.submessageId = SMID_INFO_DST;
  d->dst.smhdr.flags = (PLATFORM_IS_LITTLE_ENDIAN ? SMFLAG_ENDIANNESS : 0);
  d->dst.smhdr.octetsToNextHeader = sizeof (d->dst.guid_prefix);
  nn_xmsg_reinit (m, kind);
  return m;
}

struct nn_xmsg *nn_xmsg_new (struct nn_xmsgpool *pool, const nn_guid_prefix_t *src_guid_prefix, size_t expected_size, enum nn_xmsg_kind kind)
{
  struct nn_xmsg *m;
  if ((m = nn_freelist_pop (&pool->freelist)) != NULL)
    nn_xmsg_reinit (m, kind);
  else if ((m = nn_xmsg_allocnew (pool, expected_size, kind)) == NULL)
    return NULL;
  m->data->src.guid_prefix = nn_hton_guid_prefix (*src_guid_prefix);
  return m;
}

static void nn_xmsg_realfree (struct nn_xmsg *m)
{
  os_free (m->data);
  os_free (m);
}

void nn_xmsg_free (struct nn_xmsg *m)
{
  struct nn_xmsgpool *pool = m->pool;
  if (m->refd_payload)
    ddsi_serdata_to_ser_unref (m->refd_payload, &m->refd_payload_iov);
  if (m->dstmode == NN_XMSG_DST_ALL)
  {
    unref_addrset (m->dstaddr.all.as);
    unref_addrset (m->dstaddr.all.as_group);
  }
  if (!nn_freelist_push (&pool->freelist, m))
  {
    nn_xmsg_realfree (m);
  }
}

/************************************************/

#ifndef NDEBUG
static int submsg_is_compatible (const struct nn_xmsg *msg, SubmessageKind_t smkind)
{
  switch (msg->kind)
  {
    case NN_XMSG_KIND_CONTROL:
      switch (smkind)
      {
        case SMID_PAD:
          /* never use this one -- so let's crash when we do :) */
          return 0;
        case SMID_INFO_SRC: case SMID_INFO_REPLY_IP4:
        case SMID_INFO_DST: case SMID_INFO_REPLY:
          /* we never generate these directly */
          return 0;
        case SMID_INFO_TS:
        case SMID_ACKNACK: case SMID_HEARTBEAT:
        case SMID_GAP: case SMID_NACK_FRAG:
        case SMID_HEARTBEAT_FRAG:
        case SMID_PT_INFO_CONTAINER:
        case SMID_PT_MSG_LEN:
        case SMID_PT_ENTITY_ID:
          /* normal control stuff is ok */
          return 1;
        case SMID_DATA: case SMID_DATA_FRAG:
          /* but data is strictly verboten */
          return 0;
      }
      assert (0);
      break;
    case NN_XMSG_KIND_DATA:
    case NN_XMSG_KIND_DATA_REXMIT:
      switch (smkind)
      {
        case SMID_PAD:
          /* never use this one -- so let's crash when we do :) */
          return 0;
        case SMID_INFO_SRC: case SMID_INFO_REPLY_IP4:
        case SMID_INFO_DST: case SMID_INFO_REPLY:
          /* we never generate these directly */
          return 0;
        case SMID_INFO_TS: case SMID_DATA: case SMID_DATA_FRAG:
          /* Timestamp only preceding data; data may be present just
             once for rexmits.  The readerId offset can be used to
             ensure rexmits have only one data submessages -- the test
             won't work for initial transmits, but those currently
             don't allow a readerId */
          return msg->kindspecific.data.readerId_off == 0;
        case SMID_ACKNACK:
        case SMID_HEARTBEAT:
        case SMID_GAP:
        case SMID_NACK_FRAG:
        case SMID_HEARTBEAT_FRAG:
        case SMID_PT_INFO_CONTAINER:
        case SMID_PT_MSG_LEN:
        case SMID_PT_ENTITY_ID:
          /* anything else is strictly verboten */
          return 0;
      }
      assert (0);
      break;
  }
  assert (0);
  return 1;
}
#endif

int nn_xmsg_compare_fragid (const struct nn_xmsg *a, const struct nn_xmsg *b)
{
  int c;
  assert (a->kind == NN_XMSG_KIND_DATA_REXMIT);
  assert (b->kind == NN_XMSG_KIND_DATA_REXMIT);
  /* I think most likely discriminator is seq, then writer guid, then
     fragid, but we'll stick to the "expected" order for now: writer,
     seq, frag */
  if ((c = memcmp (&a->kindspecific.data.wrguid, &b->kindspecific.data.wrguid, sizeof (a->kindspecific.data.wrguid))) != 0)
    return c;
  else if (a->kindspecific.data.wrseq != b->kindspecific.data.wrseq)
    return (a->kindspecific.data.wrseq < b->kindspecific.data.wrseq) ? -1 : 1;
  else if (a->kindspecific.data.wrfragid != b->kindspecific.data.wrfragid)
    return (a->kindspecific.data.wrfragid < b->kindspecific.data.wrfragid) ? -1 : 1;
  else
    return 0;
}

size_t nn_xmsg_size (const struct nn_xmsg *m)
{
  return m->sz;
}

enum nn_xmsg_kind nn_xmsg_kind (const struct nn_xmsg *m)
{
  return m->kind;
}

void nn_xmsg_guid_seq_fragid (const struct nn_xmsg *m, nn_guid_t *wrguid, seqno_t *wrseq, nn_fragment_number_t *wrfragid)
{
  assert (m->kind != NN_XMSG_KIND_CONTROL);
  *wrguid = m->kindspecific.data.wrguid;
  *wrseq = m->kindspecific.data.wrseq;
  *wrfragid = m->kindspecific.data.wrfragid;
}

void *nn_xmsg_payload (size_t *sz, struct nn_xmsg *m)
{
  *sz = m->sz;
  return m->data->payload;
}

void nn_xmsg_payload_to_plistsample (struct ddsi_plist_sample *dst, nn_parameterid_t keyparam, const struct nn_xmsg *m)
{
  dst->blob = m->data->payload;
  dst->size = m->sz;
  dst->keyparam = keyparam;
}

void nn_xmsg_submsg_init (struct nn_xmsg *msg, struct nn_xmsg_marker marker, SubmessageKind_t smkind)
{
  SubmessageHeader_t *hdr = (SubmessageHeader_t *) (msg->data->payload + marker.offset);
  assert (submsg_is_compatible (msg, smkind));
  hdr->submessageId = (unsigned char)smkind;
  hdr->flags = PLATFORM_IS_LITTLE_ENDIAN ? SMFLAG_ENDIANNESS : 0;
  hdr->octetsToNextHeader = 0;
}

void nn_xmsg_submsg_setnext (struct nn_xmsg *msg, struct nn_xmsg_marker marker)
{
  SubmessageHeader_t *hdr = (SubmessageHeader_t *) (msg->data->payload + marker.offset);
  unsigned plsize = msg->refd_payload ? (unsigned) msg->refd_payload_iov.iov_len : 0;
  assert ((msg->sz % 4) == 0);
  assert ((plsize % 4) == 0);
  assert ((unsigned) (msg->data->payload + msg->sz + plsize - (char *) hdr) >= RTPS_SUBMESSAGE_HEADER_SIZE);
  hdr->octetsToNextHeader = (unsigned short)
    ((unsigned)(msg->data->payload + msg->sz + plsize - (char *) hdr) - RTPS_SUBMESSAGE_HEADER_SIZE);
}

void *nn_xmsg_submsg_from_marker (struct nn_xmsg *msg, struct nn_xmsg_marker marker)
{
  return msg->data->payload + marker.offset;
}

void * nn_xmsg_append (struct nn_xmsg *m, struct nn_xmsg_marker *marker, size_t sz)
{
  static const size_t a = 4;

  /* May realloc, in which case m may change.  But that will not
     happen if you do not exceed expected_size.  Max size is always a
     multiple of A: that means we don't have to worry about memory
     available just for alignment. */
  char *p;
  assert (1 <= a && a <= NN_XMSG_MAX_ALIGN);
  assert ((m->maxsz % a) == 0);
  if ((m->sz % a) != 0)
  {
    size_t npad = a - (m->sz % a);
    memset (m->data->payload + m->sz, 0, npad);
    m->sz += npad;
  }
  if (m->sz + sz > m->maxsz)
  {
    size_t nmax = (m->maxsz + sz + NN_XMSG_CHUNK_SIZE - 1) & (size_t)-NN_XMSG_CHUNK_SIZE;
    struct nn_xmsg_data *ndata = os_realloc (m->data, offsetof (struct nn_xmsg_data, payload) + nmax);
    m->maxsz = nmax;
    m->data = ndata;
  }
  p = m->data->payload + m->sz;
  if (marker)
    marker->offset = m->sz;
  m->sz += sz;
  return p;
}

void nn_xmsg_shrink (struct nn_xmsg *m, struct nn_xmsg_marker marker, size_t sz)
{
  assert (m != NULL);
  assert (marker.offset <= m->sz);
  assert (marker.offset + sz <= m->sz);
  m->sz = marker.offset + sz;
}

void nn_xmsg_add_timestamp (struct nn_xmsg *m, nn_wctime_t t)
{
  InfoTimestamp_t * ts;
  struct nn_xmsg_marker sm;

  ts = (InfoTimestamp_t*) nn_xmsg_append (m, &sm, sizeof (InfoTimestamp_t));
  nn_xmsg_submsg_init (m, sm, SMID_INFO_TS);
  ts->time = nn_wctime_to_ddsi_time (t);
  nn_xmsg_submsg_setnext (m, sm);
}

void nn_xmsg_add_entityid (struct nn_xmsg * m)
{
  EntityId_t * eid;
  struct nn_xmsg_marker sm;

  eid = (EntityId_t*) nn_xmsg_append (m, &sm, sizeof (EntityId_t));
  nn_xmsg_submsg_init (m, sm, SMID_PT_ENTITY_ID);
  eid->entityid.u = NN_ENTITYID_PARTICIPANT;
  nn_xmsg_submsg_setnext (m, sm);
}

void nn_xmsg_serdata (struct nn_xmsg *m, struct ddsi_serdata *serdata, size_t off, size_t len)
{
  if (serdata->kind != SDK_EMPTY)
  {
    size_t len4 = align4u (len);
    assert (m->refd_payload == NULL);
    m->refd_payload = ddsi_serdata_to_ser_ref (serdata, off, len4, &m->refd_payload_iov);
  }
}

void nn_xmsg_setdst1 (struct nn_xmsg *m, const nn_guid_prefix_t *gp, const nn_locator_t *loc)
{
  assert (m->dstmode == NN_XMSG_DST_UNSET);
  m->dstmode = NN_XMSG_DST_ONE;
  m->dstaddr.one.loc = *loc;
  m->data->dst.guid_prefix = nn_hton_guid_prefix (*gp);
}

int nn_xmsg_setdstPRD (struct nn_xmsg *m, const struct proxy_reader *prd)
{
  nn_locator_t loc;
  if (addrset_any_uc (prd->c.as, &loc) || addrset_any_mc (prd->c.as, &loc))
  {
    nn_xmsg_setdst1 (m, &prd->e.guid.prefix, &loc);
    return 0;
  }
  else
  {
    DDS_WARNING("nn_xmsg_setdstPRD: no address for %x:%x:%x:%x", PGUID (prd->e.guid));
    return ERR_NO_ADDRESS;
  }
}

int nn_xmsg_setdstPWR (struct nn_xmsg *m, const struct proxy_writer *pwr)
{
  nn_locator_t loc;
  if (addrset_any_uc (pwr->c.as, &loc) || addrset_any_mc (pwr->c.as, &loc))
  {
    nn_xmsg_setdst1 (m, &pwr->e.guid.prefix, &loc);
    return 0;
  }
  DDS_WARNING("nn_xmsg_setdstPRD: no address for %x:%x:%x:%x", PGUID (pwr->e.guid));
  return ERR_NO_ADDRESS;
}

void nn_xmsg_setdstN (struct nn_xmsg *m, struct addrset *as, struct addrset *as_group)
{
  assert (m->dstmode == NN_XMSG_DST_UNSET || m->dstmode == NN_XMSG_DST_ONE);
  m->dstmode = NN_XMSG_DST_ALL;
  m->dstaddr.all.as = ref_addrset (as);
  m->dstaddr.all.as_group = ref_addrset (as_group);
}

void nn_xmsg_set_data_readerId (struct nn_xmsg *m, nn_entityid_t *readerId)
{
  assert (m->kind == NN_XMSG_KIND_DATA_REXMIT);
  assert (m->kindspecific.data.readerId_off == 0);
  assert ((char *) readerId > m->data->payload);
  assert ((char *) readerId < m->data->payload + m->sz);
  m->kindspecific.data.readerId_off = (unsigned) ((char *) readerId - m->data->payload);
}

static void clear_readerId (struct nn_xmsg *m)
{
  assert (m->kind == NN_XMSG_KIND_DATA_REXMIT);
  assert (m->kindspecific.data.readerId_off != 0);
  *((nn_entityid_t *) (m->data->payload + m->kindspecific.data.readerId_off)) =
    nn_hton_entityid (to_entityid (NN_ENTITYID_UNKNOWN));
}

static nn_entityid_t load_readerId (const struct nn_xmsg *m)
{
  assert (m->kind == NN_XMSG_KIND_DATA_REXMIT);
  assert (m->kindspecific.data.readerId_off != 0);
  return nn_ntoh_entityid (*((nn_entityid_t *) (m->data->payload + m->kindspecific.data.readerId_off)));
}

static int readerId_compatible (const struct nn_xmsg *m, const struct nn_xmsg *madd)
{
  nn_entityid_t e = load_readerId (m);
  nn_entityid_t eadd = load_readerId (madd);
  return e.u == NN_ENTITYID_UNKNOWN || e.u == eadd.u;
}

int nn_xmsg_merge_rexmit_destinations_wrlock_held (struct nn_xmsg *m, const struct nn_xmsg *madd)
{
  assert (m->kindspecific.data.wrseq >= 1);
  assert (m->kindspecific.data.wrguid.prefix.u[0] != 0);
  assert (is_writer_entityid (m->kindspecific.data.wrguid.entityid));
  assert (memcmp (&m->kindspecific.data.wrguid, &madd->kindspecific.data.wrguid, sizeof (m->kindspecific.data.wrguid)) == 0);
  assert (m->kindspecific.data.wrseq == madd->kindspecific.data.wrseq);
  assert (m->kindspecific.data.wrfragid == madd->kindspecific.data.wrfragid);
  assert (m->kind == NN_XMSG_KIND_DATA_REXMIT);
  assert (madd->kind == NN_XMSG_KIND_DATA_REXMIT);
  assert (m->kindspecific.data.readerId_off != 0);
  assert (madd->kindspecific.data.readerId_off != 0);

  DDS_TRACE(" (%x:%x:%x:%x#%"PRId64"/%u:",
            PGUID (m->kindspecific.data.wrguid), m->kindspecific.data.wrseq, m->kindspecific.data.wrfragid + 1);

  switch (m->dstmode)
  {
    case NN_XMSG_DST_UNSET:
      assert (0);
      return 0;

    case NN_XMSG_DST_ALL:
      DDS_TRACE("*->*)");
      return 1;

    case NN_XMSG_DST_ONE:
      switch (madd->dstmode)
      {
        case NN_XMSG_DST_UNSET:
          assert (0);
          return 0;

        case NN_XMSG_DST_ALL:
          DDS_TRACE("1+*->*)");
          clear_readerId (m);
          m->dstmode = NN_XMSG_DST_ALL;
          m->dstaddr.all.as = ref_addrset (madd->dstaddr.all.as);
          m->dstaddr.all.as_group = ref_addrset (madd->dstaddr.all.as_group);
          return 1;

        case NN_XMSG_DST_ONE:
          if (memcmp (&m->data->dst.guid_prefix, &madd->data->dst.guid_prefix, sizeof (m->data->dst.guid_prefix)) != 0)
          {
            struct writer *wr;
            /* This is why wr->e.lock must be held: we can't safely
               reference the writer's address set if it isn't -- so
               FIXME: add a way to atomically replace the contents of
               an addrset in rebuild_writer_addrset: then we don't
               need the lock anymore, and the '_wrlock_held' suffix
               can go and everyone's life will become easier! */
            if ((wr = ephash_lookup_writer_guid (&m->kindspecific.data.wrguid)) == NULL)
            {
              DDS_TRACE("writer-dead)");
              return 0;
            }
            else
            {
              DDS_TRACE("1+1->*)");
              clear_readerId (m);
              m->dstmode = NN_XMSG_DST_ALL;
              m->dstaddr.all.as = ref_addrset (wr->as);
              m->dstaddr.all.as_group = ref_addrset (wr->as_group);
              return 1;
            }
          }
          else if (readerId_compatible (m, madd))
          {
            DDS_TRACE("1+1->1)");
            return 1;
          }
          else
          {
            DDS_TRACE("1+1->2)");
            clear_readerId (m);
            return 1;
          }
      }
      break;
  }
  assert (0);
  return 0;
}

int nn_xmsg_setmaxdelay (struct nn_xmsg *msg, int64_t maxdelay)
{
  assert (msg->maxdelay == 0);
  msg->maxdelay = maxdelay;
  return 0;
}

#ifdef DDSI_INCLUDE_NETWORK_PARTITIONS
int nn_xmsg_setencoderid (struct nn_xmsg *msg, uint32_t encoderid)
{
  assert (msg->encoderid == 0);
  msg->encoderid = encoderid;
  return 0;
}
#endif

void nn_xmsg_setwriterseq (struct nn_xmsg *msg, const nn_guid_t *wrguid, seqno_t wrseq)
{
  msg->kindspecific.data.wrguid = *wrguid;
  msg->kindspecific.data.wrseq = wrseq;
}

void nn_xmsg_setwriterseq_fragid (struct nn_xmsg *msg, const nn_guid_t *wrguid, seqno_t wrseq, nn_fragment_number_t wrfragid)
{
  nn_xmsg_setwriterseq (msg, wrguid, wrseq);
  msg->kindspecific.data.wrfragid = wrfragid;
}

size_t nn_xmsg_add_string_padded(_Inout_opt_ unsigned char *buf, _In_ char *str)
{
  size_t len = strlen (str) + 1;
  assert (len <= UINT32_MAX);
  if (buf) {
    /* Add cdr string */
    struct cdrstring *p = (struct cdrstring *) buf;
    p->length = (uint32_t)len;
    memcpy (p->contents, str, len);
    /* clear padding */
    if (len < align4u (len)) {
      memset (p->contents + len, 0, align4u (len) - len);
    }
  }
  len = 4 +           /* cdr string len arg + */
        align4u(len); /* strlen + possible padding */
  return len;
}

size_t nn_xmsg_add_octseq_padded(_Inout_opt_ unsigned char *buf, _In_ nn_octetseq_t *seq)
{
  unsigned len = seq->length;
  if (buf) {
    /* Add cdr octet seq */
    *((unsigned *)buf) = len;
    buf += sizeof (int);
    memcpy (buf, seq->value, len);
    /* clear padding */
    if (len < align4u (len)) {
      memset (buf + len, 0, align4u (len) - len);
    }
  }
  return 4 +           /* cdr sequence len arg + */
         align4u(len); /* seqlen + possible padding */
}


size_t nn_xmsg_add_dataholder_padded (_Inout_opt_ unsigned char *buf, const struct nn_dataholder *dh)
{
  unsigned i;
  size_t len;
  unsigned dummy = 0;
  unsigned *cnt = &dummy;

  len = nn_xmsg_add_string_padded(buf, dh->class_id);

  if (buf) {
    cnt = ((unsigned *)&(buf[len]));
    *cnt = 0;
  }
  len += sizeof(int);
  for (i = 0; i < dh->properties.n; i++) {
    nn_property_t *p = &(dh->properties.props[i]);
    if (p->propagate) {
      len += nn_xmsg_add_string_padded(buf ? &(buf[len]) : NULL, p->name);
      len += nn_xmsg_add_string_padded(buf ? &(buf[len]) : NULL, p->value);
      (*cnt)++;
    }
    /* p->propagate is not propagated over the wire. */
  }

  if (buf) {
    cnt = ((unsigned *)&(buf[len]));
    *cnt = 0;
  }
  len += sizeof(int);
  for (i = 0; i < dh->binary_properties.n; i++) {
    nn_binaryproperty_t *p = &(dh->binary_properties.props[i]);
    if (p->propagate) {
      len += nn_xmsg_add_string_padded(buf ? &(buf[len]) : NULL,   p->name  );
      len += nn_xmsg_add_octseq_padded(buf ? &(buf[len]) : NULL, &(p->value));
      (*cnt)++;
    }
    /* p->propagate is not propagated over the wire. */
  }

  return len;
}


void * nn_xmsg_addpar (struct nn_xmsg *m, unsigned pid, size_t len)
{
  const size_t len4 = (len + 3) & (size_t)-4; /* must alloc a multiple of 4 */
  nn_parameter_t *phdr;
  char *p;
  m->have_params = 1;
  phdr = nn_xmsg_append (m, NULL, sizeof (nn_parameter_t) + len4);
  phdr->parameterid = (nn_parameterid_t) pid;
  phdr->length = (unsigned short) len4;
  p = (char *) (phdr + 1);
  if (len4 > len)
  {
    /* zero out padding bytes added to satisfy parameter alignment --
       alternative: zero out, but this way valgrind/purify can tell us
       where we forgot to initialize something */
    memset (p + len, 0, len4 - len);
  }
  return p;
}

void nn_xmsg_addpar_string (struct nn_xmsg *m, unsigned pid, const char *str)
{
  struct cdrstring *p;
  unsigned len = (unsigned) strlen (str) + 1;
  p = nn_xmsg_addpar (m, pid, 4 + len);
  p->length = len;
  memcpy (p->contents, str, len);
}

void nn_xmsg_addpar_octetseq (struct nn_xmsg *m, unsigned pid, const nn_octetseq_t *oseq)
{
  char *p = nn_xmsg_addpar (m, pid, 4 + oseq->length);
  *((unsigned *) p) = oseq->length;
  memcpy (p + sizeof (int), oseq->value, oseq->length);
}

void nn_xmsg_addpar_stringseq (struct nn_xmsg *m, unsigned pid, const nn_stringseq_t *sseq)
{
  unsigned char *tmp;
  uint32_t i;
  size_t len = 0;

  for (i = 0; i < sseq->n; i++)
  {
    len += nn_xmsg_add_string_padded(NULL, sseq->strs[i]);
  }

  tmp = nn_xmsg_addpar (m, pid, 4 + len);

  *((uint32_t *) tmp) = sseq->n;
  tmp += sizeof (uint32_t);
  for (i = 0; i < sseq->n; i++)
  {
    tmp += nn_xmsg_add_string_padded(tmp, sseq->strs[i]);
  }
}

void nn_xmsg_addpar_keyhash (struct nn_xmsg *m, const struct ddsi_serdata *serdata)
{
  if (serdata->kind != SDK_EMPTY)
  {
    const struct ddsi_serdata_default *serdata_def = (const struct ddsi_serdata_default *)serdata;
    char *p = nn_xmsg_addpar (m, PID_KEYHASH, 16);
    memcpy (p, serdata_def->keyhash.m_hash, 16);
  }
}

void nn_xmsg_addpar_guid (struct nn_xmsg *m, unsigned pid, const nn_guid_t *guid)
{
  unsigned *pu;
  int i;
  pu = nn_xmsg_addpar (m, pid, 16);
  for (i = 0; i < 3; i++)
  {
    pu[i] = toBE4u (guid->prefix.u[i]);
  }
  pu[i] = toBE4u (guid->entityid.u);
}

void nn_xmsg_addpar_reliability (struct nn_xmsg *m, unsigned pid, const struct nn_reliability_qospolicy *rq)
{
  struct nn_external_reliability_qospolicy *p;
  p = nn_xmsg_addpar (m, pid, sizeof (*p));
  if (NN_PEDANTIC_P)
  {
    switch (rq->kind)
    {
      case NN_BEST_EFFORT_RELIABILITY_QOS:
        p->kind = NN_PEDANTIC_BEST_EFFORT_RELIABILITY_QOS;
        break;
      case NN_RELIABLE_RELIABILITY_QOS:
        p->kind = NN_PEDANTIC_RELIABLE_RELIABILITY_QOS;
        break;
      default:
        assert (0);
    }
  }
  else
  {
    switch (rq->kind)
    {
      case NN_BEST_EFFORT_RELIABILITY_QOS:
        p->kind = NN_INTEROP_BEST_EFFORT_RELIABILITY_QOS;
        break;
      case NN_RELIABLE_RELIABILITY_QOS:
        p->kind = NN_INTEROP_RELIABLE_RELIABILITY_QOS;
        break;
      default:
        assert (0);
    }
  }
  p->max_blocking_time = rq->max_blocking_time;
}

void nn_xmsg_addpar_4u (struct nn_xmsg *m, unsigned pid, unsigned x)
{
  unsigned *p = nn_xmsg_addpar (m, pid, 4);
  *p = x;
}

void nn_xmsg_addpar_BE4u (struct nn_xmsg *m, unsigned pid, unsigned x)
{
  unsigned *p = nn_xmsg_addpar (m, pid, 4);
  *p = toBE4u (x);
}

void nn_xmsg_addpar_statusinfo (struct nn_xmsg *m, unsigned statusinfo)
{
  if ((statusinfo & ~NN_STATUSINFO_STANDARDIZED) == 0)
    nn_xmsg_addpar_BE4u (m, PID_STATUSINFO, statusinfo);
  else
  {
    unsigned *p = nn_xmsg_addpar (m, PID_STATUSINFO, 8);
    unsigned statusinfox = 0;
    assert ((statusinfo & ~NN_STATUSINFO_STANDARDIZED) == NN_STATUSINFO_OSPL_AUTO);
    if (statusinfo & NN_STATUSINFO_OSPL_AUTO)
      statusinfox |= NN_STATUSINFOX_OSPL_AUTO;
    p[0] = toBE4u (statusinfo & NN_STATUSINFO_STANDARDIZED);
    p[1] = toBE4u (statusinfox);
  }
}


void nn_xmsg_addpar_share (struct nn_xmsg *m, unsigned pid, const struct nn_share_qospolicy *q)
{
  /* Written thus to allow q->name to be a null pointer if enable = false */
  const unsigned fixed_len = 4 + 4;
  const unsigned len = (q->enable ? (unsigned) strlen (q->name) : 0) + 1;
  unsigned char *p;
  struct cdrstring *ps;
  p = nn_xmsg_addpar (m, pid, fixed_len + len);
  p[0] = q->enable;
  p[1] = 0;
  p[2] = 0;
  p[3] = 0;
  ps = (struct cdrstring *) (p + 4);
  ps->length = len;
  if (q->enable)
    memcpy (ps->contents, q->name, len);
  else
    ps->contents[0] = 0;
}

void nn_xmsg_addpar_subscription_keys (struct nn_xmsg *m, unsigned pid, const struct nn_subscription_keys_qospolicy *q)
{
  unsigned char *tmp;
  size_t len = 8; /* use_key_list, length of key_list */
  unsigned i;

  for (i = 0; i < q->key_list.n; i++)
  {
    size_t len1 = strlen (q->key_list.strs[i]) + 1;
    len += 4 + align4u (len1);
  }

  tmp = nn_xmsg_addpar (m, pid, len);

  tmp[0] = q->use_key_list;
  for (i = 1; i < sizeof (int); i++)
  {
      tmp[i] = 0;
  }
  tmp += sizeof (int);
  *((uint32_t *) tmp) = q->key_list.n;
  tmp += sizeof (uint32_t);
  for (i = 0; i < q->key_list.n; i++)
  {
    struct cdrstring *p = (struct cdrstring *) tmp;
    size_t len1 = strlen (q->key_list.strs[i]) + 1;
    assert (len1 <= UINT32_MAX);
    p->length = (uint32_t)len1;
    memcpy (p->contents, q->key_list.strs[i], len1);
    if (len1 < align4u (len1))
      memset (p->contents + len1, 0, align4u (len1) - len1);
    tmp += 4 + align4u (len1);
  }
}

void nn_xmsg_addpar_sentinel (struct nn_xmsg * m)
{
  nn_xmsg_addpar (m, PID_SENTINEL, 0);
}

int nn_xmsg_addpar_sentinel_ifparam (struct nn_xmsg * m)
{
  if (m->have_params)
  {
    nn_xmsg_addpar_sentinel (m);
    return 1;
  }
  return 0;
}

void nn_xmsg_addpar_parvinfo (struct nn_xmsg *m, unsigned pid, const struct nn_prismtech_participant_version_info *pvi)
{
  int i;
  unsigned slen;
  unsigned *pu;
  struct cdrstring *ps;

  /* pvi->internals cannot be NULL here */
  slen = (unsigned) strlen(pvi->internals) + 1; /* +1 for '\0' terminator */
  pu = nn_xmsg_addpar (m, pid, NN_PRISMTECH_PARTICIPANT_VERSION_INFO_FIXED_CDRSIZE + slen);
  pu[0] = pvi->version;
  pu[1] = pvi->flags;
  for (i = 0; i < 3; i++)
  {
    pu[i+2] = (pvi->unused[i]);
  }
  ps = (struct cdrstring *)&pu[5];
  ps->length = slen;
  memcpy(ps->contents, pvi->internals, slen);
}

void nn_xmsg_addpar_eotinfo (struct nn_xmsg *m, unsigned pid, const struct nn_prismtech_eotinfo *txnid)
{
  uint32_t *pu, i;
  pu = nn_xmsg_addpar (m, pid, 2 * sizeof (uint32_t) + txnid->n * sizeof (txnid->tids[0]));
  pu[0] = txnid->transactionId;
  pu[1] = txnid->n;
  for (i = 0; i < txnid->n; i++)
  {
    pu[2*i + 2] = toBE4u (txnid->tids[i].writer_entityid.u);
    pu[2*i + 3] = txnid->tids[i].transactionId;
  }
}

void nn_xmsg_addpar_dataholder (_In_ struct nn_xmsg *m, _In_ unsigned pid, _In_ const struct nn_dataholder *dh)
{
    unsigned char *tmp;
    size_t len;

    /* Get total payload length. */
    len = nn_xmsg_add_dataholder_padded(NULL, dh);

    /* Prepare parameter header and get payload pointer. */
    tmp = nn_xmsg_addpar (m, pid, 4 + len);

    /* Insert dataholder. */
    nn_xmsg_add_dataholder_padded(tmp, dh);
}

/* XMSG_CHAIN ----------------------------------------------------------

   Xpacks refer to xmsgs and need to release these after having been
   sent.  For that purpose, we have a chain of xmsgs in an xpack.

   Chain elements are embedded in the xmsg, so instead of loading a
   pointer we compute the address of the xmsg from the address of the
   chain element, &c. */

static void nn_xmsg_chain_release (struct nn_xmsg_chain *chain)
{
  nn_guid_t wrguid;
  memset (&wrguid, 0, sizeof (wrguid));

  while (chain->latest)
  {
    struct nn_xmsg_chain_elem *ce = chain->latest;
    struct nn_xmsg *m = (struct nn_xmsg *) ((char *) ce - offsetof (struct nn_xmsg, link));
    chain->latest = ce->older;

    /* If this xmsg was written by a writer different from wrguid,
       update wr->xmit_seq.  There isn't necessarily a writer, and
       for fragmented data, only the last one must be updated, which
       we do by not setting the writer+seq for those xmsgs.

       These are all local writers, and are guaranteed to have the
       same, non-zero, systemId <=> wrguid.u[0].

       They are in reverse order, so we only attempt an update if this
       xmsg was produced by a writer different from the last one we
       processed. */
    if (m->kind == NN_XMSG_KIND_DATA && m->kindspecific.data.wrguid.prefix.u[0])
    {
      if (wrguid.prefix.u[1] != m->kindspecific.data.wrguid.prefix.u[1] ||
          wrguid.prefix.u[2] != m->kindspecific.data.wrguid.prefix.u[2] ||
          wrguid.entityid.u != m->kindspecific.data.wrguid.entityid.u)
      {
        struct writer *wr;
        assert (m->kindspecific.data.wrseq != 0);
        wrguid = m->kindspecific.data.wrguid;
        if ((wr = ephash_lookup_writer_guid (&m->kindspecific.data.wrguid)) != NULL)
          UPDATE_SEQ_XMIT_UNLOCKED(wr, m->kindspecific.data.wrseq);
      }
    }

    nn_xmsg_free (m);
  }
}

static void nn_xmsg_chain_add (struct nn_xmsg_chain *chain, struct nn_xmsg *m)
{
  m->link.older = chain->latest;
  chain->latest = &m->link;
}

#ifdef DDSI_INCLUDE_BANDWIDTH_LIMITING
/* BW_LIMITER ----------------------------------------------------------

   Helper for XPACKS, that contain the configuration and state to handle Bandwidth limitation.*/

/* To be called after Xpack sends out a packet.
 * Keeps a balance of real data vs. allowed data according to the bandwidth limit.
 * If data is send too fast, a sleep is inserted to get the used bandwidth at the configured rate.
 */

#define NN_BW_LIMIT_MAX_BUFFER (-30 * T_MILLISECOND)
#define NN_BW_LIMIT_MIN_SLEEP (2 * T_MILLISECOND)
static void nn_bw_limit_sleep_if_needed(struct nn_bw_limiter* this, ssize_t size)
{
  if ( this->bandwidth > 0 ) {
    nn_mtime_t tnow = now_mt();
    int64_t actual_interval;
    int64_t target_interval;

    /* calculate intervals */
    actual_interval = tnow.v - this->last_update.v;
    this->last_update = tnow;

    target_interval = T_SECOND*size/this->bandwidth;

    this->balance += (target_interval - actual_interval);


    DDS_TRACE(" <limiter(us):%"PRId64"",(target_interval - actual_interval)/1000);

    if ( this->balance < NN_BW_LIMIT_MAX_BUFFER )
    {
      /* We're below the bandwidth limit, do not further accumulate  */
      this->balance = NN_BW_LIMIT_MAX_BUFFER;
      DDS_TRACE(":%"PRId64":max",this->balance/1000);
    }
    else if ( this->balance > NN_BW_LIMIT_MIN_SLEEP )
    {
      /* We're over the bandwidth limit far enough, to warrent a sleep. */
      os_time delay;
      DDS_TRACE(":%"PRId64":sleep",this->balance/1000);
      delay.tv_sec = (int32_t) (this->balance / T_SECOND);
      delay.tv_nsec = (int32_t) (this->balance % T_SECOND);
      thread_state_blocked (lookup_thread_state ());
      os_nanoSleep (delay);
      thread_state_unblocked (lookup_thread_state ());
    }
    else
    {
      DDS_TRACE(":%"PRId64"",this->balance/1000);
    }
    DDS_TRACE(">");
  }
}


static void nn_bw_limit_init (struct nn_bw_limiter *limiter, uint32_t bandwidth_limit)
{
  limiter->bandwidth = bandwidth_limit;
  limiter->balance = 0;
  if (bandwidth_limit)
    limiter->last_update = now_mt ();
  else
    limiter->last_update.v = 0;
}
#endif /* DDSI_INCLUDE_BANDWIDTH_LIMITING */

/* XPACK ---------------------------------------------------------------

   Queued messages are packed into xpacks (all by-ref, using iovecs).
   The xpack is sent to the union of all address sets provided in the
   message added to the xpack.  */

static void nn_xpack_reinit (struct nn_xpack *xp)
{
  xp->dstmode = NN_XMSG_DST_UNSET;
  xp->niov = 0;
  xp->call_flags = 0;
  xp->msg_len.length = 0;
  xp->included_msgs.latest = NULL;
  xp->maxdelay = T_NEVER;
#ifdef DDSI_INCLUDE_NETWORK_PARTITIONS
  xp->encoderId = 0;
#endif
  xp->packetid++;
}

struct nn_xpack * nn_xpack_new (ddsi_tran_conn_t conn, uint32_t bw_limit, bool async_mode)
{
  struct nn_xpack *xp;

  /* Disallow setting async_mode if not configured to enable async mode: this way we
     can avoid starting the async send thread altogether */
  assert (!async_mode || config.xpack_send_async);

  xp = os_malloc (sizeof (*xp));
  memset (xp, 0, sizeof (*xp));
  xp->async_mode = async_mode;

  /* Fixed header fields, initialized just once */
  xp->hdr.protocol.id[0] = 'R';
  xp->hdr.protocol.id[1] = 'T';
  xp->hdr.protocol.id[2] = 'P';
  xp->hdr.protocol.id[3] = 'S';
  xp->hdr.version.major = RTPS_MAJOR;
  xp->hdr.version.minor = RTPS_MINOR;
  xp->hdr.vendorid = NN_VENDORID_ECLIPSE;

  /* MSG_LEN first sub message for stream based connections */

  xp->msg_len.smhdr.submessageId = SMID_PT_MSG_LEN;
  xp->msg_len.smhdr.flags = (PLATFORM_IS_LITTLE_ENDIAN ? SMFLAG_ENDIANNESS : 0);
  xp->msg_len.smhdr.octetsToNextHeader = 4;

  xp->conn = conn;
  nn_xpack_reinit (xp);

  if (gv.thread_pool)
    os_sem_init (&xp->sem, 0);

#ifdef DDSI_INCLUDE_ENCRYPTION
  if (q_security_plugin.new_encoder)
  {
    xp->codec = (q_security_plugin.new_encoder) ();
    xp->SecurityHeader.smhdr.submessageId = SMID_PT_INFO_CONTAINER;
    xp->SecurityHeader.smhdr.flags = PLATFORM_IS_LITTLE_ENDIAN ? SMFLAG_ENDIANNESS : 0;;
    xp->SecurityHeader.smhdr.octetsToNextHeader = 4;
    xp->SecurityHeader.id = PTINFO_ID_ENCRYPT;
  }
#endif
#ifdef DDSI_INCLUDE_BANDWIDTH_LIMITING
  nn_bw_limit_init (&xp->limiter, bw_limit);
#else
  (void) bw_limit;
#endif
  return xp;
}

void nn_xpack_free (struct nn_xpack *xp)
{
  assert (xp->niov == 0);
  assert (xp->included_msgs.latest == NULL);
#ifdef DDSI_INCLUDE_ENCRYPTION
  if (q_security_plugin.free_encoder)
  {
    (q_security_plugin.free_encoder) (xp->codec);
  }
#endif
  if (gv.thread_pool)
    os_sem_destroy (&xp->sem);
  os_free (xp);
}

static ssize_t nn_xpack_send1 (const nn_locator_t *loc, void * varg)
{
  struct nn_xpack * xp = varg;
  ssize_t nbytes = 0;

  if (dds_get_log_mask() & DDS_LC_TRACE)
  {
    char buf[DDSI_LOCSTRLEN];
    DDS_TRACE(" %s", ddsi_locator_to_string (buf, sizeof(buf), loc));
  }

  if (config.xmit_lossiness > 0)
  {
    /* We drop APPROXIMATELY a fraction of xmit_lossiness * 10**(-3)
       of all packets to be sent */
    if ((os_random () % 1000) < config.xmit_lossiness)
    {
      DDS_TRACE("(dropped)");
      xp->call_flags = 0;
      return 0;
    }
  }

#ifdef DDSI_INCLUDE_ENCRYPTION
  if (q_security_plugin.send_encoded && xp->encoderId != 0 && (q_security_plugin.encoder_type) (xp->codec, xp->encoderId) != Q_CIPHER_NONE)
  {
    struct iovec iov[NN_XMSG_MAX_MESSAGE_IOVECS];
    memcpy (iov, xp->iov, sizeof (iov));
    nbytes = (q_security_plugin.send_encoded) (xp->conn, loc, xp->niov, iov, &xp->codec, xp->encoderId, xp->call_flags);
  }
  else
#endif
  {
    if (!gv.mute)
    {
      nbytes = ddsi_conn_write (xp->conn, loc, xp->niov, xp->iov, xp->call_flags);
#ifndef NDEBUG
      {
        size_t i, len;
        for (i = 0, len = 0; i < xp->niov; i++) {
          len += xp->iov[i].iov_len;
        }
        assert (nbytes == -1 || (size_t) nbytes == len);
      }
#endif
    }
    else
    {
      DDS_TRACE("(dropped)");
      nbytes = (ssize_t) xp->msg_len.length;
    }
  }

  /* Clear call flags, as used on a per call basis */

  xp->call_flags = 0;

#ifdef DDSI_INCLUDE_BANDWIDTH_LIMITING
  if (nbytes > 0)
  {
    nn_bw_limit_sleep_if_needed (&xp->limiter, nbytes);
  }
#endif

  return nbytes;
}

static void nn_xpack_send1v (const nn_locator_t *loc, void * varg)
{
  (void) nn_xpack_send1 (loc, varg);
}

typedef struct nn_xpack_send1_thread_arg {
  const nn_locator_t *loc;
  struct nn_xpack *xp;
} *nn_xpack_send1_thread_arg_t;

static void nn_xpack_send1_thread (void * varg)
{
  nn_xpack_send1_thread_arg_t arg = varg;
  (void) nn_xpack_send1 (arg->loc, arg->xp);
  if (os_atomic_dec32_ov (&arg->xp->calls) == 1)
  {
    os_sem_post (&arg->xp->sem);
  }
  os_free (varg);
}

static void nn_xpack_send1_threaded (const nn_locator_t *loc, void * varg)
{
  nn_xpack_send1_thread_arg_t arg = os_malloc (sizeof (*arg));
  arg->xp = (struct nn_xpack *) varg;
  arg->loc = loc;
  os_atomic_inc32 (&arg->xp->calls);
  ut_thread_pool_submit (gv.thread_pool, nn_xpack_send1_thread, arg);
}

static void nn_xpack_send_real (struct nn_xpack * xp)
{
  size_t calls;

  assert (xp->niov <= NN_XMSG_MAX_MESSAGE_IOVECS);

  if (xp->niov == 0)
  {
    return;
  }

  assert (xp->dstmode != NN_XMSG_DST_UNSET);

  if (dds_get_log_mask() & DDS_LC_TRACE)
  {
    int i;
    DDS_TRACE("nn_xpack_send %u:", xp->msg_len.length);
    for (i = 0; i < (int) xp->niov; i++)
    {
      DDS_TRACE(" %p:%lu", (void *) xp->iov[i].iov_base, (unsigned long) xp->iov[i].iov_len);
    }
  }

  DDS_TRACE(" [");
  if (xp->dstmode == NN_XMSG_DST_ONE)
  {
    calls = 1;
    (void) nn_xpack_send1 (&xp->dstaddr.loc, xp);
  }
  else
  {
    /* Send to all addresses in as - as ultimately references the writer's
       address set, which is currently replaced rather than changed whenever
       it is updated, but that might not be something we want to guarantee */
    calls = 0;
    if (xp->dstaddr.all.as)
    {
      if (gv.thread_pool == NULL)
      {
        calls = addrset_forall_count (xp->dstaddr.all.as, nn_xpack_send1v, xp);
      }
      else
      {
        os_atomic_st32 (&xp->calls, 1);
        calls = addrset_forall_count (xp->dstaddr.all.as, nn_xpack_send1_threaded, xp);
        /* Wait for the thread pool to complete the write; if we're the one
           decrementing "calls" to 0, all of the work has been completed and
           none of the threads will be posting; else some thread will be
           posting it and we had better wait for it */
        if (os_atomic_dec32_ov (&xp->calls) != 1)
          os_sem_wait (&xp->sem);
      }
      unref_addrset (xp->dstaddr.all.as);
    }

    /* Send to at most one address in as_group */

    if (xp->dstaddr.all.as_group)
    {
      if (addrset_forone (xp->dstaddr.all.as_group, nn_xpack_send1, xp) == 0)
      {
        calls++;
      }
      unref_addrset (xp->dstaddr.all.as_group);
    }
  }
  DDS_TRACE(" ]\n");
  if (calls)
  {
    DDS_LOG(DDS_LC_TRAFFIC, "traffic-xmit (%lu) %u\n", (unsigned long) calls, xp->msg_len.length);
  }
  nn_xmsg_chain_release (&xp->included_msgs);
  nn_xpack_reinit (xp);
}

#define SENDQ_MAX 200
#define SENDQ_HW 10
#define SENDQ_LW 0

static uint32_t nn_xpack_sendq_thread (UNUSED_ARG (void *arg))
{
  os_mutexLock (&gv.sendq_lock);
  while (!(gv.sendq_stop && gv.sendq_head == NULL))
  {
    struct nn_xpack *xp;
    if ((xp = gv.sendq_head) == NULL)
    {
      os_time to = { 0, 1000000 };
      os_condTimedWait (&gv.sendq_cond, &gv.sendq_lock, &to);
    }
    else
    {
      gv.sendq_head = xp->sendq_next;
      if (--gv.sendq_length == SENDQ_LW)
        os_condBroadcast (&gv.sendq_cond);
      os_mutexUnlock (&gv.sendq_lock);
      nn_xpack_send_real (xp);
      nn_xpack_free (xp);
      os_mutexLock (&gv.sendq_lock);
    }
  }
  os_mutexUnlock (&gv.sendq_lock);
  return 0;
}

void nn_xpack_sendq_init (void)
{
  gv.sendq_stop = 0;
  gv.sendq_head = NULL;
  gv.sendq_tail = NULL;
  gv.sendq_length = 0;
  os_mutexInit (&gv.sendq_lock);
  os_condInit (&gv.sendq_cond, &gv.sendq_lock);
}

void nn_xpack_sendq_start (void)
{
  gv.sendq_ts = create_thread("sendq", nn_xpack_sendq_thread, NULL);
}

void nn_xpack_sendq_stop (void)
{
  os_mutexLock (&gv.sendq_lock);
  gv.sendq_stop = 1;
  os_condBroadcast (&gv.sendq_cond);
  os_mutexUnlock (&gv.sendq_lock);
}

void nn_xpack_sendq_fini (void)
{
  assert (gv.sendq_head == NULL);
  join_thread(gv.sendq_ts);
  os_condDestroy(&gv.sendq_cond);
  os_mutexDestroy(&gv.sendq_lock);
}

void nn_xpack_send (struct nn_xpack *xp, bool immediately)
{
  if (!xp->async_mode)
  {
    nn_xpack_send_real (xp);
  }
  else
  {
    struct nn_xpack *xp1 = os_malloc (sizeof (*xp));
    memcpy (xp1, xp, sizeof (*xp1));
    nn_xpack_reinit (xp);
    xp1->sendq_next = NULL;
    os_mutexLock (&gv.sendq_lock);
    if (immediately || gv.sendq_length == SENDQ_HW)
      os_condBroadcast (&gv.sendq_cond);
    if (gv.sendq_length >= SENDQ_MAX)
    {
      while (gv.sendq_length > SENDQ_LW)
        os_condWait (&gv.sendq_cond, &gv.sendq_lock);
    }
    if (gv.sendq_head)
      gv.sendq_tail->sendq_next = xp1;
    else
    {
      gv.sendq_head = xp1;
    }
    gv.sendq_tail = xp1;
    gv.sendq_length++;
    os_mutexUnlock (&gv.sendq_lock);
  }
}

static void copy_addressing_info (struct nn_xpack *xp, const struct nn_xmsg *m)
{
  xp->dstmode = m->dstmode;
  switch (m->dstmode)
  {
    case NN_XMSG_DST_UNSET:
      assert (0);
      break;
    case NN_XMSG_DST_ONE:
      xp->dstaddr.loc = m->dstaddr.one.loc;
      break;
    case NN_XMSG_DST_ALL:
      xp->dstaddr.all.as = ref_addrset (m->dstaddr.all.as);
      xp->dstaddr.all.as_group = ref_addrset (m->dstaddr.all.as_group);
      break;
  }
}

static int addressing_info_eq_onesidederr (const struct nn_xpack *xp, const struct nn_xmsg *m)
{
  if (xp->dstmode != m->dstmode)
    return 0;
  switch (xp->dstmode)
  {
    case NN_XMSG_DST_UNSET:
      assert (0);
    case NN_XMSG_DST_ONE:
      return (memcmp (&xp->dstaddr.loc, &m->dstaddr.one.loc, sizeof (xp->dstaddr.loc)) == 0);
    case NN_XMSG_DST_ALL:
      return (addrset_eq_onesidederr (xp->dstaddr.all.as, m->dstaddr.all.as) &&
              addrset_eq_onesidederr (xp->dstaddr.all.as_group, m->dstaddr.all.as_group));
  }
  assert (0);
  return 0;
}

static int nn_xpack_mayaddmsg (const struct nn_xpack *xp, const struct nn_xmsg *m, const uint32_t flags)
{
  unsigned max_msg_size = config.max_msg_size;
  unsigned payload_size;

  if (xp->niov == 0)
    return 1;
  assert (xp->included_msgs.latest != NULL);
  if (xp->niov + NN_XMSG_MAX_SUBMESSAGE_IOVECS > NN_XMSG_MAX_MESSAGE_IOVECS)
    return 0;

  payload_size = m->refd_payload ? (unsigned) m->refd_payload_iov.iov_len : 0;

#ifdef DDSI_INCLUDE_ENCRYPTION
  if (xp->encoderId)
  {
    unsigned security_header;
    security_header = (q_security_plugin.header_size) (xp->codec, xp->encoderId);
    assert (security_header < max_msg_size);
    max_msg_size -= security_header;
  }
#endif

  /* Check if max message size exceeded */

  if (xp->msg_len.length + m->sz + payload_size > max_msg_size)
  {
    return 0;
  }

  /* Check if different call semantics */

  if (xp->call_flags != flags)
  {
    return 0;
  }

#ifdef DDSI_INCLUDE_NETWORK_PARTITIONS
  /* Don't mix up xmsg for different encoders */
  if (xp->encoderId != m->encoderid)
    return 0;
#endif

  return addressing_info_eq_onesidederr (xp, m);
}

static int guid_prefix_eq (const nn_guid_prefix_t *a, const nn_guid_prefix_t *b)
{
  return a->u[0] == b->u[0] && a->u[1] == b->u[1] && a->u[2] == b->u[2];
}

int nn_xpack_addmsg (struct nn_xpack *xp, struct nn_xmsg *m, const uint32_t flags)
{
  /* Returns > 0 if pack got sent out before adding m */

  static InfoDST_t static_zero_dst = {
    { SMID_INFO_DST, (PLATFORM_IS_LITTLE_ENDIAN ? SMFLAG_ENDIANNESS : 0), sizeof (nn_guid_prefix_t) },
    { { 0,0,0,0, 0,0,0,0, 0,0,0,0 } }
  };
  InfoDST_t *dst;
  size_t niov;
  size_t sz;
  int result = 0;
  size_t xpo_niov = 0;
  uint32_t xpo_sz = 0;

  assert (m->kind != NN_XMSG_KIND_DATA_REXMIT || m->kindspecific.data.readerId_off != 0);

  assert (m->sz > 0);
  assert (m->dstmode != NN_XMSG_DST_UNSET);

  /* Submessage offset must be a multiple of 4 to meet alignment
     requirement (DDSI 2.1, 9.4.1).  If we keep everything 4-byte
     aligned all the time, we don't need to check for padding here. */
  assert ((xp->msg_len.length % 4) == 0);
  assert ((m->sz % 4) == 0);
  assert (m->refd_payload == NULL || (m->refd_payload_iov.iov_len % 4) == 0);

  if (!nn_xpack_mayaddmsg (xp, m, flags))
  {
    assert (xp->niov > 0);
    nn_xpack_send (xp, false);
    assert (nn_xpack_mayaddmsg (xp, m, flags));
    result = 1;
  }

  niov = xp->niov;
  sz = xp->msg_len.length;

  /* We try to merge iovecs, but we can never merge across messages
     because of all the headers. So we can speculatively start adding
     the submessage to the pack, and if we can't transmit and restart.
     But do make sure we can't run out of iovecs. */
  assert (niov + NN_XMSG_MAX_SUBMESSAGE_IOVECS <= NN_XMSG_MAX_MESSAGE_IOVECS);

  DDS_TRACE("xpack_addmsg %p %p %u(", (void *) xp, (void *) m, flags);
  switch (m->kind)
  {
    case NN_XMSG_KIND_CONTROL:
      DDS_TRACE("control");
      break;
    case NN_XMSG_KIND_DATA:
    case NN_XMSG_KIND_DATA_REXMIT:
      DDS_TRACE("%s(%x:%x:%x:%x:#%"PRId64"/%u)",
              (m->kind == NN_XMSG_KIND_DATA) ? "data" : "rexmit",
              PGUID (m->kindspecific.data.wrguid),
              m->kindspecific.data.wrseq,
              m->kindspecific.data.wrfragid + 1);
      break;
  }
  DDS_TRACE("): niov %d sz %"PRIuSIZE, (int) niov, sz);

  /* If a fresh xp has been provided, add an RTPS header */

  if (niov == 0)
  {
    copy_addressing_info (xp, m);
    xp->hdr.guid_prefix = m->data->src.guid_prefix;
    xp->iov[niov].iov_base = (void*) &xp->hdr;
    xp->iov[niov].iov_len = sizeof (xp->hdr);
    sz = xp->iov[niov].iov_len;
    niov++;

    /* Add MSG_LEN sub message for stream based transports */

    if (xp->conn->m_stream)
    {
      xp->iov[niov].iov_base = (void*) &xp->msg_len;
      xp->iov[niov].iov_len = sizeof (xp->msg_len);
      sz += sizeof (xp->msg_len);
      niov++;
    }

#ifdef DDSI_INCLUDE_NETWORK_PARTITIONS
    xp->encoderId = m->encoderid;
#endif
#ifdef DDSI_INCLUDE_ENCRYPTION
    if (xp->encoderId > 0 && (q_security_plugin.encoder_type) (xp->codec, xp->encoderId) != Q_CIPHER_NONE)
    {
      /* Insert a reference to the security header
         the correct size will be set upon encryption in q_xpack_sendmsg_encoded */
      xp->iov[niov].iov_base = (void*) &xp->SecurityHeader;
      xp->iov[niov].iov_len = sizeof (xp->SecurityHeader);
      sz += xp->iov[niov].iov_len;
      niov++;
    }
#endif
    xp->last_src = &xp->hdr.guid_prefix;
    xp->last_dst = NULL;
  }
  else
  {
    xpo_niov = xp->niov;
    xpo_sz = xp->msg_len.length;
    if (!guid_prefix_eq (xp->last_src, &m->data->src.guid_prefix))
    {
      /* If m's source participant differs from that of the source
         currently set in the packed message, add an InfoSRC note. */
      xp->iov[niov].iov_base = (void*) &m->data->src;
      xp->iov[niov].iov_len = sizeof (m->data->src);
      sz += sizeof (m->data->src);
      xp->last_src = &m->data->src.guid_prefix;
      niov++;
    }
  }

  /* We try to merge iovecs by checking iov[niov-1].  We used to check
     addressing_info_eq_onesidederr here (again), but can't because it
     relies on an imprecise check that may (timing-dependent) return
     false incorrectly */
  assert (niov >= 1);

  /* Adding this message may shorten the time this xpack may linger */
  if (m->maxdelay < xp->maxdelay)
    xp->maxdelay = m->maxdelay;

  /* If m's dst differs from that of the dst currently set in the
     packed message, add an InfoDST note. Note that neither has to
     have a dst set. */
  if (xp->last_dst == NULL)
    dst = (m->dstmode == NN_XMSG_DST_ONE) ? &m->data->dst : NULL;
  else if (m->dstmode != NN_XMSG_DST_ONE)
    dst = &static_zero_dst;
  else
    dst = guid_prefix_eq (&xp->last_dst->guid_prefix, &m->data->dst.guid_prefix) ? NULL : &m->data->dst;

  if (dst)
  {
    /* Try to merge iovecs, a few large ones should be more efficient
       than many small ones */
    if ((char *) xp->iov[niov-1].iov_base + xp->iov[niov-1].iov_len == (char *) dst)
    {
      xp->iov[niov-1].iov_len += sizeof (*dst);
    }
    else
    {
      xp->iov[niov].iov_base = (void*) dst;
      xp->iov[niov].iov_len = sizeof (*dst);
      niov++;
    }
    sz += sizeof (*dst);
    xp->last_dst = dst;
  }

  /* Append submessage; can possibly be merged with preceding iovec */
  if ((char *) xp->iov[niov-1].iov_base + xp->iov[niov-1].iov_len == (char *) m->data->payload)
    xp->iov[niov-1].iov_len += (os_iov_len_t)m->sz;
  else
  {
    xp->iov[niov].iov_base = m->data->payload;
    xp->iov[niov].iov_len = (os_iov_len_t)m->sz;
    niov++;
  }
  sz += m->sz;

  /* Append ref'd payload if given; whoever constructed the message
     should've taken care of proper alignment for the payload.  The
     ref'd payload is always at some weird address, so no chance of
     merging iovecs here. */
  if (m->refd_payload)
  {
    xp->iov[niov] = m->refd_payload_iov;
    sz += m->refd_payload_iov.iov_len;
    niov++;
  }

  /* Shouldn't've overrun iov, and shouldn't've tried to add a
     submessage that is too large for a message ... but the latter
     isn't worth checking. */
  assert (niov <= NN_XMSG_MAX_MESSAGE_IOVECS);

  /* Set total message length in MSG_LEN sub message */
  assert((uint32_t)sz == sz);
  xp->msg_len.length = (uint32_t) sz;
  xp->niov = niov;

  if (xpo_niov > 0 && sz > config.max_msg_size)
  {
    DDS_TRACE(" => now niov %d sz %"PRIuSIZE" > max_msg_size %u, nn_xpack_send niov %d sz %u now\n", (int) niov, sz, config.max_msg_size, (int) xpo_niov, xpo_sz);
    xp->msg_len.length = xpo_sz;
    xp->niov = xpo_niov;
    nn_xpack_send (xp, false);
    result = nn_xpack_addmsg (xp, m, flags); /* Retry on emptied xp */
  }
  else
  {
    xp->call_flags = flags;
    nn_xmsg_chain_add (&xp->included_msgs, m);
    DDS_TRACE(" => now niov %d sz %"PRIuSIZE"\n", (int) niov, sz);
  }

  return result;
}

int64_t nn_xpack_maxdelay (const struct nn_xpack *xp)
{
  return xp->maxdelay;
}

unsigned nn_xpack_packetid (const struct nn_xpack *xp)
{
  return xp->packetid;
}

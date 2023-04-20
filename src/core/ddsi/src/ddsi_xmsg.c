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
#include <assert.h>
#include <stdlib.h>
#include <string.h>

#ifndef _WIN32
#include <limits.h> /* for IOV_MAX */
#endif

#include "dds/ddsrt/atomics.h"
#include "dds/ddsrt/heap.h"
#include "dds/ddsrt/random.h"
#include "dds/ddsrt/avl.h"
#include "dds/ddsi/ddsi_xqos.h"
#include "dds/ddsi/ddsi_log.h"
#include "dds/ddsi/ddsi_unused.h"
#include "dds/ddsi/ddsi_domaingv.h"
#include "dds/ddsi/ddsi_serdata.h"
#include "dds/ddsi/ddsi_freelist.h"
#include "ddsi__protocol.h"
#include "ddsi__addrset.h"
#include "ddsi__misc.h"
#include "ddsi__xmsg.h"
#include "ddsi__entity.h"
#include "ddsi__entity_index.h"
#include "ddsi__security_omg.h"
#include "ddsi__endpoint.h"
#include "ddsi__plist.h"
#include "ddsi__tran.h"
#include "ddsi__vendor.h"

#define DDSI_XMSG_MAX_ALIGN 8
#define DDSI_XMSG_CHUNK_SIZE 128

struct ddsi_xmsgpool {
  struct ddsi_freelist freelist;
};

struct ddsi_xmsg_data {
  ddsi_rtps_info_src_t src;
  ddsi_rtps_info_dst_t dst;
  char payload[]; /* of size maxsz */
};

struct ddsi_xmsg_chain_elem {
  struct ddsi_xmsg_chain_elem *older;
};

enum ddsi_xmsg_dstmode {
  NN_XMSG_DST_UNSET,
  NN_XMSG_DST_ONE,
  NN_XMSG_DST_ALL,
  NN_XMSG_DST_ALL_UC
};

struct ddsi_xmsg {
  struct ddsi_xmsgpool *pool;
  size_t maxsz;
  size_t sz;
  int have_params;
  struct ddsi_serdata *refd_payload;
  ddsrt_iovec_t refd_payload_iov;
#ifdef DDS_HAS_SECURITY
  /* Used as pointer to contain encoded payload to which iov can alias. */
  unsigned char *refd_payload_encoded;
  ddsi_msg_sec_info_t sec_info;
#endif
  int64_t maxdelay;

  /* Backref for late updating of available sequence numbers, and
     merging of retransmits. */
  enum ddsi_xmsg_kind kind;
  union {
    char control;
    struct {
      ddsi_guid_t wrguid;
      ddsi_seqno_t wrseq;
      ddsi_fragment_number_t wrfragid;
      /* readerId encodes offset to destination readerId or 0 -- used
         only for rexmits, but more convenient to combine both into
         one struct in the union */
      unsigned readerId_off;
    } data;
  } kindspecific;

  enum ddsi_xmsg_dstmode dstmode;
  union {
    struct {
      ddsi_xlocator_t loc;  /* send just to this locator */
    } one;
    struct {
      struct ddsi_addrset *as;       /* send to all addresses in set */
    } all;
    struct {
      struct ddsi_addrset *as;       /* send to all unicast addresses in set */
    } all_uc;
  } dstaddr;

  struct ddsi_xmsg_chain_elem link;
  struct ddsi_xmsg_data *data;
};

/* Worst-case: change of SRC [+1] but no DST, submessage [+1], ref'd
   payload [+1].  So 128 iovecs => at least ~40 submessages, so for
   very small ones still >1kB. */
#define DDSI_XMSG_MAX_SUBMESSAGE_IOVECS 3

#ifdef IOV_MAX
#if IOV_MAX > 0 && IOV_MAX < 256
#define DDSI_XMSG_MAX_MESSAGE_IOVECS IOV_MAX
#endif
#endif /* defined IOV_MAX */
#ifndef DDSI_XMSG_MAX_MESSAGE_IOVECS
#define DDSI_XMSG_MAX_MESSAGE_IOVECS 256
#endif

/* Used to keep them in order, but it now transpires that delayed
   updating of writer seq nos benefits from having them in the
   reverse order.  They are not being used for anything else, so
   we no longer maintain a pointer to both ends. */
struct ddsi_xmsg_chain {
  struct ddsi_xmsg_chain_elem *latest;
};

struct ddsi_xpack
{
  struct ddsi_xpack *sendq_next;
  bool async_mode;
  ddsi_rtps_header_t hdr;
  ddsi_rtps_msg_len_t msg_len;
  ddsi_guid_prefix_t *last_src;
  ddsi_rtps_info_dst_t *last_dst;
  int64_t maxdelay;
  unsigned packetid;
  ddsrt_atomic_uint32_t calls;
  uint32_t call_flags;
  size_t niov;
  ddsrt_iovec_t *iov;
  enum ddsi_xmsg_dstmode dstmode;
  struct ddsi_domaingv *gv;

  union
  {
    ddsi_xlocator_t loc; /* send just to this locator */
    struct
    {
      struct ddsi_addrset *as;        /* send to all addresses in set */
    } all;
    struct
    {
      struct ddsi_addrset *as;        /* send to all unicast addresses in set */
    } all_uc;
  } dstaddr;

  bool includes_rexmit;
  struct ddsi_xmsg_chain included_msgs;

#ifdef DDS_HAS_NETWORK_PARTITIONS
  uint32_t encoderId;
#endif /* DDS_HAS_NETWORK_PARTITIONS */
#ifdef DDS_HAS_SECURITY
  ddsi_msg_sec_info_t sec_info;
#endif
};

static size_t align4u (size_t x)
{
  return (x + 3) & ~(size_t)3;
}

/* XMSGPOOL ------------------------------------------------------------

   Great expectations, but so far still wanting. */

/* We need about as many as will fit in a message; an otherwise unadorned data message is ~ 40 bytes
   for a really small sample, no key hash, no status info, and message sizes are (typically) < 64kB
   so we can expect not to need more than ~ 1600 xmsg at a time.  Powers-of-two are nicer :) */
#define MAX_FREELIST_SIZE 2048

static void ddsi_xmsg_realfree (struct ddsi_xmsg *m);

struct ddsi_xmsgpool *ddsi_xmsgpool_new (void)
{
  struct ddsi_xmsgpool *pool;
  pool = ddsrt_malloc (sizeof (*pool));
  ddsi_freelist_init (&pool->freelist, MAX_FREELIST_SIZE, offsetof (struct ddsi_xmsg, link.older));
  return pool;
}

static void ddsi_xmsg_realfree_wrap (void *elem)
{
  ddsi_xmsg_realfree (elem);
}

void ddsi_xmsgpool_free (struct ddsi_xmsgpool *pool)
{
  ddsi_freelist_fini (&pool->freelist, ddsi_xmsg_realfree_wrap);
  ddsrt_free (pool);
}

/* XMSG ----------------------------------------------------------------

   All messages that are sent start out as xmsgs, which is a sequence
   of submessages potentially ending with a blob of serialized data.
   Such serialized data is given as a reference to part of a serdata.

   An xmsg can be queued for transmission, after which it must be
   forgotten by its creator.  The queue handler packs them into xpacks
   (see below), transmits them, and releases them.
*/

static void ddsi_xmsg_reinit (struct ddsi_xmsg *m, enum ddsi_xmsg_kind kind)
{
  m->sz = 0;
  m->have_params = 0;
  m->refd_payload = NULL;
  m->dstmode = NN_XMSG_DST_UNSET;
  m->kind = kind;
  m->maxdelay = 0;
#ifdef DDS_HAS_SECURITY
  m->refd_payload_encoded = NULL;
  m->sec_info.use_rtps_encoding = 0;
  m->sec_info.src_pp_handle = 0;
  m->sec_info.dst_pp_handle = 0;
#endif
  memset (&m->kindspecific, 0, sizeof (m->kindspecific));
}

static struct ddsi_xmsg *ddsi_xmsg_allocnew (struct ddsi_xmsgpool *pool, size_t expected_size, enum ddsi_xmsg_kind kind)
{
  struct ddsi_xmsg *m;
  struct ddsi_xmsg_data *d;

  if (expected_size == 0)
    expected_size = DDSI_XMSG_CHUNK_SIZE;

  if ((m = ddsrt_malloc (sizeof (*m))) == NULL)
    return NULL;

  m->pool = pool;
  m->maxsz = (expected_size + DDSI_XMSG_CHUNK_SIZE - 1) & (unsigned)-DDSI_XMSG_CHUNK_SIZE;

  if ((d = m->data = ddsrt_malloc (offsetof (struct ddsi_xmsg_data, payload) + m->maxsz)) == NULL)
  {
    ddsrt_free (m);
    return NULL;
  }
  d->src.smhdr.submessageId = DDSI_RTPS_SMID_INFO_SRC;
  d->src.smhdr.flags = (DDSRT_ENDIAN == DDSRT_LITTLE_ENDIAN ? DDSI_RTPS_SUBMESSAGE_FLAG_ENDIANNESS : 0);
  d->src.smhdr.octetsToNextHeader = sizeof (d->src) - (offsetof (ddsi_rtps_info_src_t, smhdr.octetsToNextHeader) + 2);
  d->src.unused = 0;
  d->src.version.major = DDSI_RTPS_MAJOR;
  d->src.version.minor = DDSI_RTPS_MINOR;
  d->src.vendorid = DDSI_VENDORID_ECLIPSE;
  d->dst.smhdr.submessageId = DDSI_RTPS_SMID_INFO_DST;
  d->dst.smhdr.flags = (DDSRT_ENDIAN == DDSRT_LITTLE_ENDIAN ? DDSI_RTPS_SUBMESSAGE_FLAG_ENDIANNESS : 0);
  d->dst.smhdr.octetsToNextHeader = sizeof (d->dst.guid_prefix);
  ddsi_xmsg_reinit (m, kind);
  return m;
}

struct ddsi_xmsg *ddsi_xmsg_new (struct ddsi_xmsgpool *pool, const ddsi_guid_t *src_guid, struct ddsi_participant *pp, size_t expected_size, enum ddsi_xmsg_kind kind)
{
  struct ddsi_xmsg *m;
  if ((m = ddsi_freelist_pop (&pool->freelist)) != NULL)
    ddsi_xmsg_reinit (m, kind);
  else if ((m = ddsi_xmsg_allocnew (pool, expected_size, kind)) == NULL)
    return NULL;
  m->data->src.guid_prefix = ddsi_hton_guid_prefix (src_guid->prefix);

#ifdef DDS_HAS_SECURITY
  m->sec_info.use_rtps_encoding = 0;
  if (pp && ddsi_omg_participant_is_secure(pp))
  {
    if (ddsi_omg_security_is_local_rtps_protected (pp, src_guid->entityid))
    {
      m->sec_info.use_rtps_encoding = 1;
      m->sec_info.src_pp_handle = ddsi_omg_security_get_local_participant_handle (pp);
    }
  }
#else
  DDSRT_UNUSED_ARG(pp);
#endif

  return m;
}

static void ddsi_xmsg_realfree (struct ddsi_xmsg *m)
{
  ddsrt_free (m->data);
  ddsrt_free (m);
}

void ddsi_xmsg_free (struct ddsi_xmsg *m)
{
  struct ddsi_xmsgpool *pool = m->pool;
  if (m->refd_payload)
    ddsi_serdata_to_ser_unref (m->refd_payload, &m->refd_payload_iov);
#ifdef DDS_HAS_SECURITY
  ddsrt_free(m->refd_payload_encoded);
#endif
  switch (m->dstmode)
  {
    case NN_XMSG_DST_UNSET:
    case NN_XMSG_DST_ONE:
      break;
    case NN_XMSG_DST_ALL:
      ddsi_unref_addrset (m->dstaddr.all.as);
      break;
    case NN_XMSG_DST_ALL_UC:
      ddsi_unref_addrset (m->dstaddr.all_uc.as);
      break;
  }
  /* Only cache the smallest xmsgs; data messages store the payload by reference and are small */
  if (m->maxsz > DDSI_XMSG_CHUNK_SIZE || !ddsi_freelist_push (&pool->freelist, m))
  {
    ddsi_xmsg_realfree (m);
  }
}

/************************************************/

#ifndef NDEBUG
static int submsg_is_compatible (const struct ddsi_xmsg *msg, ddsi_rtps_submessage_kind_t smkind)
{
  switch (msg->kind)
  {
    case DDSI_XMSG_KIND_CONTROL:
      switch (smkind)
      {
        case DDSI_RTPS_SMID_PAD:
          /* never use this one -- so let's crash when we do :) */
          return 0;
        case DDSI_RTPS_SMID_INFO_SRC: case DDSI_RTPS_SMID_INFO_REPLY_IP4:
        case DDSI_RTPS_SMID_INFO_DST: case DDSI_RTPS_SMID_INFO_REPLY:
          /* we never generate these directly */
          return 0;
        case DDSI_RTPS_SMID_INFO_TS:
        case DDSI_RTPS_SMID_ACKNACK: case DDSI_RTPS_SMID_HEARTBEAT:
        case DDSI_RTPS_SMID_GAP: case DDSI_RTPS_SMID_NACK_FRAG:
        case DDSI_RTPS_SMID_HEARTBEAT_FRAG:
        case DDSI_RTPS_SMID_ADLINK_MSG_LEN:
        case DDSI_RTPS_SMID_ADLINK_ENTITY_ID:
          /* normal control stuff is ok */
          return 1;
        case DDSI_RTPS_SMID_DATA: case DDSI_RTPS_SMID_DATA_FRAG:
          /* but data is strictly verboten */
          return 0;
        case DDSI_RTPS_SMID_SEC_BODY:
        case DDSI_RTPS_SMID_SEC_PREFIX:
        case DDSI_RTPS_SMID_SEC_POSTFIX:
        case DDSI_RTPS_SMID_SRTPS_PREFIX:
        case DDSI_RTPS_SMID_SRTPS_POSTFIX:
          /* and the security sm are basically data. */
          return 0;
      }
      assert (0);
      break;
    case DDSI_XMSG_KIND_DATA:
    case DDSI_XMSG_KIND_DATA_REXMIT:
    case DDSI_XMSG_KIND_DATA_REXMIT_NOMERGE:
      switch (smkind)
      {
        case DDSI_RTPS_SMID_PAD:
          /* never use this one -- so let's crash when we do :) */
          return 0;
        case DDSI_RTPS_SMID_INFO_SRC: case DDSI_RTPS_SMID_INFO_REPLY_IP4:
        case DDSI_RTPS_SMID_INFO_DST: case DDSI_RTPS_SMID_INFO_REPLY:
          /* we never generate these directly */
          return 0;
        case DDSI_RTPS_SMID_INFO_TS: case DDSI_RTPS_SMID_DATA: case DDSI_RTPS_SMID_DATA_FRAG:
          /* Timestamp only preceding data; data may be present just
             once for rexmits.  The readerId offset can be used to
             ensure rexmits have only one data submessages -- the test
             won't work for initial transmits, but those currently
             don't allow a readerId */
          return msg->kindspecific.data.readerId_off == 0;
        case DDSI_RTPS_SMID_SEC_BODY:
        case DDSI_RTPS_SMID_SEC_PREFIX:
        case DDSI_RTPS_SMID_SEC_POSTFIX:
        case DDSI_RTPS_SMID_SRTPS_PREFIX:
        case DDSI_RTPS_SMID_SRTPS_POSTFIX:
          /* Just do the same as 'normal' data sm. */
          return msg->kindspecific.data.readerId_off == 0;
        case DDSI_RTPS_SMID_ACKNACK:
        case DDSI_RTPS_SMID_HEARTBEAT:
        case DDSI_RTPS_SMID_GAP:
        case DDSI_RTPS_SMID_NACK_FRAG:
        case DDSI_RTPS_SMID_HEARTBEAT_FRAG:
        case DDSI_RTPS_SMID_ADLINK_MSG_LEN:
        case DDSI_RTPS_SMID_ADLINK_ENTITY_ID:
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

int ddsi_xmsg_compare_fragid (const struct ddsi_xmsg *a, const struct ddsi_xmsg *b)
{
  int c;
  assert (a->kind == DDSI_XMSG_KIND_DATA_REXMIT);
  assert (b->kind == DDSI_XMSG_KIND_DATA_REXMIT);
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

size_t ddsi_xmsg_size (const struct ddsi_xmsg *m)
{
  return m->sz + (m->refd_payload ? (size_t) m->refd_payload_iov.iov_len : 0);
}

enum ddsi_xmsg_kind ddsi_xmsg_kind (const struct ddsi_xmsg *m)
{
  return m->kind;
}

void ddsi_xmsg_guid_seq_fragid (const struct ddsi_xmsg *m, ddsi_guid_t *wrguid, ddsi_seqno_t *wrseq, ddsi_fragment_number_t *wrfragid)
{
  assert (m->kind != DDSI_XMSG_KIND_CONTROL);
  *wrguid = m->kindspecific.data.wrguid;
  *wrseq = m->kindspecific.data.wrseq;
  *wrfragid = m->kindspecific.data.wrfragid;
}

void *ddsi_xmsg_payload (size_t *sz, struct ddsi_xmsg *m)
{
  *sz = m->sz;
  return m->data->payload;
}

void ddsi_xmsg_payload_to_plistsample (struct ddsi_plist_sample *dst, ddsi_parameterid_t keyparam, const struct ddsi_xmsg *m)
{
  dst->blob = m->data->payload;
  dst->size = m->sz;
  dst->keyparam = keyparam;
}

void ddsi_xmsg_submsg_init (struct ddsi_xmsg *msg, struct ddsi_xmsg_marker marker, ddsi_rtps_submessage_kind_t smkind)
{
  ddsi_rtps_submessage_header_t *hdr = (ddsi_rtps_submessage_header_t *) (msg->data->payload + marker.offset);
  assert (submsg_is_compatible (msg, smkind));
  hdr->submessageId = (unsigned char)smkind;
  hdr->flags = (DDSRT_ENDIAN == DDSRT_LITTLE_ENDIAN ? DDSI_RTPS_SUBMESSAGE_FLAG_ENDIANNESS : 0);
  hdr->octetsToNextHeader = 0;
}

void ddsi_xmsg_submsg_setnext (struct ddsi_xmsg *msg, struct ddsi_xmsg_marker marker)
{
  ddsi_rtps_submessage_header_t *hdr = (ddsi_rtps_submessage_header_t *) (msg->data->payload + marker.offset);
  unsigned plsize = msg->refd_payload ? (unsigned) msg->refd_payload_iov.iov_len : 0;
  assert ((msg->sz % 4) == 0);
  assert ((plsize % 4) == 0);
  assert ((unsigned) (msg->data->payload + msg->sz + plsize - (char *) hdr) >= DDSI_RTPS_SUBMESSAGE_HEADER_SIZE);
  hdr->octetsToNextHeader = (unsigned short)
    ((unsigned)(msg->data->payload + msg->sz + plsize - (char *) hdr) - DDSI_RTPS_SUBMESSAGE_HEADER_SIZE);
}

#ifdef DDS_HAS_SECURITY

size_t ddsi_xmsg_submsg_size (struct ddsi_xmsg *msg, struct ddsi_xmsg_marker marker)
{
  ddsi_rtps_submessage_header_t *hdr = (ddsi_rtps_submessage_header_t*)ddsi_xmsg_submsg_from_marker(msg, marker);
  return align4u(hdr->octetsToNextHeader + sizeof(ddsi_rtps_submessage_header_t));
}

void ddsi_xmsg_submsg_remove(struct ddsi_xmsg *msg, struct ddsi_xmsg_marker sm_marker)
{
  /* Just reset the message size to the start of the current sub-message. */
  msg->sz = sm_marker.offset;

  /* Deleting the submessage means the readerId offset in a DATA_REXMIT message is no
     longer valid.  Converting the message kind to a _NOMERGE one ensures no subsequent
     operation will assume its validity. */
  if (msg->kind == DDSI_XMSG_KIND_DATA_REXMIT)
    msg->kind = DDSI_XMSG_KIND_DATA_REXMIT_NOMERGE;
}

void ddsi_xmsg_submsg_replace(struct ddsi_xmsg *msg, struct ddsi_xmsg_marker sm_marker, unsigned char *new_submsg, size_t new_len)
{
  /* Size of current sub-message. */
  size_t old_len = msg->sz - sm_marker.offset;

  /* Adjust the message size to the new sub-message. */
  if (old_len < new_len)
  {
    ddsi_xmsg_append(msg, NULL, new_len - old_len);
  }
  else if (old_len > new_len)
  {
    ddsi_xmsg_shrink(msg, sm_marker, new_len);
  }

  /* Just a sanity check: assert(msg_end == submsg_end) */
  assert((msg->data->payload + msg->sz) == (msg->data->payload + sm_marker.offset + new_len));

  /* Replace the sub-message. */
  memcpy(msg->data->payload + sm_marker.offset, new_submsg, new_len);

  /* The replacement submessage may have undergone any transformation and so the readerId
     offset in a DATA_REXMIT message is potentially no longer valid.  Converting the
     message kind to a _NOMERGE one ensures no subsequent operation will assume its
     validity.  This is used by the security implementation when encrypting and/or signing
     messages and apart from the offset possibly no longer being valid (for which one
     might conceivably be able to correct), there is also the issue that it may now be
     meaningless junk or that rewriting it would make the receiver reject it as having
     been tampered with. */
  if (msg->kind == DDSI_XMSG_KIND_DATA_REXMIT)
    msg->kind = DDSI_XMSG_KIND_DATA_REXMIT_NOMERGE;
}

void ddsi_xmsg_submsg_append_refd_payload(struct ddsi_xmsg *msg, struct ddsi_xmsg_marker sm_marker)
{
  DDSRT_UNUSED_ARG(sm_marker);
  /*
   * Normally, the refd payload pointer is moved around until it is added to
   * the iov of the socket. This reduces the amount of allocations and copies.
   *
   * However, in a few cases (like security), the sub-message should be one
   * complete blob.
   * Appending the payload will just do that.
   */
  if (msg->refd_payload)
  {
    void *dst;

    /* Get payload information. */
    char  *payload_ptr = msg->refd_payload_iov.iov_base;
    size_t payload_len = msg->refd_payload_iov.iov_len;

    /* Make space for the payload (dst points to the start of the appended space). */
    dst = ddsi_xmsg_append(msg, NULL, payload_len);

    /* Copy the payload into the submessage. */
    memcpy(dst, payload_ptr, payload_len);

    /* No need to remember the payload now. */
    ddsi_serdata_unref(msg->refd_payload);
    msg->refd_payload = NULL;
    if (msg->refd_payload_encoded)
    {
      ddsrt_free(msg->refd_payload_encoded);
      msg->refd_payload_encoded = NULL;
    }
  }
}

#endif /* DDS_HAS_SECURITY */

void *ddsi_xmsg_submsg_from_marker (struct ddsi_xmsg *msg, struct ddsi_xmsg_marker marker)
{
  return msg->data->payload + marker.offset;
}

void *ddsi_xmsg_append (struct ddsi_xmsg *m, struct ddsi_xmsg_marker *marker, size_t sz)
{
  static const size_t a = 4;

  /* May realloc, in which case m may change.  But that will not
     happen if you do not exceed expected_size.  Max size is always a
     multiple of A: that means we don't have to worry about memory
     available just for alignment. */
  char *p;
  assert (1 <= a && a <= DDSI_XMSG_MAX_ALIGN);
  assert ((m->maxsz % a) == 0);
  if ((m->sz % a) != 0)
  {
    size_t npad = a - (m->sz % a);
    memset (m->data->payload + m->sz, 0, npad);
    m->sz += npad;
  }
  if (m->sz + sz > m->maxsz)
  {
    size_t nmax = (m->maxsz + sz + DDSI_XMSG_CHUNK_SIZE - 1) & (size_t)-DDSI_XMSG_CHUNK_SIZE;
    struct ddsi_xmsg_data *ndata = ddsrt_realloc (m->data, offsetof (struct ddsi_xmsg_data, payload) + nmax);
    m->maxsz = nmax;
    m->data = ndata;
  }
  p = m->data->payload + m->sz;
  if (marker)
    marker->offset = m->sz;
  m->sz += sz;
  return p;
}

void ddsi_xmsg_shrink (struct ddsi_xmsg *m, struct ddsi_xmsg_marker marker, size_t sz)
{
  assert (m != NULL);
  assert (marker.offset <= m->sz);
  assert (marker.offset + sz <= m->sz);
  m->sz = marker.offset + sz;
}

void ddsi_xmsg_add_timestamp (struct ddsi_xmsg *m, ddsrt_wctime_t t)
{
  ddsi_rtps_info_ts_t * ts;
  struct ddsi_xmsg_marker sm;

  ts = (ddsi_rtps_info_ts_t*) ddsi_xmsg_append (m, &sm, sizeof (ddsi_rtps_info_ts_t));
  ddsi_xmsg_submsg_init (m, sm, DDSI_RTPS_SMID_INFO_TS);
  ts->time = ddsi_wctime_to_ddsi_time (t);
  ddsi_xmsg_submsg_setnext (m, sm);
}

void ddsi_xmsg_add_entityid (struct ddsi_xmsg * m)
{
  ddsi_rtps_entityid_t * eid;
  struct ddsi_xmsg_marker sm;

  eid = (ddsi_rtps_entityid_t*) ddsi_xmsg_append (m, &sm, sizeof (ddsi_rtps_entityid_t));
  ddsi_xmsg_submsg_init (m, sm, DDSI_RTPS_SMID_ADLINK_ENTITY_ID);
  eid->entityid.u = DDSI_ENTITYID_PARTICIPANT;
  ddsi_xmsg_submsg_setnext (m, sm);
}

void ddsi_xmsg_serdata (struct ddsi_xmsg *m, struct ddsi_serdata *serdata, size_t off, size_t len, struct ddsi_writer *wr)
{
  if (serdata->kind != SDK_EMPTY)
  {
    size_t len4 = align4u (len);
    assert (m->refd_payload == NULL);
    m->refd_payload = ddsi_serdata_to_ser_ref (serdata, off, len4, &m->refd_payload_iov);

#ifdef DDS_HAS_SECURITY
    assert (m->refd_payload_encoded == NULL);
    /* When encoding is necessary, m->refd_payload_encoded will be allocated
     * and m->refd_payload_iov contents will change to point to that buffer.
     * If no encoding is necessary, nothing changes. */
    if (!ddsi_security_encode_payload(wr, &(m->refd_payload_iov), &(m->refd_payload_encoded)))
    {
      DDS_CWARNING (&wr->e.gv->logconfig, "ddsi_xmsg_serdata: failed to encrypt data for "PGUIDFMT"", PGUID (wr->e.guid));
      ddsi_serdata_to_ser_unref (m->refd_payload, &m->refd_payload_iov);
      assert (m->refd_payload_encoded == NULL);
      m->refd_payload_iov.iov_base = NULL;
      m->refd_payload_iov.iov_len = 0;
      m->refd_payload = NULL;
    }
#else
    DDSRT_UNUSED_ARG(wr);
#endif
  }
}

static void ddsi_xmsg_setdst1_common (struct ddsi_domaingv *gv, struct ddsi_xmsg *m, const ddsi_guid_prefix_t *gp)
{
  m->data->dst.guid_prefix = ddsi_hton_guid_prefix (*gp);
#ifdef DDS_HAS_SECURITY
  if (m->sec_info.use_rtps_encoding && !m->sec_info.dst_pp_handle)
  {
    struct ddsi_proxy_participant *proxypp;
    ddsi_guid_t guid;

    guid.prefix = *gp;
    guid.entityid.u = DDSI_ENTITYID_PARTICIPANT;

    proxypp = ddsi_entidx_lookup_proxy_participant_guid(gv->entity_index, &guid);
    if (proxypp)
      m->sec_info.dst_pp_handle = ddsi_omg_security_get_remote_participant_handle (proxypp);
  }
#else
  DDSRT_UNUSED_ARG(gv);
#endif
}

void ddsi_xmsg_setdst1 (struct ddsi_domaingv *gv, struct ddsi_xmsg *m, const ddsi_guid_prefix_t *gp, const ddsi_xlocator_t *loc)
{
  assert (m->dstmode == NN_XMSG_DST_UNSET);
  m->dstmode = NN_XMSG_DST_ONE;
  m->dstaddr.one.loc = *loc;
  ddsi_xmsg_setdst1_common (gv, m, gp);
}

bool ddsi_xmsg_getdst1_prefix (struct ddsi_xmsg *m, ddsi_guid_prefix_t *gp)
{
  if (m->dstmode == NN_XMSG_DST_ONE)
  {
    *gp = ddsi_hton_guid_prefix(m->data->dst.guid_prefix);
    return true;
  }
  return false;
}

void ddsi_xmsg_setdst_prd (struct ddsi_xmsg *m, const struct ddsi_proxy_reader *prd)
{
  // only accepting endpoints that have an address
  assert (m->dstmode == NN_XMSG_DST_UNSET);
  if (!prd->redundant_networking)
  {
    ddsi_xlocator_t loc;
    ddsi_addrset_any_uc_else_mc_nofail (prd->c.as, &loc);
    ddsi_xmsg_setdst1 (prd->e.gv, m, &prd->e.guid.prefix, &loc);
  }
  else
  {
    // FIXME: maybe I should work on the merging instead ...
    if (m->kind == DDSI_XMSG_KIND_DATA_REXMIT)
      m->kind = DDSI_XMSG_KIND_DATA_REXMIT_NOMERGE;
    m->dstmode = NN_XMSG_DST_ALL_UC;
    m->dstaddr.all_uc.as = ddsi_ref_addrset (prd->c.as);
    ddsi_xmsg_setdst1_common (prd->e.gv, m, &prd->e.guid.prefix);
  }
}

void ddsi_xmsg_setdst_pwr (struct ddsi_xmsg *m, const struct ddsi_proxy_writer *pwr)
{
  // only accepting endpoints that have an address
  assert (m->dstmode == NN_XMSG_DST_UNSET);
  if (!pwr->redundant_networking)
  {
    ddsi_xlocator_t loc;
    ddsi_addrset_any_uc_else_mc_nofail (pwr->c.as, &loc);
    ddsi_xmsg_setdst1 (pwr->e.gv, m, &pwr->e.guid.prefix, &loc);
  }
  else
  {
    // FIXME: maybe I should work on the merging instead ...
    if (m->kind == DDSI_XMSG_KIND_DATA_REXMIT)
      m->kind = DDSI_XMSG_KIND_DATA_REXMIT_NOMERGE;
    m->dstmode = NN_XMSG_DST_ALL_UC;
    m->dstaddr.all_uc.as = ddsi_ref_addrset (pwr->c.as);
    ddsi_xmsg_setdst1_common (pwr->e.gv, m, &pwr->e.guid.prefix);
  }
}

void ddsi_xmsg_setdst_addrset (struct ddsi_xmsg *m, struct ddsi_addrset *as)
{
  assert (m->dstmode == NN_XMSG_DST_UNSET || m->dstmode == NN_XMSG_DST_ONE);
  m->dstmode = NN_XMSG_DST_ALL;
  m->dstaddr.all.as = ddsi_ref_addrset (as);
}

void ddsi_xmsg_set_data_reader_id (struct ddsi_xmsg *m, ddsi_entityid_t *readerId)
{
  assert (m->kind == DDSI_XMSG_KIND_DATA_REXMIT || m->kind == DDSI_XMSG_KIND_DATA_REXMIT_NOMERGE);
  assert (m->kindspecific.data.readerId_off == 0);
  assert ((char *) readerId > m->data->payload);
  assert ((char *) readerId < m->data->payload + m->sz);
  m->kindspecific.data.readerId_off = (unsigned) ((char *) readerId - m->data->payload);
}

static void clear_readerId (struct ddsi_xmsg *m)
{
  assert (m->kind == DDSI_XMSG_KIND_DATA_REXMIT || m->kind == DDSI_XMSG_KIND_DATA_REXMIT_NOMERGE);
  assert (m->kindspecific.data.readerId_off != 0);
  *((ddsi_entityid_t *) (m->data->payload + m->kindspecific.data.readerId_off)) =
    ddsi_hton_entityid (ddsi_to_entityid (DDSI_ENTITYID_UNKNOWN));
}

static ddsi_entityid_t load_readerId (const struct ddsi_xmsg *m)
{
  assert (m->kind == DDSI_XMSG_KIND_DATA_REXMIT || m->kind == DDSI_XMSG_KIND_DATA_REXMIT_NOMERGE);
  assert (m->kindspecific.data.readerId_off != 0);
  return ddsi_ntoh_entityid (*((ddsi_entityid_t *) (m->data->payload + m->kindspecific.data.readerId_off)));
}

static int readerId_compatible (const struct ddsi_xmsg *m, const struct ddsi_xmsg *madd)
{
  ddsi_entityid_t e = load_readerId (m);
  ddsi_entityid_t eadd = load_readerId (madd);
  return e.u == DDSI_ENTITYID_UNKNOWN || e.u == eadd.u;
}

int ddsi_xmsg_merge_rexmit_destinations_wrlock_held (struct ddsi_domaingv *gv, struct ddsi_xmsg *m, const struct ddsi_xmsg *madd)
{
  assert (m->kindspecific.data.wrseq >= 1);
  assert (m->kindspecific.data.wrguid.prefix.u[0] != 0);
  assert (ddsi_is_writer_entityid (m->kindspecific.data.wrguid.entityid));
  assert (memcmp (&m->kindspecific.data.wrguid, &madd->kindspecific.data.wrguid, sizeof (m->kindspecific.data.wrguid)) == 0);
  assert (m->kindspecific.data.wrseq == madd->kindspecific.data.wrseq);
  assert (m->kindspecific.data.wrfragid == madd->kindspecific.data.wrfragid);
  assert (m->kind == DDSI_XMSG_KIND_DATA_REXMIT);
  assert (madd->kind == DDSI_XMSG_KIND_DATA_REXMIT);
  assert (m->kindspecific.data.readerId_off != 0);
  assert (madd->kindspecific.data.readerId_off != 0);

  GVTRACE (" ("PGUIDFMT"#%"PRId64"/%"PRIu32":", PGUID (m->kindspecific.data.wrguid), m->kindspecific.data.wrseq, m->kindspecific.data.wrfragid + 1);

  switch (m->dstmode)
  {
    case NN_XMSG_DST_UNSET:
      assert (0);
      return 0;

    case NN_XMSG_DST_ALL:
      GVTRACE ("*->*)");
      return 1;

    case NN_XMSG_DST_ALL_UC:
      GVTRACE ("all-uc)");
      return 0;

    case NN_XMSG_DST_ONE:
      switch (madd->dstmode)
      {
        case NN_XMSG_DST_UNSET:
          assert (0);
          return 0;

        case NN_XMSG_DST_ALL_UC:
          GVTRACE ("all-uc)");
          return 0;

        case NN_XMSG_DST_ALL:
          GVTRACE ("1+*->*)");
          clear_readerId (m);
          m->dstmode = NN_XMSG_DST_ALL;
          m->dstaddr.all.as = ddsi_ref_addrset (madd->dstaddr.all.as);
          return 1;

        case NN_XMSG_DST_ONE:
          if (memcmp (&m->data->dst.guid_prefix, &madd->data->dst.guid_prefix, sizeof (m->data->dst.guid_prefix)) != 0)
          {
            struct ddsi_writer *wr;
            /* This is why wr->e.lock must be held: we can't safely
               reference the writer's address set if it isn't -- so
               FIXME: add a way to atomically replace the contents of
               an addrset in ddsi_rebuild_writer_addrset: then we don't
               need the lock anymore, and the '_wrlock_held' suffix
               can go and everyone's life will become easier! */
            if ((wr = ddsi_entidx_lookup_writer_guid (gv->entity_index, &m->kindspecific.data.wrguid)) == NULL)
            {
              GVTRACE ("writer-dead)");
              return 0;
            }
            else
            {
              GVTRACE ("1+1->*)");
              clear_readerId (m);
              m->dstmode = NN_XMSG_DST_ALL;
              m->dstaddr.all.as = ddsi_ref_addrset (wr->as);
              return 1;
            }
          }
          else if (readerId_compatible (m, madd))
          {
            GVTRACE ("1+1->1)");
            return 1;
          }
          else
          {
            GVTRACE ("1+1->2)");
            clear_readerId (m);
            return 1;
          }
      }
      break;
  }
  assert (0);
  return 0;
}

int ddsi_xmsg_setmaxdelay (struct ddsi_xmsg *msg, int64_t maxdelay)
{
  assert (msg->maxdelay == 0);
  msg->maxdelay = maxdelay;
  return 0;
}

void ddsi_xmsg_setwriterseq (struct ddsi_xmsg *msg, const ddsi_guid_t *wrguid, ddsi_seqno_t wrseq)
{
  msg->kindspecific.data.wrguid = *wrguid;
  msg->kindspecific.data.wrseq = wrseq;
}

void ddsi_xmsg_setwriterseq_fragid (struct ddsi_xmsg *msg, const ddsi_guid_t *wrguid, ddsi_seqno_t wrseq, ddsi_fragment_number_t wrfragid)
{
  ddsi_xmsg_setwriterseq (msg, wrguid, wrseq);
  msg->kindspecific.data.wrfragid = wrfragid;
}

void *ddsi_xmsg_addpar_bo (struct ddsi_xmsg *m, ddsi_parameterid_t pid, size_t len, enum ddsrt_byte_order_selector bo)
{
  const size_t len4 = (len + 3) & ~(size_t)3; /* must alloc a multiple of 4 */
  ddsi_parameter_t *phdr;
  char *p;
  assert (len4 < UINT16_MAX); /* FIXME: return error */
  m->have_params = 1;
  phdr = ddsi_xmsg_append (m, NULL, sizeof (ddsi_parameter_t) + len4);
  phdr->parameterid = ddsrt_toBO2u(bo, pid);
  phdr->length = ddsrt_toBO2u(bo, (uint16_t) len4);
  p = (char *) (phdr + 1);
  /* zero out padding bytes added to satisfy parameter alignment: this way
     valgrind can tell us where we forgot to initialize something */
  while (len < len4)
    p[len++] = 0;
  return p;
}

void *ddsi_xmsg_addpar (struct ddsi_xmsg *m, ddsi_parameterid_t pid, size_t len)
{
  return ddsi_xmsg_addpar_bo(m, pid, len, DDSRT_BOSEL_NATIVE);
}

void ddsi_xmsg_addpar_keyhash (struct ddsi_xmsg *m, const struct ddsi_serdata *serdata, bool force_md5)
{
  if (serdata->kind != SDK_EMPTY)
  {
    char *p = ddsi_xmsg_addpar (m, DDSI_PID_KEYHASH, 16);
    ddsi_serdata_get_keyhash(serdata, (struct ddsi_keyhash*)p, force_md5);
  }
}

static void ddsi_xmsg_addpar_BE4u (struct ddsi_xmsg *m, ddsi_parameterid_t pid, uint32_t x)
{
  unsigned *p = ddsi_xmsg_addpar (m, pid, sizeof (x));
  *p = ddsrt_toBE4u (x);
}

void ddsi_xmsg_addpar_statusinfo (struct ddsi_xmsg *m, unsigned statusinfo)
{
  if ((statusinfo & ~DDSI_STATUSINFO_STANDARDIZED) == 0)
    ddsi_xmsg_addpar_BE4u (m, DDSI_PID_STATUSINFO, statusinfo);
  else
  {
    unsigned *p = ddsi_xmsg_addpar (m, DDSI_PID_STATUSINFO, 8);
    unsigned statusinfox = 0;
    assert ((statusinfo & ~DDSI_STATUSINFO_STANDARDIZED) == DDSI_STATUSINFO_OSPL_AUTO);
    if (statusinfo & DDSI_STATUSINFO_OSPL_AUTO)
      statusinfox |= DDSI_STATUSINFOX_OSPL_AUTO;
    p[0] = ddsrt_toBE4u (statusinfo & DDSI_STATUSINFO_STANDARDIZED);
    p[1] = ddsrt_toBE4u (statusinfox);
  }
}

void ddsi_xmsg_addpar_sentinel (struct ddsi_xmsg * m)
{
  ddsi_xmsg_addpar (m, DDSI_PID_SENTINEL, 0);
}

void ddsi_xmsg_addpar_sentinel_bo (struct ddsi_xmsg * m, enum ddsrt_byte_order_selector bo)
{
  ddsi_xmsg_addpar_bo (m, DDSI_PID_SENTINEL, 0, bo);
}

int ddsi_xmsg_addpar_sentinel_ifparam (struct ddsi_xmsg * m)
{
  if (m->have_params)
  {
    ddsi_xmsg_addpar_sentinel (m);
    return 1;
  }
  return 0;
}

/* XMSG_CHAIN ----------------------------------------------------------

   Xpacks refer to xmsgs and need to release these after having been
   sent.  For that purpose, we have a chain of xmsgs in an xpack.

   Chain elements are embedded in the xmsg, so instead of loading a
   pointer we compute the address of the xmsg from the address of the
   chain element, &c. */

static void ddsi_xmsg_chain_release (struct ddsi_domaingv *gv, struct ddsi_xmsg_chain *chain)
{
  ddsi_guid_t wrguid;
  memset (&wrguid, 0, sizeof (wrguid));

  while (chain->latest)
  {
    struct ddsi_xmsg_chain_elem *ce = chain->latest;
    struct ddsi_xmsg *m = (struct ddsi_xmsg *) ((char *) ce - offsetof (struct ddsi_xmsg, link));
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
    if (m->kind == DDSI_XMSG_KIND_DATA && m->kindspecific.data.wrguid.prefix.u[0])
    {
      if (wrguid.prefix.u[1] != m->kindspecific.data.wrguid.prefix.u[1] ||
          wrguid.prefix.u[2] != m->kindspecific.data.wrguid.prefix.u[2] ||
          wrguid.entityid.u != m->kindspecific.data.wrguid.entityid.u)
      {
        struct ddsi_writer *wr;
        assert (m->kindspecific.data.wrseq != 0);
        wrguid = m->kindspecific.data.wrguid;
        if ((wr = ddsi_entidx_lookup_writer_guid (gv->entity_index, &m->kindspecific.data.wrguid)) != NULL)
          ddsi_writer_update_seq_xmit (wr, m->kindspecific.data.wrseq);
      }
    }

    ddsi_xmsg_free (m);
  }
}

static void ddsi_xmsg_chain_add (struct ddsi_xmsg_chain *chain, struct ddsi_xmsg *m)
{
  m->link.older = chain->latest;
  chain->latest = &m->link;
}

/* XPACK ---------------------------------------------------------------

   Queued messages are packed into xpacks (all by-ref, using iovecs).
   The xpack is sent to the union of all address sets provided in the
   message added to the xpack.  */

static void ddsi_xpack_reinit (struct ddsi_xpack *xp)
{
  xp->dstmode = NN_XMSG_DST_UNSET;
  xp->niov = 0;
  xp->call_flags = 0;
  xp->msg_len.length = 0;
  xp->includes_rexmit = false;
  xp->included_msgs.latest = NULL;
  xp->maxdelay = DDS_INFINITY;
#ifdef DDS_HAS_SECURITY
  xp->sec_info.use_rtps_encoding = 0;
#endif
#ifdef DDS_HAS_NETWORK_PARTITIONS
  xp->encoderId = 0;
#endif
  xp->packetid++;
}

struct ddsi_xpack * ddsi_xpack_new (struct ddsi_domaingv *gv, bool async_mode)
{
  struct ddsi_xpack *xp;

  xp = ddsrt_malloc (sizeof (*xp));
  memset (xp, 0, sizeof (*xp));
  xp->async_mode = async_mode;
  xp->iov = NULL;
  xp->gv = gv;

  /* Fixed header fields, initialized just once */
  xp->hdr.protocol.id[0] = 'R';
  xp->hdr.protocol.id[1] = 'T';
  xp->hdr.protocol.id[2] = 'P';
  xp->hdr.protocol.id[3] = 'S';
  xp->hdr.version.major = DDSI_RTPS_MAJOR;
  xp->hdr.version.minor = DDSI_RTPS_MINOR;
  xp->hdr.vendorid = DDSI_VENDORID_ECLIPSE;

  /* MSG_LEN first sub message for stream based connections */
  xp->msg_len.smhdr.submessageId = DDSI_RTPS_SMID_ADLINK_MSG_LEN;
  xp->msg_len.smhdr.flags = (DDSRT_ENDIAN == DDSRT_LITTLE_ENDIAN ? DDSI_RTPS_SUBMESSAGE_FLAG_ENDIANNESS : 0);
  xp->msg_len.smhdr.octetsToNextHeader = 4;

  ddsi_xpack_reinit (xp);

  return xp;
}

void ddsi_xpack_free (struct ddsi_xpack *xp)
{
  assert (xp->niov == 0);
  assert (xp->included_msgs.latest == NULL);
  ddsrt_free (xp->iov);
  ddsrt_free (xp);
}

static ssize_t ddsi_xpack_send_rtps(struct ddsi_xpack * xp, const ddsi_xlocator_t *loc)
{
  ssize_t ret = -1;

#ifdef DDS_HAS_SECURITY
  /* Only encode when needed. */
  if (xp->sec_info.use_rtps_encoding)
  {
    ret = ddsi_security_secure_conn_write(
                      xp->gv,
                      loc->conn,
                      &loc->c,
                      xp->niov,
                      xp->iov,
                      xp->call_flags,
                      &(xp->msg_len),
                      (xp->dstmode == NN_XMSG_DST_ONE || xp->dstmode == NN_XMSG_DST_ALL_UC),
                      &(xp->sec_info),
                      ddsi_conn_write);
  }
  else
#endif /* DDS_HAS_SECURITY */
  {
    ret = ddsi_conn_write (loc->conn, &loc->c, xp->niov, xp->iov, xp->call_flags);
  }

  return ret;
}

static ssize_t ddsi_xpack_send1 (const ddsi_xlocator_t *loc, void * varg)
{
  struct ddsi_xpack *xp = varg;
  struct ddsi_domaingv const * const gv = xp->gv;
  ssize_t nbytes = 0;

  if (gv->logconfig.c.mask & DDS_LC_TRACE)
  {
    char buf[DDSI_LOCSTRLEN];
    GVTRACE (" %s", ddsi_xlocator_to_string (buf, sizeof(buf), loc));
  }

  if (gv->config.xmit_lossiness > 0)
  {
    /* We drop APPROXIMATELY a fraction of xmit_lossiness * 10**(-3)
       of all packets to be sent */
    if ((ddsrt_random () % 1000) < (uint32_t) gv->config.xmit_lossiness)
    {
      GVTRACE ("(dropped)");
      xp->call_flags = 0;
      return 0;
    }
  }
#ifdef DDS_HAS_SHM
  // SHM_TODO: We avoid sending packet while data is SHMEM.
  //           I'm not sure whether this is correct or not.
  if (!gv->mute && loc->c.kind != DDSI_LOCATOR_KIND_SHEM)
#else
  if (!gv->mute)
#endif
  {
    nbytes = ddsi_xpack_send_rtps(xp, loc);

#ifndef NDEBUG
    {
      size_t i, len;
      for (i = 0, len = 0; i < xp->niov; i++) {
        len += xp->iov[i].iov_len;
      }
      /* Possible number of bytes written can be larger
       * due to security. */
      assert (nbytes == -1 || (size_t) nbytes >= len);
    }
#endif
  }
  else
  {
    GVTRACE ("(dropped)");
    nbytes = (ssize_t) xp->msg_len.length;
  }

  /* Clear call flags, as used on a per call basis */
  xp->call_flags = 0;

  return nbytes;
}

static void ddsi_xpack_send1v (const ddsi_xlocator_t *loc, void * varg)
{
  (void) ddsi_xpack_send1 (loc, varg);
}

static void ddsi_xpack_send_real (struct ddsi_xpack *xp)
{
  struct ddsi_domaingv const * const gv = xp->gv;
  size_t calls;

  assert (xp->niov <= DDSI_XMSG_MAX_MESSAGE_IOVECS);

  if (xp->niov == 0)
  {
    return;
  }

  assert (xp->dstmode != NN_XMSG_DST_UNSET);

  if (gv->logconfig.c.mask & DDS_LC_TRACE)
  {
    int i;
    GVTRACE ("ddsi_xpack_send %"PRIu32":", xp->msg_len.length);
    for (i = 0; i < (int) xp->niov; i++)
    {
      GVTRACE (" %p:%lu", (void *) xp->iov[i].iov_base, (unsigned long) xp->iov[i].iov_len);
    }
  }

  GVTRACE (" [");
  if (xp->dstmode == NN_XMSG_DST_ONE)
  {
    calls = 1;
    (void) ddsi_xpack_send1 (&xp->dstaddr.loc, xp);
  }
  else
  {
    /* Send to all addresses in as - as ultimately references the writer's
       address set, which is currently replaced rather than changed whenever
       it is updated, but that might not be something we want to guarantee */
    calls = 0;
    if (xp->dstaddr.all.as)
    {
      calls = ddsi_addrset_forall_count (xp->dstaddr.all.as, ddsi_xpack_send1v, xp);
      ddsi_unref_addrset (xp->dstaddr.all.as);
    }
  }
  GVTRACE (" ]\n");
  if (calls)
  {
    GVLOG (DDS_LC_TRAFFIC, "traffic-xmit (%lu) %"PRIu32"\n", (unsigned long) calls, xp->msg_len.length);
  }
  ddsi_xmsg_chain_release (xp->gv, &xp->included_msgs);
  ddsi_xpack_reinit (xp);
}

#define SENDQ_MAX 200
#define SENDQ_HW 10
#define SENDQ_LW 0

static uint32_t ddsi_xpack_sendq_thread (void *vgv)
{
  struct ddsi_domaingv *gv = vgv;
  struct ddsi_thread_state * const thrst = ddsi_lookup_thread_state ();
  ddsi_thread_state_awake_fixed_domain (thrst);
  ddsrt_mutex_lock (&gv->sendq_lock);
  while (!(gv->sendq_stop && gv->sendq_head == NULL))
  {
    struct ddsi_xpack *xp;
    if ((xp = gv->sendq_head) == NULL)
    {
      ddsi_thread_state_asleep (thrst);
      (void) ddsrt_cond_wait (&gv->sendq_cond, &gv->sendq_lock);
      ddsi_thread_state_awake_fixed_domain (thrst);
    }
    else
    {
      gv->sendq_head = xp->sendq_next;
      if (--gv->sendq_length == SENDQ_LW)
        ddsrt_cond_broadcast (&gv->sendq_cond);
      ddsrt_mutex_unlock (&gv->sendq_lock);
      ddsi_xpack_send_real (xp);
      ddsi_xpack_free (xp);
      ddsrt_mutex_lock (&gv->sendq_lock);
    }
  }
  ddsrt_mutex_unlock (&gv->sendq_lock);
  ddsi_thread_state_asleep (thrst);
  return 0;
}

void ddsi_xpack_sendq_init (struct ddsi_domaingv *gv)
{
  gv->sendq_stop = 0;
  gv->sendq_head = NULL;
  gv->sendq_tail = NULL;
  gv->sendq_length = 0;
  ddsrt_mutex_init (&gv->sendq_lock);
  ddsrt_cond_init (&gv->sendq_cond);
}

void ddsi_xpack_sendq_start (struct ddsi_domaingv *gv)
{
  if (ddsi_create_thread (&gv->sendq_ts, gv, "sendq", ddsi_xpack_sendq_thread, gv) != DDS_RETCODE_OK)
    GVERROR ("ddsi_xpack_sendq_start: can't create ddsi_xpack_sendq_thread\n");
  gv->sendq_running = true;
}

void ddsi_xpack_sendq_stop (struct ddsi_domaingv *gv)
{
  ddsrt_mutex_lock (&gv->sendq_lock);
  gv->sendq_stop = 1;
  ddsrt_cond_broadcast (&gv->sendq_cond);
  ddsrt_mutex_unlock (&gv->sendq_lock);
}

void ddsi_xpack_sendq_fini (struct ddsi_domaingv *gv)
{
  ddsi_join_thread (gv->sendq_ts);
  assert (gv->sendq_head == NULL);
  ddsrt_cond_destroy (&gv->sendq_cond);
  ddsrt_mutex_destroy (&gv->sendq_lock);
}

void ddsi_xpack_send (struct ddsi_xpack *xp, bool immediately)
{
  if (!xp->async_mode)
  {
    ddsi_xpack_send_real (xp);
  }
  else
  {
    struct ddsi_domaingv * const gv = xp->gv;
    // copy xp
    struct ddsi_xpack *xp1 = ddsrt_malloc (sizeof (*xp));
    memcpy(xp1, xp, sizeof(*xp1));
    if (xp->iov != NULL) {
      xp1->iov = ddsrt_malloc(xp->niov * sizeof(*xp->iov));
      memcpy(xp1->iov, xp->iov, (xp->niov * sizeof(*xp->iov)));
    }
    ddsi_xpack_reinit (xp);
    xp1->sendq_next = NULL;
    ddsrt_mutex_lock (&gv->sendq_lock);
    if (immediately || gv->sendq_length > SENDQ_LW)
      ddsrt_cond_broadcast (&gv->sendq_cond);
    if (gv->sendq_length >= SENDQ_MAX)
    {
        ddsrt_cond_wait (&gv->sendq_cond, &gv->sendq_lock);
    }
    if (gv->sendq_head)
      gv->sendq_tail->sendq_next = xp1;
    else
    {
      gv->sendq_head = xp1;
    }
    gv->sendq_tail = xp1;
    gv->sendq_length++;
    ddsrt_mutex_unlock (&gv->sendq_lock);
  }
}

static void copy_addressing_info (struct ddsi_xpack *xp, const struct ddsi_xmsg *m)
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
      xp->dstaddr.all.as = ddsi_ref_addrset (m->dstaddr.all.as);
      break;
    case NN_XMSG_DST_ALL_UC:
      xp->dstaddr.all_uc.as = ddsi_ref_addrset (m->dstaddr.all_uc.as);
      break;
  }
}

static int addressing_info_eq_onesidederr (const struct ddsi_xpack *xp, const struct ddsi_xmsg *m)
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
      return ddsi_addrset_eq_onesidederr (xp->dstaddr.all.as, m->dstaddr.all.as);
    case NN_XMSG_DST_ALL_UC:
      return ddsi_addrset_eq_onesidederr (xp->dstaddr.all_uc.as, m->dstaddr.all_uc.as);
  }
  assert (0);
  return 0;
}

static int ddsi_xmsg_is_rexmit (const struct ddsi_xmsg *m)
{
  switch (m->kind)
  {
    case DDSI_XMSG_KIND_DATA:
    case DDSI_XMSG_KIND_CONTROL:
      return 0;
    case DDSI_XMSG_KIND_DATA_REXMIT:
    case DDSI_XMSG_KIND_DATA_REXMIT_NOMERGE:
      return 1;
  }
  return 0;
}

static int ddsi_xpack_mayaddmsg (const struct ddsi_xpack *xp, const struct ddsi_xmsg *m, const uint32_t flags)
{
  const bool rexmit = xp->includes_rexmit || ddsi_xmsg_is_rexmit (m);
  const unsigned max_msg_size = rexmit ? xp->gv->config.max_rexmit_msg_size : xp->gv->config.max_msg_size;
  unsigned payload_size;

  if (xp->niov == 0)
    return 1;
  assert (xp->included_msgs.latest != NULL);
  if (xp->niov + DDSI_XMSG_MAX_SUBMESSAGE_IOVECS > DDSI_XMSG_MAX_MESSAGE_IOVECS)
    return 0;

  payload_size = m->refd_payload ? (unsigned) m->refd_payload_iov.iov_len : 0;

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

#ifdef DDS_HAS_SECURITY
  /* Don't mix up encoded and plain rtps messages */
  if (xp->sec_info.use_rtps_encoding != m->sec_info.use_rtps_encoding)
    return 0;
#endif

  return addressing_info_eq_onesidederr (xp, m);
}

int ddsi_xpack_addmsg (struct ddsi_xpack *xp, struct ddsi_xmsg *m, const uint32_t flags)
{
  /* Returns > 0 if pack got sent out before adding m */
  struct ddsi_domaingv const * const gv = xp->gv;
  static ddsi_rtps_info_dst_t static_zero_dst = {
    { DDSI_RTPS_SMID_INFO_DST, (DDSRT_ENDIAN == DDSRT_LITTLE_ENDIAN ? DDSI_RTPS_SUBMESSAGE_FLAG_ENDIANNESS : 0), sizeof (ddsi_guid_prefix_t) },
    { { 0,0,0,0, 0,0,0,0, 0,0,0,0 } }
  };
  ddsi_rtps_info_dst_t *dst;
  size_t niov;
  size_t sz;
  int result = 0;
  size_t xpo_niov = 0;
  uint32_t xpo_sz = 0;

  assert (m->kind != DDSI_XMSG_KIND_DATA_REXMIT || m->kindspecific.data.readerId_off != 0);

  assert (m->sz > 0);
  assert (m->dstmode != NN_XMSG_DST_UNSET);

  /* Submessage offset must be a multiple of 4 to meet alignment
     requirement (DDSI 2.1, 9.4.1).  If we keep everything 4-byte
     aligned all the time, we don't need to check for padding here. */
  assert ((xp->msg_len.length % 4) == 0);
  assert ((m->sz % 4) == 0);
  assert (m->refd_payload == NULL || (m->refd_payload_iov.iov_len % 4) == 0);

  if (xp->iov == NULL)
    xp->iov = ddsrt_malloc (DDSI_XMSG_MAX_MESSAGE_IOVECS * sizeof (*xp->iov));

  if (!ddsi_xpack_mayaddmsg (xp, m, flags))
  {
    assert (xp->niov > 0);
    ddsi_xpack_send (xp, false);
    assert (ddsi_xpack_mayaddmsg (xp, m, flags));
    result = 1;
  }

  niov = xp->niov;
  sz = xp->msg_len.length;

  /* We try to merge iovecs, but we can never merge across messages
     because of all the headers. So we can speculatively start adding
     the submessage to the pack, and if we can't transmit and restart.
     But do make sure we can't run out of iovecs. */
  assert (niov + DDSI_XMSG_MAX_SUBMESSAGE_IOVECS <= DDSI_XMSG_MAX_MESSAGE_IOVECS);

  GVTRACE ("xpack_addmsg %p %p %"PRIu32"(", (void *) xp, (void *) m, flags);
  switch (m->kind)
  {
    case DDSI_XMSG_KIND_CONTROL:
      GVTRACE ("control");
      break;
    case DDSI_XMSG_KIND_DATA:
    case DDSI_XMSG_KIND_DATA_REXMIT:
    case DDSI_XMSG_KIND_DATA_REXMIT_NOMERGE:
      GVTRACE ("%s("PGUIDFMT":#%"PRId64"/%"PRIu32")",
               (m->kind == DDSI_XMSG_KIND_DATA) ? "data" : "rexmit",
               PGUID (m->kindspecific.data.wrguid),
               m->kindspecific.data.wrseq,
               m->kindspecific.data.wrfragid + 1);
      break;
  }
  GVTRACE ("): niov %d sz %"PRIuSIZE, (int) niov, sz);

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

    if (!gv->m_factory->m_connless)
    {
      xp->iov[niov].iov_base = (void*) &xp->msg_len;
      xp->iov[niov].iov_len = sizeof (xp->msg_len);
      sz += sizeof (xp->msg_len);
      niov++;
    }

#ifdef DDS_HAS_SECURITY
    xp->sec_info = m->sec_info;
#endif
    xp->last_src = &xp->hdr.guid_prefix;
    xp->last_dst = NULL;
  }
  else
  {
    xpo_niov = xp->niov;
    xpo_sz = xp->msg_len.length;
    if (!ddsi_guid_prefix_eq (xp->last_src, &m->data->src.guid_prefix))
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
    dst = (m->dstmode == NN_XMSG_DST_ONE || m->dstmode == NN_XMSG_DST_ALL_UC) ? &m->data->dst : NULL;
  else if (m->dstmode != NN_XMSG_DST_ONE && m->dstmode != NN_XMSG_DST_ALL_UC)
    dst = &static_zero_dst;
  else
    dst = ddsi_guid_prefix_eq (&xp->last_dst->guid_prefix, &m->data->dst.guid_prefix) ? NULL : &m->data->dst;

  if (dst)
  {
    /* Try to merge iovecs, a few large ones should be more efficient
       than many small ones */
    if ((char *) xp->iov[niov-1].iov_base + xp->iov[niov-1].iov_len == (char *) dst)
    {
      xp->iov[niov-1].iov_len += (ddsrt_iov_len_t)sizeof (*dst);
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
    xp->iov[niov-1].iov_len += (ddsrt_iov_len_t)m->sz;
  else
  {
    xp->iov[niov].iov_base = m->data->payload;
    xp->iov[niov].iov_len = (ddsrt_iov_len_t)m->sz;
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
  assert (niov <= DDSI_XMSG_MAX_MESSAGE_IOVECS);

  /* Set total message length in MSG_LEN sub message */
  assert((uint32_t)sz == sz);
  xp->msg_len.length = (uint32_t) sz;
  xp->niov = niov;

  const bool rexmit = xp->includes_rexmit || ddsi_xmsg_is_rexmit (m);
  const uint32_t max_msg_size = rexmit ? xp->gv->config.max_rexmit_msg_size : xp->gv->config.max_msg_size;
  if (xpo_niov > 0 && sz > max_msg_size)
  {
    GVTRACE (" => now niov %d sz %"PRIuSIZE" > max_msg_size %"PRIu32", ddsi_xpack_send niov %d sz %"PRIu32" now\n",
             (int) niov, sz, max_msg_size, (int) xpo_niov, xpo_sz);
    xp->msg_len.length = xpo_sz;
    xp->niov = xpo_niov;
    ddsi_xpack_send (xp, false);
    result = ddsi_xpack_addmsg (xp, m, flags); /* Retry on emptied xp */
  }
  else
  {
    xp->call_flags = flags;
    if (ddsi_xmsg_is_rexmit (m))
      xp->includes_rexmit = true;
    ddsi_xmsg_chain_add (&xp->included_msgs, m);
    GVTRACE (" => now niov %d sz %"PRIuSIZE"\n", (int) niov, sz);
  }

  return result;
}

int64_t ddsi_xpack_maxdelay (const struct ddsi_xpack *xp)
{
  return xp->maxdelay;
}

unsigned ddsi_xpack_packetid (const struct ddsi_xpack *xp)
{
  return xp->packetid;
}

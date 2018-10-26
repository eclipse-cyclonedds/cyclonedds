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
#include <stddef.h>
#include <ctype.h>
#include <assert.h>
#include <string.h>

#include "os/os.h"
#include "ddsi/sysdeps.h"
#include "ddsi/q_md5.h"
#include "ddsi/q_bswap.h"
#include "ddsi/q_config.h"
#include "ddsi/q_freelist.h"
#include <assert.h>
#include <string.h>
#include "os/os.h"
#include "dds__key.h"
#include "dds__tkmap.h"
#include "dds__stream.h"
#include "ddsi/q_radmin.h"
#include "ddsi/ddsi_serdata_default.h"

#define MAX_POOL_SIZE 16384
#define CLEAR_PADDING 0

#ifndef NDEBUG
static int ispowerof2_size (size_t x)
{
  return x > 0 && !(x & (x-1));
}
#endif

static size_t alignup_size (size_t x, size_t a);

struct serstatepool * ddsi_serstatepool_new (void)
{
  struct serstatepool * pool;
  pool = os_malloc (sizeof (*pool));
  nn_freelist_init (&pool->freelist, MAX_POOL_SIZE, offsetof (struct ddsi_serdata_default, next));
  return pool;
}

static void serstate_free_wrap (void *elem)
{
#ifndef NDEBUG
  struct ddsi_serdata_default *d = elem;
  assert(os_atomic_ld32(&d->c.refc) == 1);
#endif
  ddsi_serdata_unref(elem);
}

void ddsi_serstatepool_free (struct serstatepool * pool)
{
  TRACE (("ddsi_serstatepool_free(%p)\n", pool));
  nn_freelist_fini (&pool->freelist, serstate_free_wrap);
  os_free (pool);
}

void ddsi_serstate_append_blob (struct serstate * st, size_t align, size_t sz, const void *data)
{
  char *p = ddsi_serstate_append_aligned (st, sz, align);
  memcpy (p, data, sz);
}

static size_t alignup_size (size_t x, size_t a)
{
  size_t m = a-1;
  assert (ispowerof2_size (a));
  return (x+m) & ~m;
}

void * ddsi_serstate_append (struct serstate * st, size_t n)
{
  char *p;
  if (st->data->pos + n > st->data->size)
  {
    size_t size1 = alignup_size (st->data->pos + n, 128);
    struct ddsi_serdata_default * data1 = os_realloc (st->data, offsetof (struct ddsi_serdata_default, data) + size1);
    st->data = data1;
    st->data->size = (uint32_t)size1;
  }
  assert (st->data->pos + n <= st->data->size);
  p = st->data->data + st->data->pos;
  st->data->pos += (uint32_t)n;
  return p;
}

void * ddsi_serstate_append_aligned (struct serstate * st, size_t n, size_t a)
{
  /* Simply align st->pos, without verifying it fits in the allocated
     buffer: ddsi_serstate_append() is called immediately afterward and will
     grow the buffer as soon as the end of the requested space no
     longer fits. */
#if CLEAR_PADDING
  size_t pos0 = st->pos;
#endif
  char *p;
  assert (ispowerof2_size (a));
  st->data->pos = (uint32_t) alignup_size (st->data->pos, a);
  p = ddsi_serstate_append (st, n);
#if CLEAR_PADDING
  if (p && st->pos > pos0)
    memset (st->data->data + pos0, 0, st->pos - pos0);
#endif
  return p;
}

/* Fixed seed and length */

#define DDS_MH3_LEN 16
#define DDS_MH3_SEED 0

#define DDS_MH3_ROTL32(x,r) (((x) << (r)) | ((x) >> (32 - (r))))

/* Really
 http://code.google.com/p/smhasher/source/browse/trunk/MurmurHash3.cpp,
 MurmurHash3_x86_32
 */

static uint32_t dds_mh3 (const void * key)
{
  const uint8_t *data = (const uint8_t *) key;
  const intptr_t nblocks = (intptr_t) (DDS_MH3_LEN / 4);
  const uint32_t c1 = 0xcc9e2d51;
  const uint32_t c2 = 0x1b873593;

  uint32_t h1 = DDS_MH3_SEED;

  const uint32_t *blocks = (const uint32_t *) (data + nblocks * 4);
  register intptr_t i;

  for (i = -nblocks; i; i++)
  {
    uint32_t k1 = blocks[i];

    k1 *= c1;
    k1 = DDS_MH3_ROTL32 (k1, 15);
    k1 *= c2;

    h1 ^= k1;
    h1 = DDS_MH3_ROTL32 (h1, 13);
    h1 = h1 * 5+0xe6546b64;
  }

  /* finalization */

  h1 ^= DDS_MH3_LEN;
  h1 ^= h1 >> 16;
  h1 *= 0x85ebca6b;
  h1 ^= h1 >> 13;
  h1 *= 0xc2b2ae35;
  h1 ^= h1 >> 16;

  return h1;
}

static struct ddsi_serdata *fix_serdata_default(struct ddsi_serdata_default *d, uint64_t tp_iid)
{
  if (d->keyhash.m_flags & DDS_KEY_IS_HASH)
    d->c.hash = dds_mh3 (d->keyhash.m_hash) ^ (uint32_t)tp_iid;
  else
    d->c.hash = *((uint32_t *)d->keyhash.m_hash) ^ (uint32_t)tp_iid;
  return &d->c;
}

static uint32_t serdata_default_get_size(const struct ddsi_serdata *dcmn)
{
  const struct ddsi_serdata_default *d = (const struct ddsi_serdata_default *) dcmn;
  return d->pos + (uint32_t)sizeof (struct CDRHeader);
}

static bool serdata_default_eqkey(const struct ddsi_serdata *acmn, const struct ddsi_serdata *bcmn)
{
  const struct ddsi_serdata_default *a = (const struct ddsi_serdata_default *)acmn;
  const struct ddsi_serdata_default *b = (const struct ddsi_serdata_default *)bcmn;
  const struct ddsi_sertopic_default *tp;

  assert(a->c.ops == b->c.ops);
  tp = (struct ddsi_sertopic_default *)a->c.topic;
  if (tp->nkeys == 0)
    return true;
  else
  {
    assert (a->keyhash.m_flags & DDS_KEY_HASH_SET);
    return memcmp (a->keyhash.m_hash, b->keyhash.m_hash, 16) == 0;
  }
}

static void serdata_default_free(struct ddsi_serdata *dcmn)
{
  struct ddsi_serdata_default *d = (struct ddsi_serdata_default *)dcmn;
  dds_free (d->keyhash.m_key_buff);
  dds_free (d);
}

static void serdata_default_init(struct ddsi_serdata_default *d, const struct ddsi_sertopic_default *tp, enum ddsi_serdata_kind kind)
{
  ddsi_serdata_init (&d->c, &tp->c, kind);
  d->pos = 0;
#ifndef NDEBUG
  d->fixed = false;
#endif
  d->hdr.identifier = tp->native_encoding_identifier;
  d->hdr.options = 0;
  d->bswap = false;
  memset (d->keyhash.m_hash, 0, sizeof (d->keyhash.m_hash));
  d->keyhash.m_key_len = 0;
  d->keyhash.m_flags = 0;
  d->keyhash.m_key_buff = NULL;
  d->keyhash.m_key_buff_size = 0;
}

static struct ddsi_serdata_default *serdata_default_allocnew(struct serstatepool *pool)
{
  const uint32_t init_size = 128;
  struct ddsi_serdata_default *d = os_malloc(offsetof (struct ddsi_serdata_default, data) + init_size);
  d->size = init_size;
  d->pool = pool;
  return d;
}

static struct ddsi_serdata_default *serdata_default_new(const struct ddsi_sertopic_default *tp, enum ddsi_serdata_kind kind)
{
  struct ddsi_serdata_default *d;
  if ((d = nn_freelist_pop (&gv.serpool->freelist)) == NULL)
    d = serdata_default_allocnew(gv.serpool);
  serdata_default_init(d, tp, kind);
  return d;
}

/* Construct a serdata from a fragchain received over the network */
static struct ddsi_serdata *serdata_default_from_ser (const struct ddsi_sertopic *tpcmn, enum ddsi_serdata_kind kind, const struct nn_rdata *fragchain, size_t size)
{
  const struct ddsi_sertopic_default *tp = (const struct ddsi_sertopic_default *)tpcmn;
  struct ddsi_serdata_default *d = serdata_default_new(tp, kind);
  uint32_t off = 4; /* must skip the CDR header */
  struct serstate st = { .data = d };

  assert (fragchain->min == 0);
  assert (fragchain->maxp1 >= off); /* CDR header must be in first fragment */
  (void)size;

  memcpy (&d->hdr, NN_RMSG_PAYLOADOFF (fragchain->rmsg, NN_RDATA_PAYLOAD_OFF (fragchain)), sizeof (d->hdr));
  switch (d->hdr.identifier) {
    case CDR_LE:
    case PL_CDR_LE:
      d->bswap = ! PLATFORM_IS_LITTLE_ENDIAN;
      break;
    case CDR_BE:
    case PL_CDR_BE:
      d->bswap = PLATFORM_IS_LITTLE_ENDIAN;
      break;
    default:
      /* must not ever try to use a serdata format for an unsupported encoding */
      abort ();
  }

  while (fragchain)
  {
    assert (fragchain->min <= off);
    assert (fragchain->maxp1 <= size);
    if (fragchain->maxp1 > off)
    {
      /* only copy if this fragment adds data */
      const unsigned char *payload = NN_RMSG_PAYLOADOFF (fragchain->rmsg, NN_RDATA_PAYLOAD_OFF (fragchain));
      ddsi_serstate_append_blob (&st, 1, fragchain->maxp1 - off, payload + off - fragchain->min);
      off = fragchain->maxp1;
    }
    fragchain = fragchain->nextfrag;
  }

  /* FIXME: assignment here is because of reallocs, but doing it this way is a bit hacky */
  d = st.data;

  dds_stream_t is;
  dds_stream_from_serdata_default (&is, d);
  dds_stream_read_keyhash (&is, &d->keyhash, (const dds_topic_descriptor_t *)tp->type, kind == SDK_KEY);
  return fix_serdata_default (d, tp->c.iid);
}

struct ddsi_serdata *ddsi_serdata_from_keyhash_cdr (const struct ddsi_sertopic *tpcmn, const nn_keyhash_t *keyhash)
{
  const struct ddsi_sertopic_default *tp = (const struct ddsi_sertopic_default *)tpcmn;
  struct ddsi_serdata_default *d = serdata_default_new(tp, SDK_KEY);
  struct serstate st = { .data = d };
  /* FIXME: not quite sure this is correct */
  ddsi_serstate_append_blob (&st, 1, sizeof (keyhash->value), keyhash->value);
  /* FIXME: assignment here is because of reallocs, but doing it this way is a bit hacky */
  d = st.data;
  return fix_serdata_default(d, tp->c.iid);
}

static struct ddsi_serdata *serdata_default_from_sample_cdr (const struct ddsi_sertopic *tpcmn, enum ddsi_serdata_kind kind, const void *sample)
{
  const struct ddsi_sertopic_default *tp = (const struct ddsi_sertopic_default *)tpcmn;
  struct ddsi_serdata_default *d = serdata_default_new(tp, kind);
  dds_stream_t os;
  dds_key_gen ((const dds_topic_descriptor_t *)tp->type, &d->keyhash, (char*)sample);
  dds_stream_from_serdata_default (&os, d);
  switch (kind)
  {
    case SDK_EMPTY:
      break;
    case SDK_KEY:
      dds_stream_write_key (&os, sample, tp);
      break;
    case SDK_DATA:
      dds_stream_write_sample (&os, sample, tp);
      break;
  }
  dds_stream_add_to_serdata_default (&os, &d);
  return fix_serdata_default (d, tp->c.iid);
}

static struct ddsi_serdata *serdata_default_from_sample_plist (const struct ddsi_sertopic *tpcmn, enum ddsi_serdata_kind kind, const void *vsample)
{
  /* Currently restricted to DDSI discovery data (XTypes will need a rethink of the default representation and that may result in discovery data being moved to that new representation), and that means: keys are either GUIDs or an unbounded string for topics, for which MD5 is acceptable. Furthermore, these things don't get written very often, so scanning the parameter list to get the key value out is good enough for now. And at least it keeps the DDSI discovery data writing out of the internals of the sample representation */
  const struct ddsi_sertopic_default *tp = (const struct ddsi_sertopic_default *)tpcmn;
  const struct ddsi_plist_sample *sample = vsample;
  struct ddsi_serdata_default *d = serdata_default_new(tp, kind);
  struct serstate st = { .data = d };
  ddsi_serstate_append_blob (&st, 1, sample->size, sample->blob);
  d = st.data;
  const unsigned char *rawkey = nn_plist_findparam_native_unchecked (sample->blob, sample->keyparam);
#ifndef NDEBUG
  size_t keysize;
#endif
  switch (sample->keyparam)
  {
    case PID_PARTICIPANT_GUID:
    case PID_ENDPOINT_GUID:
    case PID_GROUP_GUID:
      d->keyhash.m_flags = DDS_KEY_SET | DDS_KEY_HASH_SET | DDS_KEY_IS_HASH;
      d->keyhash.m_key_len = 16;
      memcpy (&d->keyhash.m_hash, rawkey, d->keyhash.m_key_len);
#ifndef NDEBUG
      keysize = d->keyhash.m_key_len;
#endif
      break;

    case PID_TOPIC_NAME: {
      const char *topic_name = (const char *) (rawkey + sizeof(uint32_t));
      uint32_t topic_name_sz;
      uint32_t topic_name_sz_BE;
      md5_state_t md5st;
      md5_byte_t digest[16];
      topic_name_sz = (uint32_t) strlen (topic_name) + 1;
      d->keyhash.m_flags = DDS_KEY_SET | DDS_KEY_HASH_SET;
      d->keyhash.m_key_len = 16;
      md5_init (&md5st);
      md5_append (&md5st, (const md5_byte_t *) &topic_name_sz_BE, sizeof (topic_name_sz_BE));
      md5_append (&md5st, (const md5_byte_t *) topic_name, topic_name_sz);
      md5_finish (&md5st, digest);
      memcpy (&d->keyhash.m_hash, digest, d->keyhash.m_key_len);
#ifndef NDEBUG
      keysize = sizeof (uint32_t) + topic_name_sz;
#endif
      break;
    }

    default:
      abort();
  }

  /* if we're it is supposed to be just a key, rawkey must be be the first field and followed only by a sentinel */
  assert (kind != SDK_KEY || rawkey == (const unsigned char *)sample->blob + sizeof (nn_parameter_t));
  assert (kind != SDK_KEY || sample->size == sizeof (nn_parameter_t) + alignup_size (keysize, 4) + sizeof (nn_parameter_t));
  return fix_serdata_default (d, tp->c.iid);
}

static struct ddsi_serdata *serdata_default_from_sample_rawcdr (const struct ddsi_sertopic *tpcmn, enum ddsi_serdata_kind kind, const void *vsample)
{
  /* Currently restricted to DDSI discovery data (XTypes will need a rethink of the default representation and that may result in discovery data being moved to that new representation), and that means: keys are either GUIDs or an unbounded string for topics, for which MD5 is acceptable. Furthermore, these things don't get written very often, so scanning the parameter list to get the key value out is good enough for now. And at least it keeps the DDSI discovery data writing out of the internals of the sample representation */
  const struct ddsi_sertopic_default *tp = (const struct ddsi_sertopic_default *)tpcmn;
  const struct ddsi_rawcdr_sample *sample = vsample;
  struct ddsi_serdata_default *d = serdata_default_new(tp, kind);
  struct serstate st = { .data = d };
  assert (sample->keysize <= 16);
  ddsi_serstate_append_blob (&st, 1, sample->size, sample->blob);
  d = st.data;
  d->keyhash.m_flags = DDS_KEY_SET | DDS_KEY_HASH_SET | DDS_KEY_IS_HASH;
  d->keyhash.m_key_len = (uint32_t) sample->keysize;
  if (sample->keysize > 0)
    memcpy (&d->keyhash.m_hash, sample->key, sample->keysize);
  return fix_serdata_default (d, tp->c.iid);
}

/* Fill buffer with 'size' bytes of serialised data, starting from 'off'; 0 <= off < off+sz <= alignup4(size(d)) */
static void serdata_default_to_ser (const struct ddsi_serdata *serdata_common, size_t off, size_t sz, void *buf)
{
  const struct ddsi_serdata_default *d = (const struct ddsi_serdata_default *)serdata_common;
  assert (off < d->pos + sizeof(struct CDRHeader));
  assert (sz <= alignup_size (d->pos + sizeof(struct CDRHeader), 4) - off);
  /* FIXME: maybe I should pull the header out ... */
  memcpy (buf, (char *)&d->hdr + off, sz);
}

static struct ddsi_serdata *serdata_default_to_ser_ref (const struct ddsi_serdata *serdata_common, size_t off, size_t sz, ddsi_iovec_t *ref)
{
  const struct ddsi_serdata_default *d = (const struct ddsi_serdata_default *)serdata_common;
  assert (off < d->pos + sizeof(struct CDRHeader));
  assert (sz <= alignup_size (d->pos + sizeof(struct CDRHeader), 4) - off);
  ref->iov_base = (char *)&d->hdr + off;
  ref->iov_len = sz;
  return ddsi_serdata_ref(serdata_common);
}

static void serdata_default_to_ser_unref (struct ddsi_serdata *serdata_common, const ddsi_iovec_t *ref)
{
  (void)ref;
  ddsi_serdata_unref(serdata_common);
}

static bool serdata_default_to_sample_cdr (const struct ddsi_serdata *serdata_common, void *sample, void **bufptr, void *buflim)
{
  const struct ddsi_serdata_default *d = (const struct ddsi_serdata_default *)serdata_common;
  dds_stream_t is;
  if (bufptr) abort(); else { (void)buflim; } /* FIXME: haven't implemented that bit yet! */
  dds_stream_from_serdata_default(&is, d);
  if (d->c.kind == SDK_KEY)
    dds_stream_read_key (&is, sample, (const dds_topic_descriptor_t*) ((struct ddsi_sertopic_default *)d->c.topic)->type);
  else
    dds_stream_read_sample (&is, sample, (const struct ddsi_sertopic_default *)d->c.topic);
  return true; /* FIXME: can't conversion to sample fail? */
}

static bool serdata_default_to_sample_plist (const struct ddsi_serdata *serdata_common, void *vsample, void **bufptr, void *buflim)
{
#if 0
  const struct ddsi_serdata_default *d = (const struct ddsi_serdata_default *)serdata_common;
  struct ddsi_plist_sample *sample = vsample;
  /* output of to_sample for normal samples is a copy, and so it should be for this one; only for native format (like the inverse) */
  if (bufptr) abort(); else { (void)buflim; } /* FIXME: haven't implemented that bit yet! */
  assert (d->hdr.identifier == PLATFORM_IS_LITTLE_ENDIAN ? PL_CDR_LE : PL_CDR_BE);
  sample->size = d->pos;
  sample->blob = os_malloc (sample->size);
  memcpy (sample->blob, (char *)&d->hdr + sizeof(struct CDRHeader), sample->size);
  sample->keyparam = PID_PAD;
  return true;
#else
  /* I don't think I need this */
  (void)serdata_common; (void)vsample; (void)bufptr; (void)buflim;
  abort();
  return false;
#endif
}

static bool serdata_default_to_sample_rawcdr (const struct ddsi_serdata *serdata_common, void *vsample, void **bufptr, void *buflim)
{
  /* I don't think I need this */
  (void)serdata_common; (void)vsample; (void)bufptr; (void)buflim;
  abort();
  return false;
}

const struct ddsi_serdata_ops ddsi_serdata_ops_cdr = {
  .get_size = serdata_default_get_size,
  .cmpkey = 0,
  .eqkey = serdata_default_eqkey,
  .free = serdata_default_free,
  .from_ser = serdata_default_from_ser,
  .from_keyhash = ddsi_serdata_from_keyhash_cdr,
  .from_sample = serdata_default_from_sample_cdr,
  .to_ser = serdata_default_to_ser,
  .to_sample = serdata_default_to_sample_cdr,
  .to_ser_ref = serdata_default_to_ser_ref,
  .to_ser_unref = serdata_default_to_ser_unref
};

const struct ddsi_serdata_ops ddsi_serdata_ops_plist = {
  .get_size = serdata_default_get_size,
  .cmpkey = 0,
  .eqkey = serdata_default_eqkey,
  .free = serdata_default_free,
  .from_ser = serdata_default_from_ser,
  .from_keyhash = 0, /* q_ddsi_discovery.c takes care of it internally */
  .from_sample = serdata_default_from_sample_plist,
  .to_ser = serdata_default_to_ser,
  .to_sample = serdata_default_to_sample_plist,
  .to_ser_ref = serdata_default_to_ser_ref,
  .to_ser_unref = serdata_default_to_ser_unref
};

const struct ddsi_serdata_ops ddsi_serdata_ops_rawcdr = {
  .get_size = serdata_default_get_size,
  .cmpkey = 0,
  .eqkey = serdata_default_eqkey,
  .free = serdata_default_free,
  .from_ser = serdata_default_from_ser,
  .from_keyhash = 0, /* q_ddsi_discovery.c takes care of it internally */
  .from_sample = serdata_default_from_sample_rawcdr,
  .to_ser = serdata_default_to_ser,
  .to_sample = serdata_default_to_sample_rawcdr,
  .to_ser_ref = serdata_default_to_ser_ref,
  .to_ser_unref = serdata_default_to_ser_unref
};

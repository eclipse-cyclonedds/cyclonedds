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
#include "ddsi/q_md5.h"
#include "ddsi/q_bswap.h"
#include "ddsi/q_config.h"
#include "ddsi/q_freelist.h"
#include <assert.h>
#include <string.h>
#include "os/os.h"
#include "dds__key.h"
#include "ddsi/ddsi_tkmap.h"
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

struct serdatapool * ddsi_serdatapool_new (void)
{
  struct serdatapool * pool;
  pool = os_malloc (sizeof (*pool));
  nn_freelist_init (&pool->freelist, MAX_POOL_SIZE, offsetof (struct ddsi_serdata_default, next));
  return pool;
}

static void serdata_free_wrap (void *elem)
{
#ifndef NDEBUG
  struct ddsi_serdata_default *d = elem;
  assert(os_atomic_ld32(&d->c.refc) == 0);
#endif
  dds_free(elem);
}

void ddsi_serdatapool_free (struct serdatapool * pool)
{
  DDS_TRACE("ddsi_serdatapool_free(%p)\n", (void *) pool);
  nn_freelist_fini (&pool->freelist, serdata_free_wrap);
  os_free (pool);
}

static size_t alignup_size (size_t x, size_t a)
{
  size_t m = a-1;
  assert (ispowerof2_size (a));
  return (x+m) & ~m;
}

static void *serdata_default_append (struct ddsi_serdata_default **d, size_t n)
{
  char *p;
  if ((*d)->pos + n > (*d)->size)
  {
    size_t size1 = alignup_size ((*d)->pos + n, 128);
    *d = os_realloc (*d, offsetof (struct ddsi_serdata_default, data) + size1);
    (*d)->size = (uint32_t)size1;
  }
  assert ((*d)->pos + n <= (*d)->size);
  p = (*d)->data + (*d)->pos;
  (*d)->pos += (uint32_t)n;
  return p;
}

static void *serdata_default_append_aligned (struct ddsi_serdata_default **d, size_t n, size_t a)
{
#if CLEAR_PADDING
  size_t pos0 = st->pos;
#endif
  char *p;
  assert (ispowerof2_size (a));
  (*d)->pos = (uint32_t) alignup_size ((*d)->pos, a);
  p = serdata_default_append (d, n);
#if CLEAR_PADDING
  if (p && (*d)->pos > pos0)
    memset ((*d)->data + pos0, 0, (*d)->pos - pos0);
#endif
  return p;
}

static void serdata_default_append_blob (struct ddsi_serdata_default **d, size_t align, size_t sz, const void *data)
{
  char *p = serdata_default_append_aligned (d, sz, align);
  memcpy (p, data, sz);
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

static struct ddsi_serdata *fix_serdata_default(struct ddsi_serdata_default *d, uint32_t basehash)
{
  if (d->keyhash.m_iskey)
    d->c.hash = dds_mh3 (d->keyhash.m_hash) ^ basehash;
  else
    d->c.hash = *((uint32_t *)d->keyhash.m_hash) ^ basehash;
  return &d->c;
}

static struct ddsi_serdata *fix_serdata_default_nokey(struct ddsi_serdata_default *d, uint32_t basehash)
{
  d->c.hash = basehash;
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
  assert (a->keyhash.m_set);
#if 0
  char astr[50], bstr[50];
  for (int i = 0; i < 16; i++) {
    sprintf (astr + 3*i, ":%02x", (unsigned char)a->keyhash.m_hash[i]);
  }
  for (int i = 0; i < 16; i++) {
    sprintf (bstr + 3*i, ":%02x", (unsigned char)b->keyhash.m_hash[i]);
  }
  printf("serdata_default_eqkey: %s %s\n", astr+1, bstr+1);
#endif
  return memcmp (a->keyhash.m_hash, b->keyhash.m_hash, 16) == 0;
}

static bool serdata_default_eqkey_nokey (const struct ddsi_serdata *acmn, const struct ddsi_serdata *bcmn)
{
  (void)acmn; (void)bcmn;
  return true;
}

static void serdata_default_free(struct ddsi_serdata *dcmn)
{
  struct ddsi_serdata_default *d = (struct ddsi_serdata_default *)dcmn;
  assert(os_atomic_ld32(&d->c.refc) == 0);
  if (!nn_freelist_push (&gv.serpool->freelist, d))
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
  memset (d->keyhash.m_hash, 0, sizeof (d->keyhash.m_hash));
  d->keyhash.m_set = 0;
  d->keyhash.m_iskey = 0;
}

static struct ddsi_serdata_default *serdata_default_allocnew(struct serdatapool *pool)
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
  else
    os_atomic_st32(&d->c.refc, 1);
  serdata_default_init(d, tp, kind);
  return d;
}

/* Construct a serdata from a fragchain received over the network */
static struct ddsi_serdata_default *serdata_default_from_ser_common (const struct ddsi_sertopic *tpcmn, enum ddsi_serdata_kind kind, const struct nn_rdata *fragchain, size_t size)
{
  const struct ddsi_sertopic_default *tp = (const struct ddsi_sertopic_default *)tpcmn;
  struct ddsi_serdata_default *d = serdata_default_new(tp, kind);
  uint32_t off = 4; /* must skip the CDR header */

  assert (fragchain->min == 0);
  assert (fragchain->maxp1 >= off); /* CDR header must be in first fragment */
  (void)size;

  memcpy (&d->hdr, NN_RMSG_PAYLOADOFF (fragchain->rmsg, NN_RDATA_PAYLOAD_OFF (fragchain)), sizeof (d->hdr));
  assert (d->hdr.identifier == CDR_LE || d->hdr.identifier == CDR_BE);

  while (fragchain)
  {
    assert (fragchain->min <= off);
    assert (fragchain->maxp1 <= size);
    if (fragchain->maxp1 > off)
    {
      /* only copy if this fragment adds data */
      const unsigned char *payload = NN_RMSG_PAYLOADOFF (fragchain->rmsg, NN_RDATA_PAYLOAD_OFF (fragchain));
      serdata_default_append_blob (&d, 1, fragchain->maxp1 - off, payload + off - fragchain->min);
      off = fragchain->maxp1;
    }
    fragchain = fragchain->nextfrag;
  }

  dds_stream_t is;
  dds_stream_from_serdata_default (&is, d);
  dds_stream_read_keyhash (&is, &d->keyhash, (const dds_topic_descriptor_t *)tp->type, kind == SDK_KEY);
  return d;
}

static struct ddsi_serdata *serdata_default_from_ser (const struct ddsi_sertopic *tpcmn, enum ddsi_serdata_kind kind, const struct nn_rdata *fragchain, size_t size)
{
  return fix_serdata_default (serdata_default_from_ser_common (tpcmn, kind, fragchain, size), tpcmn->serdata_basehash);
}

static struct ddsi_serdata *serdata_default_from_ser_nokey (const struct ddsi_sertopic *tpcmn, enum ddsi_serdata_kind kind, const struct nn_rdata *fragchain, size_t size)
{
  return fix_serdata_default_nokey (serdata_default_from_ser_common (tpcmn, kind, fragchain, size), tpcmn->serdata_basehash);
}

struct ddsi_serdata *ddsi_serdata_from_keyhash_cdr (const struct ddsi_sertopic *tpcmn, const nn_keyhash_t *keyhash)
{
  /* FIXME: not quite sure this is correct, though a check against a specially hacked OpenSplice suggests it is */
  const struct ddsi_sertopic_default *tp = (const struct ddsi_sertopic_default *)tpcmn;
  if (!(tp->type->m_flagset & DDS_TOPIC_FIXED_KEY))
  {
    /* keyhash is MD5 of a key value, so impossible to turn into a key value */
    return NULL;
  }
  else
  {
    struct ddsi_serdata_default *d = serdata_default_new(tp, SDK_KEY);
    d->hdr.identifier = CDR_BE;
    serdata_default_append_blob (&d, 1, sizeof (keyhash->value), keyhash->value);
    memcpy (d->keyhash.m_hash, keyhash->value, sizeof (d->keyhash.m_hash));
    d->keyhash.m_set = 1;
    d->keyhash.m_iskey = 1;
    return fix_serdata_default(d, tp->c.serdata_basehash);
  }
}

struct ddsi_serdata *ddsi_serdata_from_keyhash_cdr_nokey (const struct ddsi_sertopic *tpcmn, const nn_keyhash_t *keyhash)
{
  const struct ddsi_sertopic_default *tp = (const struct ddsi_sertopic_default *)tpcmn;
  struct ddsi_serdata_default *d = serdata_default_new(tp, SDK_KEY);
  (void)keyhash;
  d->keyhash.m_set = 1;
  d->keyhash.m_iskey = 1;
  return fix_serdata_default_nokey(d, tp->c.serdata_basehash);
}

static struct ddsi_serdata_default *serdata_default_from_sample_cdr_common (const struct ddsi_sertopic *tpcmn, enum ddsi_serdata_kind kind, const void *sample)
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
  return d;
}

static struct ddsi_serdata *serdata_default_from_sample_cdr (const struct ddsi_sertopic *tpcmn, enum ddsi_serdata_kind kind, const void *sample)
{
  return fix_serdata_default (serdata_default_from_sample_cdr_common (tpcmn, kind, sample), tpcmn->serdata_basehash);
}

static struct ddsi_serdata *serdata_default_from_sample_cdr_nokey (const struct ddsi_sertopic *tpcmn, enum ddsi_serdata_kind kind, const void *sample)
{
  return fix_serdata_default_nokey (serdata_default_from_sample_cdr_common (tpcmn, kind, sample), tpcmn->serdata_basehash);
}

static struct ddsi_serdata *serdata_default_from_sample_plist (const struct ddsi_sertopic *tpcmn, enum ddsi_serdata_kind kind, const void *vsample)
{
  /* Currently restricted to DDSI discovery data (XTypes will need a rethink of the default representation and that may result in discovery data being moved to that new representation), and that means: keys are either GUIDs or an unbounded string for topics, for which MD5 is acceptable. Furthermore, these things don't get written very often, so scanning the parameter list to get the key value out is good enough for now. And at least it keeps the DDSI discovery data writing out of the internals of the sample representation */
  const struct ddsi_sertopic_default *tp = (const struct ddsi_sertopic_default *)tpcmn;
  const struct ddsi_plist_sample *sample = vsample;
  struct ddsi_serdata_default *d = serdata_default_new(tp, kind);
  serdata_default_append_blob (&d, 1, sample->size, sample->blob);
  const unsigned char *rawkey = nn_plist_findparam_native_unchecked (sample->blob, sample->keyparam);
#ifndef NDEBUG
  size_t keysize;
#endif
  switch (sample->keyparam)
  {
    case PID_PARTICIPANT_GUID:
    case PID_ENDPOINT_GUID:
    case PID_GROUP_GUID:
      d->keyhash.m_set = 1;
      d->keyhash.m_iskey = 1;
      memcpy (d->keyhash.m_hash, rawkey, 16);
#ifndef NDEBUG
      keysize = 16;
#endif
      break;

    case PID_TOPIC_NAME: {
      const char *topic_name = (const char *) (rawkey + sizeof(uint32_t));
      uint32_t topic_name_sz;
      uint32_t topic_name_sz_BE;
      md5_state_t md5st;
      md5_byte_t digest[16];
      topic_name_sz = (uint32_t) strlen (topic_name) + 1;
      topic_name_sz_BE = toBE4u (topic_name_sz);
      d->keyhash.m_set = 1;
      d->keyhash.m_iskey = 0;
      md5_init (&md5st);
      md5_append (&md5st, (const md5_byte_t *) &topic_name_sz_BE, sizeof (topic_name_sz_BE));
      md5_append (&md5st, (const md5_byte_t *) topic_name, topic_name_sz);
      md5_finish (&md5st, digest);
      memcpy (d->keyhash.m_hash, digest, 16);
#ifndef NDEBUG
      keysize = sizeof (uint32_t) + topic_name_sz;
#endif
      break;
    }

    default:
      abort();
  }

  /* if it is supposed to be just a key, rawkey must be be the first field and followed only by a sentinel */
  assert (kind != SDK_KEY || rawkey == (const unsigned char *)sample->blob + sizeof (nn_parameter_t));
  assert (kind != SDK_KEY || sample->size == sizeof (nn_parameter_t) + alignup_size (keysize, 4) + sizeof (nn_parameter_t));
  return fix_serdata_default (d, tp->c.serdata_basehash);
}

static struct ddsi_serdata *serdata_default_from_sample_rawcdr (const struct ddsi_sertopic *tpcmn, enum ddsi_serdata_kind kind, const void *vsample)
{
  /* Currently restricted to DDSI discovery data (XTypes will need a rethink of the default representation and that may result in discovery data being moved to that new representation), and that means: keys are either GUIDs or an unbounded string for topics, for which MD5 is acceptable. Furthermore, these things don't get written very often, so scanning the parameter list to get the key value out is good enough for now. And at least it keeps the DDSI discovery data writing out of the internals of the sample representation */
  const struct ddsi_sertopic_default *tp = (const struct ddsi_sertopic_default *)tpcmn;
  const struct ddsi_rawcdr_sample *sample = vsample;
  struct ddsi_serdata_default *d = serdata_default_new(tp, kind);
  assert (sample->keysize <= 16);
  serdata_default_append_blob (&d, 1, sample->size, sample->blob);
  d->keyhash.m_set = 1;
  d->keyhash.m_iskey = 1;
  if (sample->keysize == 0)
    return fix_serdata_default_nokey (d, tp->c.serdata_basehash);
  else
  {
    memcpy (&d->keyhash.m_hash, sample->key, sample->keysize);
    return fix_serdata_default (d, tp->c.serdata_basehash);
  }
}

static struct ddsi_serdata *serdata_default_to_topicless (const struct ddsi_serdata *serdata_common)
{
  const struct ddsi_serdata_default *d = (const struct ddsi_serdata_default *)serdata_common;
  const struct ddsi_sertopic_default *tp = (const struct ddsi_sertopic_default *)d->c.topic;
  struct ddsi_serdata_default *d_tl = serdata_default_new(tp, SDK_KEY);
  d_tl->c.topic = NULL;
  d_tl->c.hash = d->c.hash;
  d_tl->c.timestamp.v = INT64_MIN;
  d_tl->keyhash = d->keyhash;
  /* These things are used for the key-to-instance map and only subject to eq, free and conversion to an invalid
     sample of some topic for topics that can end up in a RHC, so, of the four kinds we have, only for CDR-with-key
     the payload is of interest. */
  if (d->c.ops == &ddsi_serdata_ops_cdr)
  {
    if (d->c.kind == SDK_KEY)
    {
      d_tl->hdr.identifier = d->hdr.identifier;
      serdata_default_append_blob (&d_tl, 1, d->pos, d->data);
    }
    else if (d->keyhash.m_iskey)
    {
      d_tl->hdr.identifier = CDR_BE;
      serdata_default_append_blob (&d_tl, 1, sizeof (d->keyhash.m_hash), d->keyhash.m_hash);
    }
    else
    {
      const struct dds_topic_descriptor *desc = tp->type;
      dds_stream_t is, os;
      uint32_t nbytes;
      dds_stream_from_serdata_default (&is, d);
      dds_stream_from_serdata_default (&os, d_tl);
      nbytes = dds_stream_extract_key (&is, &os, desc->m_ops, false);
      os.m_index += nbytes;
      if (os.m_index < os.m_size)
      {
        os.m_buffer.p8 = dds_realloc (os.m_buffer.p8, os.m_index);
        os.m_size = os.m_index;
      }
      dds_stream_add_to_serdata_default (&os, &d_tl);
    }
  }
  return (struct ddsi_serdata *)d_tl;
}

/* Fill buffer with 'size' bytes of serialised data, starting from 'off'; 0 <= off < off+sz <= alignup4(size(d)) */
static void serdata_default_to_ser (const struct ddsi_serdata *serdata_common, size_t off, size_t sz, void *buf)
{
  const struct ddsi_serdata_default *d = (const struct ddsi_serdata_default *)serdata_common;
  assert (off < d->pos + sizeof(struct CDRHeader));
  assert (sz <= alignup_size (d->pos + sizeof(struct CDRHeader), 4) - off);
  memcpy (buf, (char *)&d->hdr + off, sz);
}

static struct ddsi_serdata *serdata_default_to_ser_ref (const struct ddsi_serdata *serdata_common, size_t off, size_t sz, os_iovec_t *ref)
{
  const struct ddsi_serdata_default *d = (const struct ddsi_serdata_default *)serdata_common;
  assert (off < d->pos + sizeof(struct CDRHeader));
  assert (sz <= alignup_size (d->pos + sizeof(struct CDRHeader), 4) - off);
  ref->iov_base = (char *)&d->hdr + off;
  ref->iov_len = (os_iov_len_t)sz;
  return ddsi_serdata_ref(serdata_common);
}

static void serdata_default_to_ser_unref (struct ddsi_serdata *serdata_common, const os_iovec_t *ref)
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

static bool serdata_default_topicless_to_sample_cdr (const struct ddsi_sertopic *topic, const struct ddsi_serdata *serdata_common, void *sample, void **bufptr, void *buflim)
{
  const struct ddsi_serdata_default *d = (const struct ddsi_serdata_default *)serdata_common;
  dds_stream_t is;
  assert (d->c.topic == NULL);
  assert (d->c.kind == SDK_KEY);
  assert (d->c.ops == topic->serdata_ops);
  if (bufptr) abort(); else { (void)buflim; } /* FIXME: haven't implemented that bit yet! */
  dds_stream_from_serdata_default(&is, d);
  dds_stream_read_key (&is, sample, (const dds_topic_descriptor_t*) ((struct ddsi_sertopic_default *)topic)->type);
  return true; /* FIXME: can't conversion to sample fail? */
}

static bool serdata_default_topicless_to_sample_cdr_nokey (const struct ddsi_sertopic *topic, const struct ddsi_serdata *serdata_common, void *sample, void **bufptr, void *buflim)
{
  (void)topic; (void)sample; (void)bufptr; (void)buflim; (void)serdata_common;
  assert (serdata_common->topic == NULL);
  assert (serdata_common->kind == SDK_KEY);
  return true;
}

const struct ddsi_serdata_ops ddsi_serdata_ops_cdr = {
  .get_size = serdata_default_get_size,
  .eqkey = serdata_default_eqkey,
  .free = serdata_default_free,
  .from_ser = serdata_default_from_ser,
  .from_keyhash = ddsi_serdata_from_keyhash_cdr,
  .from_sample = serdata_default_from_sample_cdr,
  .to_ser = serdata_default_to_ser,
  .to_sample = serdata_default_to_sample_cdr,
  .to_ser_ref = serdata_default_to_ser_ref,
  .to_ser_unref = serdata_default_to_ser_unref,
  .to_topicless = serdata_default_to_topicless,
  .topicless_to_sample = serdata_default_topicless_to_sample_cdr
};

const struct ddsi_serdata_ops ddsi_serdata_ops_cdr_nokey = {
  .get_size = serdata_default_get_size,
  .eqkey = serdata_default_eqkey_nokey,
  .free = serdata_default_free,
  .from_ser = serdata_default_from_ser_nokey,
  .from_keyhash = ddsi_serdata_from_keyhash_cdr_nokey,
  .from_sample = serdata_default_from_sample_cdr_nokey,
  .to_ser = serdata_default_to_ser,
  .to_sample = serdata_default_to_sample_cdr,
  .to_ser_ref = serdata_default_to_ser_ref,
  .to_ser_unref = serdata_default_to_ser_unref,
  .to_topicless = serdata_default_to_topicless,
  .topicless_to_sample = serdata_default_topicless_to_sample_cdr_nokey
};

const struct ddsi_serdata_ops ddsi_serdata_ops_plist = {
  .get_size = serdata_default_get_size,
  .eqkey = serdata_default_eqkey,
  .free = serdata_default_free,
  .from_ser = serdata_default_from_ser,
  .from_keyhash = 0,
  .from_sample = serdata_default_from_sample_plist,
  .to_ser = serdata_default_to_ser,
  .to_sample = 0,
  .to_ser_ref = serdata_default_to_ser_ref,
  .to_ser_unref = serdata_default_to_ser_unref,
  .to_topicless = serdata_default_to_topicless,
  .topicless_to_sample = 0
};

const struct ddsi_serdata_ops ddsi_serdata_ops_rawcdr = {
  .get_size = serdata_default_get_size,
  .eqkey = serdata_default_eqkey,
  .free = serdata_default_free,
  .from_ser = serdata_default_from_ser,
  .from_keyhash = 0,
  .from_sample = serdata_default_from_sample_rawcdr,
  .to_ser = serdata_default_to_ser,
  .to_sample = 0,
  .to_ser_ref = serdata_default_to_ser_ref,
  .to_ser_unref = serdata_default_to_ser_unref,
  .to_topicless = serdata_default_to_topicless,
  .topicless_to_sample = 0
};

// Copyright(c) 2006 to 2022 ZettaScale Technology and others
//
// This program and the accompanying materials are made available under the
// terms of the Eclipse Public License v. 2.0 which is available at
// http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
// v. 1.0 which is available at
// http://www.eclipse.org/org/documents/edl-v10.php.
//
// SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

#include <stdlib.h>
#include <ctype.h>
#include <assert.h>
#include <string.h>

#include "dds/ddsrt/heap.h"
#include "dds/ddsrt/log.h"
#include "dds/ddsrt/md5.h"
#include "dds/ddsrt/mh3.h"
#include "dds/ddsi/ddsi_freelist.h"
#include "dds/ddsi/ddsi_tkmap.h"
#include "dds/cdr/dds_cdrstream.h"
#include "dds/ddsi/ddsi_radmin.h" /* sampleinfo */
#include "dds/ddsi/ddsi_domaingv.h"
#include "dds/ddsi/ddsi_serdata.h"
#include "dds__serdata_default.h"
#include "dds__loaned_sample.h"
#include "dds__heap_loan.h"
#include "dds__psmx.h"

/* This file implements the C language binding's default internal sample representation.

  Most of it is fairly straightforward given:
  - `data` ordinarily stores the guaranteed well-formed, native endianness serialised representation
  - zero-copy support can screw that up, but only if the sample is amenable to zero-copying because it
    contains no pointers
  - the key always stores the actual key value in XCDR2 native endianness representation with the key
    fields ordered in definition order (to protect against reording in mutable types, but without
    having to do something to order them for final and appendable types)
  - the key can alias the input data, and it can either be stored in a separately allocated block of
    memory or a embedded array for tiny keys (serdata_default_keybuf() returns the correct address)

  Regarding zero-copy support a.k.a. PSMX support a.k.a. loans: the loan pointer (`c.loan`) is always
  a null pointer unless the serdata is constructed via:
  - serdata_from_loan, or
  - serdata_default_from_psmx

  In case of `serdata_from_loan` (where the loan originates in dds_request_loan() and it is used
  exclusively for writing):
  - if “raw”:
     - `d->c.loan->sample_ptr` points to sample contents
     - `d->data` points to an empty CDR stream
  - otherwise (doesn't really occur):
     - `d->c.loan->sample_ptr` points to serialized data in borrowed memory
     - `d->data` points to a local copy

  In case of `serdata_default_from_psmx` (where the loan originates in PSMX and is used exclusively
  for reading):
  - if “raw”:
      - `d->c.loan` points to PSMX-owned loan with sample contents
      - `d->data` points to an empty CDR stream
  - otherwise:
      - `d->c.loan` null pointer
      - `d->data` points to a local copy
*/


/* 8k entries in the freelist seems to be roughly the amount needed to send
   minimum-size (well, 4 bytes) samples as fast as possible over loopback
   while using large messages -- actually, it stands to reason that this would
   be the same as the WHC node pool size */
#define MAX_POOL_SIZE 8192
#define MAX_SIZE_FOR_POOL 256
#define DEFAULT_NEW_SIZE 128
#define CHUNK_SIZE 128

static void serdata_default_get_keyhash (const struct ddsi_serdata *serdata_common, struct ddsi_keyhash *buf, bool force_md5);

#ifndef NDEBUG
static int ispowerof2_size (size_t x)
{
  return x > 0 && !(x & (x-1));
}
#endif

struct dds_serdatapool * dds_serdatapool_new (void)
{
  struct dds_serdatapool * pool;
  pool = ddsrt_malloc (sizeof (*pool));
  ddsi_freelist_init (&pool->freelist, MAX_POOL_SIZE, offsetof (struct dds_serdata_default, next));
  return pool;
}

static void serdata_free_wrap (void *elem)
{
#ifndef NDEBUG
  struct dds_serdata_default *d = elem;
  assert(ddsrt_atomic_ld32(&d->c.refc) == 0);
#endif
  dds_free(elem);
}

void dds_serdatapool_free (struct dds_serdatapool * pool)
{
  ddsi_freelist_fini (&pool->freelist, serdata_free_wrap);
  ddsrt_free (pool);
}

static size_t alignup_size (size_t x, size_t a)
{
  size_t m = a-1;
  assert (ispowerof2_size (a));
  return (x+m) & ~m;
}

static void *serdata_default_append (struct dds_serdata_default **d, size_t n)
{
  char *p;
  if ((*d)->pos + n > (*d)->size)
  {
    size_t size1 = alignup_size ((*d)->pos + n, CHUNK_SIZE);
    *d = ddsrt_realloc (*d, offsetof (struct dds_serdata_default, data) + size1);
    (*d)->size = (uint32_t)size1;
  }
  assert ((*d)->pos + n <= (*d)->size);
  p = (*d)->data + (*d)->pos;
  (*d)->pos += (uint32_t)n;
  return p;
}

static void serdata_default_append_blob (struct dds_serdata_default **d, size_t sz, const void *data)
{
  char *p = serdata_default_append (d, sz);
  memcpy (p, data, sz);
}

static const unsigned char *serdata_default_keybuf(const struct dds_serdata_default *d)
{
  assert(d->key.buftype != KEYBUFTYPE_UNSET);
  return (d->key.buftype == KEYBUFTYPE_STATIC) ? d->key.u.stbuf : d->key.u.dynbuf;
}

static struct ddsi_serdata *fix_serdata_default(struct dds_serdata_default *d, uint32_t basehash)
{
  assert (d->key.keysize > 0); // we use a different function for implementing the keyless case
  d->c.hash = ddsrt_mh3 (serdata_default_keybuf(d), d->key.keysize, basehash); // FIXME: or the full buffer, regardless of actual size?
  return &d->c;
}

static struct ddsi_serdata *fix_serdata_default_nokey(struct dds_serdata_default *d, uint32_t basehash)
{
  d->c.hash = basehash;
  return &d->c;
}

static uint32_t serdata_default_get_size(const struct ddsi_serdata *dcmn)
{
  const struct dds_serdata_default *d = (const struct dds_serdata_default *) dcmn;
  return d->pos + (uint32_t)sizeof (struct dds_cdr_header);
}

static bool serdata_default_eqkey(const struct ddsi_serdata *acmn, const struct ddsi_serdata *bcmn)
{
  const struct dds_serdata_default *a = (const struct dds_serdata_default *)acmn;
  const struct dds_serdata_default *b = (const struct dds_serdata_default *)bcmn;
  assert (a->key.buftype != KEYBUFTYPE_UNSET && b->key.buftype != KEYBUFTYPE_UNSET);
  return a->key.keysize == b->key.keysize && memcmp (serdata_default_keybuf(a), serdata_default_keybuf(b), a->key.keysize) == 0;
}

static bool serdata_default_eqkey_nokey (const struct ddsi_serdata *acmn, const struct ddsi_serdata *bcmn)
{
  (void)acmn; (void)bcmn;
  return true;
}

static void serdata_default_free(struct ddsi_serdata *dcmn)
{
  struct dds_serdata_default *d = (struct dds_serdata_default *)dcmn;
  assert (ddsrt_atomic_ld32 (&d->c.refc) == 0);

  if (d->key.buftype == KEYBUFTYPE_DYNALLOC)
    ddsrt_free (d->key.u.dynbuf);
  if (d->c.loan)
    dds_loaned_sample_unref (d->c.loan);
  if (d->size > MAX_SIZE_FOR_POOL || !ddsi_freelist_push (&d->serpool->freelist, d))
    dds_free (d);
}

static void serdata_default_init(struct dds_serdata_default *d, const struct dds_sertype_default *tp, enum ddsi_serdata_kind kind, uint32_t xcdr_version)
{
  ddsi_serdata_init (&d->c, &tp->c, kind);
  d->pos = 0;
#ifndef NDEBUG
  d->fixed = false;
#endif
  if (xcdr_version != DDSI_RTPS_CDR_ENC_VERSION_UNDEF)
    d->hdr.identifier = ddsi_sertype_get_native_enc_identifier (xcdr_version, tp->encoding_format);
  else
    d->hdr.identifier = 0;
  d->hdr.options = 0;
  d->key.buftype = KEYBUFTYPE_UNSET;
  d->key.keysize = 0;
}

static struct dds_serdata_default *serdata_default_allocnew (struct dds_serdatapool *serpool, uint32_t init_size)
{
  struct dds_serdata_default *d = ddsrt_malloc (offsetof (struct dds_serdata_default, data) + init_size);
  d->size = init_size;
  d->serpool = serpool;
  return d;
}

static struct dds_serdata_default *serdata_default_new_size (const struct dds_sertype_default *tp, enum ddsi_serdata_kind kind, uint32_t size, uint32_t xcdr_version)
{
  struct dds_serdata_default *d;
  if (size <= MAX_SIZE_FOR_POOL && (d = ddsi_freelist_pop (&tp->serpool->freelist)) != NULL)
    ddsrt_atomic_st32 (&d->c.refc, 1);
  else if ((d = serdata_default_allocnew (tp->serpool, size)) == NULL)
    return NULL;
  serdata_default_init (d, tp, kind, xcdr_version);
  return d;
}

static struct dds_serdata_default *serdata_default_new (const struct dds_sertype_default *tp, enum ddsi_serdata_kind kind, uint32_t xcdr_version)
{
  return serdata_default_new_size (tp, kind, DEFAULT_NEW_SIZE, xcdr_version);
}

static inline bool is_valid_xcdr1_id (unsigned short cdr_identifier)
{
  return (cdr_identifier == DDSI_RTPS_CDR_LE || cdr_identifier == DDSI_RTPS_CDR_BE);
}

static inline bool is_valid_xcdr2_id (unsigned short cdr_identifier)
{
  return (cdr_identifier == DDSI_RTPS_CDR2_LE || cdr_identifier == DDSI_RTPS_CDR2_BE
    || cdr_identifier == DDSI_RTPS_D_CDR2_LE || cdr_identifier == DDSI_RTPS_D_CDR2_BE
    || cdr_identifier == DDSI_RTPS_PL_CDR2_LE || cdr_identifier == DDSI_RTPS_PL_CDR2_BE);
}

static inline bool is_valid_xcdr_id (unsigned short cdr_identifier)
{
  return is_valid_xcdr1_id (cdr_identifier) || is_valid_xcdr2_id (cdr_identifier);
}

enum gen_serdata_key_input_kind {
  GSKIK_SAMPLE,
  GSKIK_CDRSAMPLE,
  GSKIK_CDRKEY
};

static inline bool is_topic_fixed_key_impl (uint32_t flagset, uint32_t xcdrv, bool key_hash)
{
  if (xcdrv == DDSI_RTPS_CDR_ENC_VERSION_1)
    return flagset & DDS_TOPIC_FIXED_KEY;
  else if (xcdrv == DDSI_RTPS_CDR_ENC_VERSION_2 && key_hash)
    return flagset & DDS_TOPIC_FIXED_KEY_XCDR2_KEYHASH;
  else if (xcdrv == DDSI_RTPS_CDR_ENC_VERSION_2)
    return flagset & DDS_TOPIC_FIXED_KEY_XCDR2;
  assert (0);
  return false;
}

static inline bool is_topic_fixed_key (uint32_t flagset, uint32_t xcdrv)
{
  return is_topic_fixed_key_impl (flagset, xcdrv, false);
}

static inline bool is_topic_fixed_key_keyhash (uint32_t flagset, uint32_t xcdrv)
{
  return is_topic_fixed_key_impl (flagset, xcdrv, true);
}

static bool gen_serdata_key (const struct dds_sertype_default *type, struct dds_serdata_default_key *kh, enum gen_serdata_key_input_kind input_kind, void *input)
{
  const struct dds_cdrstream_desc *desc = &type->type;
  struct dds_istream *is = NULL;
  kh->buftype = KEYBUFTYPE_UNSET;
  if (desc->keys.nkeys == 0)
  {
    kh->buftype = KEYBUFTYPE_STATIC;
    kh->keysize = 0;
  }
  else if (input_kind == GSKIK_CDRKEY)
  {
    is = input;
    if (is->m_xcdr_version == DDSI_RTPS_CDR_ENC_VERSION_2)
    {
      kh->buftype = KEYBUFTYPE_DYNALIAS;
      assert (is->m_size < (1u << 30));
      kh->keysize = is->m_size & SERDATA_DEFAULT_KEYSIZE_MASK;
      kh->u.dynbuf = (unsigned char *) is->m_buffer;
    }
  }

  if (kh->buftype == KEYBUFTYPE_UNSET)
  {
    // Force the key in the serdata object to be serialized in XCDR2 format
    dds_ostream_t os;
    dds_ostream_init (&os, &dds_cdrstream_default_allocator, 0, DDSI_RTPS_CDR_ENC_VERSION_2);
    if (is_topic_fixed_key(desc->flagset, DDSI_RTPS_CDR_ENC_VERSION_2))
    {
      // FIXME: there are more cases where we don't have to allocate memory
      os.m_buffer = kh->u.stbuf;
      os.m_size = DDS_FIXED_KEY_MAX_SIZE;
    }
    switch (input_kind)
    {
      case GSKIK_SAMPLE:
        if (!dds_stream_write_key (&os, DDS_CDR_KEY_SERIALIZATION_SAMPLE, &dds_cdrstream_default_allocator, input, &type->type))
          return false;
        break;
      case GSKIK_CDRSAMPLE:
        if (!dds_stream_extract_key_from_data (input, &os, &dds_cdrstream_default_allocator, &type->type))
          return false;
        break;
      case GSKIK_CDRKEY:
        assert (is);
        assert (is->m_xcdr_version == DDSI_RTPS_CDR_ENC_VERSION_1);
        dds_stream_extract_key_from_key (is, &os, DDS_CDR_KEY_SERIALIZATION_SAMPLE, &dds_cdrstream_default_allocator, &type->type);
        break;
    }
    assert (os.m_index < (1u << 30));
    kh->keysize = os.m_index & SERDATA_DEFAULT_KEYSIZE_MASK;
    if (is_topic_fixed_key (desc->flagset, DDSI_RTPS_CDR_ENC_VERSION_2))
      kh->buftype = KEYBUFTYPE_STATIC;
    else
    {
      kh->buftype = KEYBUFTYPE_DYNALLOC;
      kh->u.dynbuf = ddsrt_realloc (os.m_buffer, os.m_index); // don't waste bytes FIXME: maybe should be willing to waste a little
    }
  }
  return true;
}

static bool gen_serdata_key_from_sample (const struct dds_sertype_default *type, struct dds_serdata_default_key *kh, const char *sample)
{
  return gen_serdata_key (type, kh, GSKIK_SAMPLE, (void *) sample);
}

static bool gen_serdata_key_from_cdr (dds_istream_t *is, struct dds_serdata_default_key *kh, const struct dds_sertype_default *type, const bool just_key)
{
  return gen_serdata_key (type, kh, just_key ? GSKIK_CDRKEY : GSKIK_CDRSAMPLE, is);
}

/* Construct a serdata from a fragchain received over the network */
static struct dds_serdata_default *serdata_default_from_ser_common (const struct ddsi_sertype *tpcmn, enum ddsi_serdata_kind kind, const struct ddsi_rdata *fragchain, size_t size)
  ddsrt_nonnull_all;

static struct dds_serdata_default *serdata_default_from_ser_common (const struct ddsi_sertype *tpcmn, enum ddsi_serdata_kind kind, const struct ddsi_rdata *fragchain, size_t size)
{
  const struct dds_sertype_default *tp = (const struct dds_sertype_default *)tpcmn;

  /* FIXME: check whether this really is the correct maximum: offsets are relative
     to the CDR header, but there are also some places that use a serdata as-if it
     were a stream, and those use offsets (m_index) relative to the start of the
     serdata */
  if (size > UINT32_MAX - offsetof (struct dds_serdata_default, hdr))
    return NULL;
  struct dds_serdata_default *d = serdata_default_new_size (tp, kind, (uint32_t) size, DDSI_RTPS_CDR_ENC_VERSION_UNDEF);
  if (d == NULL)
    return NULL;

  uint32_t off = 4; /* must skip the CDR header */

  assert (fragchain->min == 0);
  assert (fragchain->maxp1 >= off); /* CDR header must be in first fragment */

  memcpy (&d->hdr, DDSI_RMSG_PAYLOADOFF (fragchain->rmsg, DDSI_RDATA_PAYLOAD_OFF (fragchain)), sizeof (d->hdr));
  if (!is_valid_xcdr_id (d->hdr.identifier))
    goto err;

  for (const struct ddsi_rdata *frag = fragchain; frag != NULL; frag = frag->nextfrag)
  {
    assert (frag->min <= off);
    assert (frag->maxp1 <= size);
    if (frag->maxp1 > off)
    {
      /* only copy if this fragment adds data */
      const unsigned char *payload = DDSI_RMSG_PAYLOADOFF (frag->rmsg, DDSI_RDATA_PAYLOAD_OFF (frag));
      serdata_default_append_blob (&d, frag->maxp1 - off, payload + off - frag->min);
      off = frag->maxp1;
    }
  }

  const bool needs_bswap = !DDSI_RTPS_CDR_ENC_IS_NATIVE (d->hdr.identifier);
  d->hdr.identifier = DDSI_RTPS_CDR_ENC_TO_NATIVE (d->hdr.identifier);
  const uint32_t pad = ddsrt_fromBE2u (d->hdr.options) & DDS_CDR_HDR_PADDING_MASK;
  const uint32_t xcdr_version = ddsi_sertype_enc_id_xcdr_version (d->hdr.identifier);
  const uint32_t encoding_format = ddsi_sertype_enc_id_enc_format (d->hdr.identifier);
  if (ddsi_sertype_get_native_enc_identifier (xcdr_version, encoding_format) != ddsi_sertype_get_native_enc_identifier (tp->write_encoding_version /* FIXME: is this correct, or use xcdr_version from data? */, tp->encoding_format))
    goto err;

  uint32_t actual_size;
  if (d->pos < pad || !dds_stream_normalize (d->data, d->pos - pad, needs_bswap, xcdr_version, &tp->type, kind == SDK_KEY, &actual_size))
    goto err;

  dds_istream_t is;
  dds_istream_init (&is, actual_size, d->data, xcdr_version);
  if (!gen_serdata_key_from_cdr (&is, &d->key, tp, kind == SDK_KEY))
    goto err;
  return d;

err:
  ddsi_serdata_unref (&d->c);
  return NULL;
}

static struct dds_serdata_default *serdata_default_from_ser_iov_common (const struct ddsi_sertype *tpcmn, enum ddsi_serdata_kind kind, ddsrt_msg_iovlen_t niov, const ddsrt_iovec_t *iov, size_t size)
{
  const struct dds_sertype_default *tp = (const struct dds_sertype_default *)tpcmn;

  /* FIXME: check whether this really is the correct maximum: offsets are relative
     to the CDR header, but there are also some places that use a serdata as-if it
     were a stream, and those use offsets (m_index) relative to the start of the
     serdata */
  if (size > UINT32_MAX - offsetof (struct dds_serdata_default, hdr))
    return NULL;
  assert (niov >= 1);
  if (iov[0].iov_len < 4) /* CDR header */
    return NULL;
  struct dds_serdata_default *d = serdata_default_new_size (tp, kind, (uint32_t) size, DDSI_RTPS_CDR_ENC_VERSION_UNDEF);
  if (d == NULL)
    return NULL;

  memcpy (&d->hdr, iov[0].iov_base, sizeof (d->hdr));
  if (!is_valid_xcdr_id (d->hdr.identifier))
    goto err;
  serdata_default_append_blob (&d, iov[0].iov_len - 4, (const char *) iov[0].iov_base + 4);
  for (ddsrt_msg_iovlen_t i = 1; i < niov; i++)
    serdata_default_append_blob (&d, iov[i].iov_len, iov[i].iov_base);

  const bool needs_bswap = !DDSI_RTPS_CDR_ENC_IS_NATIVE (d->hdr.identifier);
  d->hdr.identifier = DDSI_RTPS_CDR_ENC_TO_NATIVE (d->hdr.identifier);
  const uint32_t pad = ddsrt_fromBE2u (d->hdr.options) & 2;
  const uint32_t xcdr_version = ddsi_sertype_enc_id_xcdr_version (d->hdr.identifier);
  const uint32_t encoding_format = ddsi_sertype_enc_id_enc_format (d->hdr.identifier);
  if (ddsi_sertype_get_native_enc_identifier (xcdr_version, encoding_format) != ddsi_sertype_get_native_enc_identifier (tp->write_encoding_version /* FIXME: is this correct, or use xcdr_version from data? */, tp->encoding_format))
    goto err;

  uint32_t actual_size;
  if (d->pos < pad || !dds_stream_normalize (d->data, d->pos - pad, needs_bswap, xcdr_version, &tp->type, kind == SDK_KEY, &actual_size))
    goto err;

  dds_istream_t is;
  dds_istream_init (&is, actual_size, d->data, xcdr_version);
  if (!gen_serdata_key_from_cdr (&is, &d->key, tp, kind == SDK_KEY))
    goto err;
  return d;

err:
  ddsi_serdata_unref (&d->c);
  return NULL;
}

static struct ddsi_serdata *serdata_default_from_ser (const struct ddsi_sertype *tpcmn, enum ddsi_serdata_kind kind, const struct ddsi_rdata *fragchain, size_t size)
{
  struct dds_serdata_default *d;
  if ((d = serdata_default_from_ser_common (tpcmn, kind, fragchain, size)) == NULL)
    return NULL;
  return fix_serdata_default (d, tpcmn->serdata_basehash);
}

static struct ddsi_serdata *serdata_default_from_ser_iov (const struct ddsi_sertype *tpcmn, enum ddsi_serdata_kind kind, ddsrt_msg_iovlen_t niov, const ddsrt_iovec_t *iov, size_t size)
{
  struct dds_serdata_default *d;
  if ((d = serdata_default_from_ser_iov_common (tpcmn, kind, niov, iov, size)) == NULL)
    return NULL;
  return fix_serdata_default (d, tpcmn->serdata_basehash);
}

static struct ddsi_serdata *serdata_default_from_ser_nokey (const struct ddsi_sertype *tpcmn, enum ddsi_serdata_kind kind, const struct ddsi_rdata *fragchain, size_t size)
{
  struct dds_serdata_default *d;
  if ((d = serdata_default_from_ser_common (tpcmn, kind, fragchain, size)) == NULL)
    return NULL;
  return fix_serdata_default_nokey (d, tpcmn->serdata_basehash);
}

static struct ddsi_serdata *serdata_default_from_ser_iov_nokey (const struct ddsi_sertype *tpcmn, enum ddsi_serdata_kind kind, ddsrt_msg_iovlen_t niov, const ddsrt_iovec_t *iov, size_t size)
{
  struct dds_serdata_default *d;
  if ((d = serdata_default_from_ser_iov_common (tpcmn, kind, niov, iov, size)) == NULL)
    return NULL;
  return fix_serdata_default_nokey (d, tpcmn->serdata_basehash);
}

static struct ddsi_serdata *serdata_default_from_keyhash_cdr (const struct ddsi_sertype *tpcmn, const ddsi_keyhash_t *keyhash)
{
  const struct dds_sertype_default *tp = (const struct dds_sertype_default *)tpcmn;
  if (!is_topic_fixed_key_keyhash (tp->type.flagset, DDSI_RTPS_CDR_ENC_VERSION_2))
  {
    /* keyhash is MD5 of a key value, so impossible to turn into a key value */
    return NULL;
  }
  else
  {
    const ddsrt_iovec_t iovec[2] = {
      { .iov_base = (unsigned char[]) { 0,0,0,0 }, .iov_len = 4}, // big-endian, unspecified padding
      { .iov_base = (void *) keyhash->value, .iov_len = (ddsrt_iov_len_t) sizeof (*keyhash) }
    };
    return serdata_default_from_ser_iov (tpcmn, SDK_KEY, 2, iovec, 4 + sizeof (*keyhash));
  }
}

static struct ddsi_serdata *serdata_default_from_keyhash_cdr_nokey (const struct ddsi_sertype *tpcmn, const ddsi_keyhash_t *keyhash)
{
  const struct dds_sertype_default *tp = (const struct dds_sertype_default *)tpcmn;
  /* For keyless topic, the CDR encoding version is irrelevant */
  struct dds_serdata_default *d = serdata_default_new(tp, SDK_KEY, DDSI_RTPS_CDR_ENC_VERSION_UNDEF);
  if (d == NULL)
    return NULL;
  (void)keyhash;
  return fix_serdata_default_nokey(d, tp->c.serdata_basehash);
}

static void istream_from_serdata_default (dds_istream_t *s, const struct dds_serdata_default *d)
{
  if (d->c.loan != NULL &&
      (d->c.loan->metadata->sample_state == DDS_LOANED_SAMPLE_STATE_SERIALIZED_KEY ||
       d->c.loan->metadata->sample_state == DDS_LOANED_SAMPLE_STATE_SERIALIZED_DATA))
  {
    s->m_buffer = d->c.loan->sample_ptr;
    s->m_index = 0;
    s->m_size = d->c.loan->metadata->sample_size;
  }
  else
  {
    s->m_buffer = (const unsigned char *) d;
    s->m_index = (uint32_t) offsetof (struct dds_serdata_default, data);
    s->m_size = d->size + s->m_index;
  }
#if DDSRT_ENDIAN == DDSRT_LITTLE_ENDIAN
  assert (DDSI_RTPS_CDR_ENC_LE (d->hdr.identifier));
#elif DDSRT_ENDIAN == DDSRT_BIG_ENDIAN
  assert (!DDSI_RTPS_CDR_ENC_LE (d->hdr.identifier));
#endif
  s->m_xcdr_version = ddsi_sertype_enc_id_xcdr_version (d->hdr.identifier);
}

static void ostream_from_serdata_default (dds_ostream_t *s, const struct dds_serdata_default *d)
{
  s->m_buffer = (unsigned char *) d;
  s->m_index = (uint32_t) offsetof (struct dds_serdata_default, data);
  s->m_size = d->size + s->m_index;
#if DDSRT_ENDIAN == DDSRT_LITTLE_ENDIAN
  assert (DDSI_RTPS_CDR_ENC_LE (d->hdr.identifier));
#elif DDSRT_ENDIAN == DDSRT_BIG_ENDIAN
  assert (!DDSI_RTPS_CDR_ENC_LE (d->hdr.identifier));
#endif
  s->m_xcdr_version = ddsi_sertype_enc_id_xcdr_version (d->hdr.identifier);
}

static void ostream_add_to_serdata_default (dds_ostream_t *s, struct dds_serdata_default **d)
{
  /* DDSI requires 4 byte alignment */
  const uint32_t pad = dds_cdr_alignto4_clear_and_resize (s, &dds_cdrstream_default_allocator, s->m_xcdr_version);
  assert (pad <= 3);

  /* Reset data pointer as stream may have reallocated */
  (*d) = (void *) s->m_buffer;
  (*d)->pos = (s->m_index - (uint32_t) offsetof (struct dds_serdata_default, data));
  (*d)->size = (s->m_size - (uint32_t) offsetof (struct dds_serdata_default, data));
  (*d)->hdr.options = ddsrt_toBE2u ((uint16_t) pad);
}


static struct dds_serdata_default *serdata_default_from_sample_cdr_common (const struct ddsi_sertype *tpcmn, enum ddsi_serdata_kind kind, uint32_t xcdr_version, const void *sample)
{
  const struct dds_sertype_default *tp = (const struct dds_sertype_default *)tpcmn;
  struct dds_serdata_default *d = serdata_default_new(tp, kind, xcdr_version);
  if (d == NULL)
    return NULL;

  dds_ostream_t os;
  ostream_from_serdata_default (&os, d);
  switch (kind)
  {
    case SDK_EMPTY:
      ostream_add_to_serdata_default (&os, &d);
      break;
    case SDK_KEY: {
      const bool ok = dds_stream_write_key (&os, DDS_CDR_KEY_SERIALIZATION_SAMPLE, &dds_cdrstream_default_allocator, sample, &tp->type);
      ostream_add_to_serdata_default (&os, &d);
      if (!ok)
        goto error;

      /* FIXME: detect cases where the XCDR1 and 2 representations are equal,
         so that we could alias the XCDR1 key from d->data */
      /* FIXME: use CDR encoding version used by the writer, not the write encoding
         of the sertype */
      if (tp->write_encoding_version == DDSI_RTPS_CDR_ENC_VERSION_2)
      {
        d->key.buftype = KEYBUFTYPE_DYNALIAS;
        // dds_ostream_add_to_serdata_default pads the size to a multiple of 4,
        // writing the number of padding bytes added in the 2 least-significant
        // bits of options (when interpreted as a big-endian 16-bit number) in
        // accordance with the XTypes spec.
        //
        // Those padding bytes are not part of the key!
        assert (ddsrt_fromBE2u (d->hdr.options) < 4);
        d->key.keysize = (d->pos - ddsrt_fromBE2u (d->hdr.options)) & SERDATA_DEFAULT_KEYSIZE_MASK;
        d->key.u.dynbuf = (unsigned char *) d->data;
      }
      else
      {
        /* We have a XCDR1 key, so this must be converted to XCDR2 to store
           it as key in this serdata. */
        if (!gen_serdata_key_from_sample (tp, &d->key, sample))
          goto error;
      }
      break;
    }
    case SDK_DATA: {
      const bool ok = dds_stream_write_sample (&os, &dds_cdrstream_default_allocator, sample, &tp->type);
      // `os` aliased what was in `d`, but was changed and may have moved.
      // `d` therefore needs to be updated even when write_sample failed.
      ostream_add_to_serdata_default (&os, &d);
      if (!ok)
        goto error;
      if (!gen_serdata_key_from_sample (tp, &d->key, sample))
        goto error;
      break;
    }
  }
  return d;

error:
  ddsi_serdata_unref (&d->c);
  return NULL;
}

static struct ddsi_serdata *serdata_default_from_sample_data_representation (const struct ddsi_sertype *tpcmn, enum ddsi_serdata_kind kind, dds_data_representation_id_t data_representation, const void *sample, bool key)
{
  assert (data_representation == DDS_DATA_REPRESENTATION_XCDR1 || data_representation == DDS_DATA_REPRESENTATION_XCDR2);
  struct dds_serdata_default *d;
  uint32_t xcdr_version = data_representation == DDS_DATA_REPRESENTATION_XCDR1 ? DDSI_RTPS_CDR_ENC_VERSION_1 : DDSI_RTPS_CDR_ENC_VERSION_2;
  if ((d = serdata_default_from_sample_cdr_common (tpcmn, kind, xcdr_version, sample)) == NULL)
    return NULL;
  return key ? fix_serdata_default (d, tpcmn->serdata_basehash) : fix_serdata_default_nokey (d, tpcmn->serdata_basehash);
}

static struct ddsi_serdata *serdata_default_from_sample_cdr (const struct ddsi_sertype *tpcmn, enum ddsi_serdata_kind kind, const void *sample)
{
  return serdata_default_from_sample_data_representation (tpcmn, kind, DDS_DATA_REPRESENTATION_XCDR1, sample, true);
}

static struct ddsi_serdata *serdata_default_from_sample_xcdr2 (const struct ddsi_sertype *tpcmn, enum ddsi_serdata_kind kind, const void *sample)
{
  return serdata_default_from_sample_data_representation (tpcmn, kind, DDS_DATA_REPRESENTATION_XCDR2, sample, true);
}

static struct ddsi_serdata *serdata_default_from_sample_cdr_nokey (const struct ddsi_sertype *tpcmn, enum ddsi_serdata_kind kind, const void *sample)
{
  return serdata_default_from_sample_data_representation (tpcmn, kind, DDS_DATA_REPRESENTATION_XCDR1, sample, false);
}

static struct ddsi_serdata *serdata_default_from_sample_xcdr2_nokey (const struct ddsi_sertype *tpcmn, enum ddsi_serdata_kind kind, const void *sample)
{
  return serdata_default_from_sample_data_representation (tpcmn, kind, DDS_DATA_REPRESENTATION_XCDR2, sample, false);
}


static struct ddsi_serdata *serdata_default_to_untyped (const struct ddsi_serdata *serdata_common)
{
  const struct dds_serdata_default *d = (const struct dds_serdata_default *)serdata_common;
  const struct dds_sertype_default *tp = (const struct dds_sertype_default *)d->c.type;

  assert (d->hdr.identifier == DDSI_RTPS_SAMPLE_NATIVE || DDSI_RTPS_CDR_ENC_IS_NATIVE (d->hdr.identifier));
  struct dds_serdata_default *d_tl = serdata_default_new (tp, SDK_KEY, DDSI_RTPS_CDR_ENC_VERSION_2);
  if (d_tl == NULL)
    return NULL;
  d_tl->c.type = NULL;
  d_tl->c.hash = d->c.hash;
  d_tl->c.timestamp.v = INT64_MIN;
  /* These things are used for the key-to-instance map and only subject to eq, free and conversion to an invalid
     sample of some type for topics that can end up in a RHC, so, of the four kinds we have, only for CDR-with-key
     the payload is of interest. */
  if (d->c.ops == &dds_serdata_ops_cdr || d->c.ops == &dds_serdata_ops_xcdr2)
  {
    serdata_default_append_blob (&d_tl, d->key.keysize, serdata_default_keybuf (d));
    d_tl->key.buftype = KEYBUFTYPE_DYNALIAS;
    d_tl->key.keysize = d->key.keysize;
    d_tl->key.u.dynbuf = (unsigned char *) d_tl->data;
  }
  else
  {
    assert (d->c.ops == &dds_serdata_ops_cdr_nokey || d->c.ops == &dds_serdata_ops_xcdr2_nokey);
  }
  return (struct ddsi_serdata *)d_tl;
}

/* Fill buffer with 'size' bytes of serialised data, starting from 'off'; 0 <= off < off+sz <= alignup4(size(d)) */
static void serdata_default_to_ser (const struct ddsi_serdata *serdata_common, size_t off, size_t sz, void *buf)
{
  const struct dds_serdata_default *d = (const struct dds_serdata_default *)serdata_common;
  assert (off < d->pos + sizeof(struct dds_cdr_header));
  assert (sz <= alignup_size (d->pos + sizeof(struct dds_cdr_header), 4) - off);
  memcpy (buf, (char *)&d->hdr + off, sz);
}

static struct ddsi_serdata *serdata_default_to_ser_ref (const struct ddsi_serdata *serdata_common, size_t off, size_t sz, ddsrt_iovec_t *ref)
{
  const struct dds_serdata_default *d = (const struct dds_serdata_default *)serdata_common;
  assert (off < d->pos + sizeof(struct dds_cdr_header));
  assert (sz <= alignup_size (d->pos + sizeof(struct dds_cdr_header), 4) - off);
  ref->iov_base = (char *)&d->hdr + off;
  ref->iov_len = (ddsrt_iov_len_t)sz;
  return ddsi_serdata_ref(serdata_common);
}

static void serdata_default_to_ser_unref (struct ddsi_serdata *serdata_common, const ddsrt_iovec_t *ref)
{
  (void)ref;
  ddsi_serdata_unref(serdata_common);
}

static bool serdata_default_to_sample_cdr (const struct ddsi_serdata *serdata_common, void *sample, void **bufptr, void *buflim)
{
  const struct dds_serdata_default *d = (const struct dds_serdata_default *)serdata_common;
  const struct dds_sertype_default *tp = (const struct dds_sertype_default *) d->c.type;
  dds_istream_t is;
  if (bufptr) abort(); else { (void)buflim; } /* FIXME: haven't implemented that bit yet! */
  if (d->c.loan != NULL &&
      tp->c.is_memcpy_safe &&
      (d->c.loan->metadata->sample_state == DDS_LOANED_SAMPLE_STATE_RAW_DATA ||
       d->c.loan->metadata->sample_state == DDS_LOANED_SAMPLE_STATE_RAW_KEY))
  {
    assert (d->c.loan->metadata->cdr_identifier == DDSI_RTPS_SAMPLE_NATIVE);
    memcpy (sample, d->c.loan->sample_ptr, d->c.loan->metadata->sample_size);
  }
  else
  {
    assert (DDSI_RTPS_CDR_ENC_IS_NATIVE (d->hdr.identifier));
    istream_from_serdata_default (&is, d);
    if (d->c.kind == SDK_KEY)
      dds_stream_read_key (&is, sample, &dds_cdrstream_default_allocator, &tp->type);
    else
      dds_stream_read_sample (&is, sample, &dds_cdrstream_default_allocator, &tp->type);
  }
  return true; /* FIXME: can't conversion to sample fail? */
}

static bool serdata_default_untyped_to_sample_cdr (const struct ddsi_sertype *sertype_common, const struct ddsi_serdata *serdata_common, void *sample, void **bufptr, void *buflim)
{
  const struct dds_serdata_default *d = (const struct dds_serdata_default *)serdata_common;
  const struct dds_sertype_default *tp = (const struct dds_sertype_default *) sertype_common;
  dds_istream_t is;
  assert (d->c.type == NULL);
  assert (d->c.kind == SDK_KEY);
  assert (d->c.ops == sertype_common->serdata_ops);
  assert (DDSI_RTPS_CDR_ENC_IS_NATIVE (d->hdr.identifier));
  if (bufptr) abort(); else { (void)buflim; } /* FIXME: haven't implemented that bit yet! */
  dds_istream_init (&is, d->key.keysize, serdata_default_keybuf (d), DDSI_RTPS_CDR_ENC_VERSION_2);
  dds_stream_read_key (&is, sample, &dds_cdrstream_default_allocator, &tp->type);
  return true; /* FIXME: can't conversion to sample fail? */
}

static bool serdata_default_untyped_to_sample_cdr_nokey (const struct ddsi_sertype *sertype_common, const struct ddsi_serdata *serdata_common, void *sample, void **bufptr, void *buflim)
{
  (void)sertype_common; (void)sample; (void)bufptr; (void)buflim; (void)serdata_common;
  assert (serdata_common->type == NULL);
  assert (serdata_common->kind == SDK_KEY);
  return true;
}

static size_t serdata_default_print_cdr (const struct ddsi_sertype *sertype_common, const struct ddsi_serdata *serdata_common, char *buf, size_t size)
{
  const struct dds_serdata_default *d = (const struct dds_serdata_default *)serdata_common;
  const struct dds_sertype_default *tp = (const struct dds_sertype_default *)sertype_common;
  dds_istream_t is;
  if (d->c.loan != NULL &&
      (d->c.loan->metadata->sample_state == DDS_LOANED_SAMPLE_STATE_RAW_KEY ||
       d->c.loan->metadata->sample_state == DDS_LOANED_SAMPLE_STATE_RAW_DATA))
  {
    return (size_t) snprintf (buf, size, "[RAW]");
  }
  else
  {
    istream_from_serdata_default (&is, d);
    if (d->c.kind == SDK_KEY)
      return dds_stream_print_key (&is, &tp->type, buf, size);
    else
      return dds_stream_print_sample (&is, &tp->type, buf, size);
  }
}

static void serdata_default_get_keyhash (const struct ddsi_serdata *serdata_common, struct ddsi_keyhash *buf, bool force_md5)
{
  const struct dds_serdata_default *d = (const struct dds_serdata_default *)serdata_common;
  const struct dds_sertype_default *tp = (const struct dds_sertype_default *)d->c.type;
  assert(buf);
  assert(d->key.buftype != KEYBUFTYPE_UNSET);

  // Convert native representation to what keyhashes expect
  // d->key could also be in big-endian, but that eliminates the possibility of aliasing d->data
  // on little endian machines and forces a double copy of the key to be present for SDK_KEY
  //
  // As nobody should be using the DDSI keyhash, the price to pay for the conversion looks like
  // a price worth paying

  uint32_t xcdrv = ddsi_sertype_enc_id_xcdr_version (d->hdr.identifier);

  /* serdata has a XCDR2 serialized key, so initializer the istream with this version
     and with the size of that key (d->key.keysize) */
  dds_istream_t is;
  dds_istream_init (&is, d->key.keysize, serdata_default_keybuf(d), DDSI_RTPS_CDR_ENC_VERSION_2);

  /* The output stream uses the XCDR version from the serdata, so that the keyhash in
     ostream is calculated using this CDR representation (XTypes spec 7.6.8, RTPS spec 9.6.3.8) */
  dds_ostreamBE_t os;
  dds_ostreamBE_init (&os, &dds_cdrstream_default_allocator, 0, xcdrv);
  dds_stream_extract_keyBE_from_key (&is, &os, DDS_CDR_KEY_SERIALIZATION_KEYHASH, &dds_cdrstream_default_allocator, &tp->type);
  assert (is.m_index == d->key.keysize);

  /* Don't use the actual key size for checking if hashing is required,
     but the worst-case key-size (see also XTypes spec 7.6.8 step 5.2) */
  uint32_t actual_keysz = os.x.m_index;
  if (force_md5 || !is_topic_fixed_key_keyhash (tp->type.flagset, xcdrv))
  {
    ddsrt_md5_state_t md5st;
    ddsrt_md5_init (&md5st);
    ddsrt_md5_append (&md5st, (ddsrt_md5_byte_t *) os.x.m_buffer, actual_keysz);
    ddsrt_md5_finish (&md5st, (ddsrt_md5_byte_t *) buf->value);
  }
  else
  {
    memset (buf->value, 0, DDS_FIXED_KEY_MAX_SIZE);
    if (actual_keysz > 0)
      memcpy (buf->value, os.x.m_buffer, actual_keysz);
  }
  dds_ostreamBE_fini (&os, &dds_cdrstream_default_allocator);
}

static bool loaned_sample_state_to_serdata_kind (dds_loaned_sample_state_t lss, enum ddsi_serdata_kind *kind)
{
  switch (lss)
  {
    case DDS_LOANED_SAMPLE_STATE_RAW_KEY:
    case DDS_LOANED_SAMPLE_STATE_SERIALIZED_KEY:
      *kind = SDK_KEY;
      return true;
    case DDS_LOANED_SAMPLE_STATE_RAW_DATA:
    case DDS_LOANED_SAMPLE_STATE_SERIALIZED_DATA:
      *kind = SDK_DATA;
      return true;
    case DDS_LOANED_SAMPLE_STATE_UNITIALIZED:
      // invalid
      return false;
  }
  // "impossible" value
  return false;
}

static struct ddsi_serdata *serdata_default_from_loaned_sample (const struct ddsi_sertype *type, enum ddsi_serdata_kind kind, const char *sample, dds_loaned_sample_t *loaned_sample, bool will_require_cdr)
{
  /*
    type = the type of data being serialized
    kind = the kind of data contained (key or normal)
    sample = the raw sample made into the serdata
    loaned_sample = the loaned buffer in use
    will_require_cdr = whether we will need the CDR (or a highly likely to need it)
  */
  const struct dds_sertype_default *tp = (const struct dds_sertype_default *) type;

  assert (sample == loaned_sample->sample_ptr);
  assert (loaned_sample->metadata->sample_state == (kind == SDK_KEY ? DDS_LOANED_SAMPLE_STATE_RAW_KEY : DDS_LOANED_SAMPLE_STATE_RAW_DATA));
  assert (loaned_sample->metadata->cdr_identifier == DDSI_RTPS_SAMPLE_NATIVE);
  assert (loaned_sample->metadata->cdr_options == 0);

  struct dds_serdata_default *d;
  if (will_require_cdr)
  {
    // If serialization is/will be required, construct the serdata the normal way
    d = (struct dds_serdata_default *) type->serdata_ops->from_sample (type, kind, sample);
    if (d == NULL)
      return NULL;
  }
  else
  {
    // If we know there is no neeed for the serialized representation (so only PSMX and "memcpy safe"),
    // construct an empty serdata and stay away from the serializers
    d = serdata_default_new (tp, kind, tp->write_encoding_version);
    if (d == NULL)
      return NULL;
    if (!gen_serdata_key_from_sample (tp, &d->key, sample))
    {
      ddsi_serdata_unref (&d->c);
      return NULL;
    }
  }

  // now owner of loan
  d->c.loan = loaned_sample;
  if (tp->c.has_key)
    (void) fix_serdata_default (d, tp->c.serdata_basehash);
  else
    (void) fix_serdata_default_nokey (d, tp->c.serdata_basehash);
  return (struct ddsi_serdata *) d;
}

static struct ddsi_serdata * serdata_default_from_psmx (const struct ddsi_sertype *type, dds_loaned_sample_t *loaned_sample)
{
  const struct dds_sertype_default *tp = (const struct dds_sertype_default *) type;
  struct dds_psmx_metadata *md = loaned_sample->metadata;
  enum ddsi_serdata_kind kind;
  if (!loaned_sample_state_to_serdata_kind (md->sample_state, &kind))
    return NULL;

  uint32_t xcdr_version;
  if (is_valid_xcdr1_id (md->cdr_identifier))
    xcdr_version = DDSI_RTPS_CDR_ENC_VERSION_1;
  else if (is_valid_xcdr2_id (md->cdr_identifier))
    xcdr_version = DDSI_RTPS_CDR_ENC_VERSION_2;
  else if (md->cdr_identifier == DDSI_RTPS_SAMPLE_NATIVE)
    xcdr_version = DDSI_RTPS_CDR_ENC_VERSION_UNDEF;
  else
    return NULL;

  const uint32_t pad = ddsrt_fromBE2u (md->cdr_options) & DDS_CDR_HDR_PADDING_MASK;
  struct dds_serdata_default *d = serdata_default_new_size (tp, kind, md->sample_size + pad, xcdr_version);
  d->c.statusinfo = md->statusinfo;
  d->c.timestamp.v = md->timestamp;
  if (md->cdr_identifier == DDSI_RTPS_SAMPLE_NATIVE)
    d->hdr.identifier = DDSI_RTPS_SAMPLE_NATIVE;
  d->hdr.options = md->cdr_options;

  switch (md->sample_state)
  {
    case DDS_LOANED_SAMPLE_STATE_UNITIALIZED:
      assert (0);
      return NULL;
    case DDS_LOANED_SAMPLE_STATE_RAW_KEY:
    case DDS_LOANED_SAMPLE_STATE_RAW_DATA:
      if (d->hdr.identifier != DDSI_RTPS_SAMPLE_NATIVE)
      {
        ddsi_serdata_unref (&d->c);
        return NULL;
      }
      d->c.loan = loaned_sample;
      dds_loaned_sample_ref (d->c.loan);
      gen_serdata_key_from_sample (tp, &d->key, d->c.loan->sample_ptr);
      break;
    case DDS_LOANED_SAMPLE_STATE_SERIALIZED_KEY:
    case DDS_LOANED_SAMPLE_STATE_SERIALIZED_DATA: {
      const bool just_key = (md->sample_state == DDS_LOANED_SAMPLE_STATE_SERIALIZED_KEY);
      uint32_t actual_size;

      // FIXME: how much do we trust PSMX-provided data? If we *really* trust it, we can skip this
      if (!dds_stream_normalize (loaned_sample->sample_ptr, md->sample_size, false, xcdr_version, &tp->type, just_key, &actual_size))
      {
        ddsi_serdata_unref (&d->c);
        return NULL;
      }
      serdata_default_append_blob (&d, actual_size, loaned_sample->sample_ptr);
      dds_istream_t is;
      dds_istream_init (&is, actual_size, d->data, xcdr_version);
      if (!gen_serdata_key_from_cdr (&is, &d->key, tp, just_key))
      {
        ddsi_serdata_unref (&d->c);
        return NULL;
      }
      break;
    }
  }

  if (tp->c.has_key)
    (void) fix_serdata_default (d, tp->c.serdata_basehash);
  else
    (void) fix_serdata_default_nokey (d, tp->c.serdata_basehash);
  return (struct ddsi_serdata *) d;
}

const struct ddsi_serdata_ops dds_serdata_ops_cdr = {
  .get_size = serdata_default_get_size,
  .eqkey = serdata_default_eqkey,
  .free = serdata_default_free,
  .from_ser = serdata_default_from_ser,
  .from_ser_iov = serdata_default_from_ser_iov,
  .from_keyhash = serdata_default_from_keyhash_cdr,
  .from_sample = serdata_default_from_sample_cdr,
  .to_ser = serdata_default_to_ser,
  .to_sample = serdata_default_to_sample_cdr,
  .to_ser_ref = serdata_default_to_ser_ref,
  .to_ser_unref = serdata_default_to_ser_unref,
  .to_untyped = serdata_default_to_untyped,
  .untyped_to_sample = serdata_default_untyped_to_sample_cdr,
  .print = serdata_default_print_cdr,
  .get_keyhash = serdata_default_get_keyhash,
  .from_loaned_sample = serdata_default_from_loaned_sample,
  .from_psmx = serdata_default_from_psmx
};

const struct ddsi_serdata_ops dds_serdata_ops_xcdr2 = {
  .get_size = serdata_default_get_size,
  .eqkey = serdata_default_eqkey,
  .free = serdata_default_free,
  .from_ser = serdata_default_from_ser,
  .from_ser_iov = serdata_default_from_ser_iov,
  .from_keyhash = serdata_default_from_keyhash_cdr,
  .from_sample = serdata_default_from_sample_xcdr2,
  .to_ser = serdata_default_to_ser,
  .to_sample = serdata_default_to_sample_cdr,
  .to_ser_ref = serdata_default_to_ser_ref,
  .to_ser_unref = serdata_default_to_ser_unref,
  .to_untyped = serdata_default_to_untyped,
  .untyped_to_sample = serdata_default_untyped_to_sample_cdr,
  .print = serdata_default_print_cdr,
  .get_keyhash = serdata_default_get_keyhash,
  .from_loaned_sample = serdata_default_from_loaned_sample,
  .from_psmx = serdata_default_from_psmx
};

const struct ddsi_serdata_ops dds_serdata_ops_cdr_nokey = {
  .get_size = serdata_default_get_size,
  .eqkey = serdata_default_eqkey_nokey,
  .free = serdata_default_free,
  .from_ser = serdata_default_from_ser_nokey,
  .from_ser_iov = serdata_default_from_ser_iov_nokey,
  .from_keyhash = serdata_default_from_keyhash_cdr_nokey,
  .from_sample = serdata_default_from_sample_cdr_nokey,
  .to_ser = serdata_default_to_ser,
  .to_sample = serdata_default_to_sample_cdr,
  .to_ser_ref = serdata_default_to_ser_ref,
  .to_ser_unref = serdata_default_to_ser_unref,
  .to_untyped = serdata_default_to_untyped,
  .untyped_to_sample = serdata_default_untyped_to_sample_cdr_nokey,
  .print = serdata_default_print_cdr,
  .get_keyhash = serdata_default_get_keyhash,
  .from_loaned_sample = serdata_default_from_loaned_sample,
  .from_psmx = serdata_default_from_psmx
};

const struct ddsi_serdata_ops dds_serdata_ops_xcdr2_nokey = {
  .get_size = serdata_default_get_size,
  .eqkey = serdata_default_eqkey_nokey,
  .free = serdata_default_free,
  .from_ser = serdata_default_from_ser_nokey,
  .from_ser_iov = serdata_default_from_ser_iov_nokey,
  .from_keyhash = serdata_default_from_keyhash_cdr_nokey,
  .from_sample = serdata_default_from_sample_xcdr2_nokey,
  .to_ser = serdata_default_to_ser,
  .to_sample = serdata_default_to_sample_cdr,
  .to_ser_ref = serdata_default_to_ser_ref,
  .to_ser_unref = serdata_default_to_ser_unref,
  .to_untyped = serdata_default_to_untyped,
  .untyped_to_sample = serdata_default_untyped_to_sample_cdr_nokey,
  .print = serdata_default_print_cdr,
  .get_keyhash = serdata_default_get_keyhash,
  .from_loaned_sample = serdata_default_from_loaned_sample,
  .from_psmx = serdata_default_from_psmx
};

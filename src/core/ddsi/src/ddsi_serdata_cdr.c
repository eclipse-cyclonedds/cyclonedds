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
#include "dds/ddsi/ddsi_domaingv.h"
#include "ddsi__radmin.h"
#include "ddsi__serdata_cdr.h"
#include "dds/cdr/dds_cdrstream.h"

#define DEFAULT_NEW_SIZE 128
#define CHUNK_SIZE 128

#ifndef NDEBUG
static int ispowerof2_size (size_t x)
{
  return x > 0 && !(x & (x-1));
}
#endif

static size_t alignup_size (size_t x, size_t a)
{
  size_t m = a - 1;
  assert (ispowerof2_size (a));
  return (x + m) & ~m;
}

static void *serdata_cdr_append (struct ddsi_serdata_cdr **d, size_t n)
{
  char *p;
  if ((*d)->pos + n > (*d)->size)
  {
    size_t size1 = alignup_size ((*d)->pos + n, CHUNK_SIZE);
    *d = ddsrt_realloc (*d, offsetof (struct ddsi_serdata_cdr, data) + size1);
    (*d)->size = (uint32_t)size1;
  }
  assert ((*d)->pos + n <= (*d)->size);
  p = (*d)->data + (*d)->pos;
  (*d)->pos += (uint32_t)n;
  return p;
}

static void serdata_cdr_append_blob (struct ddsi_serdata_cdr **d, size_t sz, const void *data)
{
  char *p = serdata_cdr_append (d, sz);
  memcpy (p, data, sz);
}

static struct ddsi_serdata *fix_serdata_cdr (struct ddsi_serdata_cdr *d, uint32_t basehash)
{
  d->c.hash = basehash;
  return &d->c;
}

static uint32_t serdata_cdr_get_size (const struct ddsi_serdata *dcmn)
{
  const struct ddsi_serdata_cdr *d = (const struct ddsi_serdata_cdr *) dcmn;
  return d->pos + (uint32_t)sizeof (struct dds_cdr_header);
}

static bool serdata_cdr_eqkey (const struct ddsi_serdata *acmn, const struct ddsi_serdata *bcmn)
{
  (void) acmn;
  (void) bcmn;
  /* Sertype_cdr is only used for types that have no keys, but this function must be implemented
     because it is used by tkmap as the equal function for the hash table, so simply returning
     true here. */
  return true;
}

static void serdata_cdr_free (struct ddsi_serdata *dcmn)
{
  struct ddsi_serdata_cdr *d = (struct ddsi_serdata_cdr *)dcmn;
  assert(ddsrt_atomic_ld32 (&d->c.refc) == 0);
  dds_free (d);
}

static void serdata_cdr_init (struct ddsi_serdata_cdr *d, const struct ddsi_sertype_cdr *tp, enum ddsi_serdata_kind kind)
{
  ddsi_serdata_init (&d->c, &tp->c, kind);
  d->pos = 0;
  d->hdr.identifier = ddsi_sertype_get_native_enc_identifier (DDSI_RTPS_CDR_ENC_VERSION_2, tp->encoding_format);
  d->hdr.options = 0;
}

static struct ddsi_serdata_cdr *serdata_cdr_allocnew (uint32_t init_size)
{
  struct ddsi_serdata_cdr *d = ddsrt_malloc (offsetof (struct ddsi_serdata_cdr, data) + init_size);
  d->size = init_size;
  return d;
}

static struct ddsi_serdata_cdr *serdata_cdr_new_size (const struct ddsi_sertype_cdr *tp, enum ddsi_serdata_kind kind, uint32_t size)
{
  struct ddsi_serdata_cdr *d;
  if ((d = serdata_cdr_allocnew (size)) == NULL)
    return NULL;
  serdata_cdr_init (d, tp, kind);
  return d;
}

static struct ddsi_serdata_cdr *serdata_cdr_new (const struct ddsi_sertype_cdr *tp, enum ddsi_serdata_kind kind)
{
  return serdata_cdr_new_size (tp, kind, DEFAULT_NEW_SIZE);
}

static inline bool is_valid_xcdr_id (unsigned short cdr_identifier)
{
  return (cdr_identifier == DDSI_RTPS_CDR2_LE || cdr_identifier == DDSI_RTPS_CDR2_BE
    || cdr_identifier == DDSI_RTPS_D_CDR2_LE || cdr_identifier == DDSI_RTPS_D_CDR2_BE
    || cdr_identifier == DDSI_RTPS_PL_CDR2_LE || cdr_identifier == DDSI_RTPS_PL_CDR2_BE);
}

/* Construct a serdata from a fragchain received over the network */
static struct ddsi_serdata_cdr *serdata_cdr_from_ser_common (const struct ddsi_sertype *tpcmn, enum ddsi_serdata_kind kind, const struct ddsi_rdata *fragchain, size_t size)
{
  assert (kind == SDK_DATA);
  const struct ddsi_sertype_cdr *tp = (const struct ddsi_sertype_cdr *)tpcmn;

  /* FIXME: check whether this really is the correct maximum: offsets are relative
     to the CDR header, but there are also some places that use a serdata as-if it
     were a stream, and those use offsets (m_index) relative to the start of the
     serdata */
  if (size > UINT32_MAX - offsetof (struct ddsi_serdata_cdr, hdr))
    return NULL;
  struct ddsi_serdata_cdr *d = serdata_cdr_new_size (tp, kind, (uint32_t) size);
  if (d == NULL)
    return NULL;

  uint32_t off = 4; /* must skip the CDR header */

  assert (fragchain->min == 0);
  assert (fragchain->maxp1 >= off); /* CDR header must be in first fragment */

  memcpy (&d->hdr, DDSI_RMSG_PAYLOADOFF (fragchain->rmsg, DDSI_RDATA_PAYLOAD_OFF (fragchain)), sizeof (d->hdr));
  if (!is_valid_xcdr_id (d->hdr.identifier))
    goto err;

  while (fragchain)
  {
    assert (fragchain->min <= off);
    assert (fragchain->maxp1 <= size);
    if (fragchain->maxp1 > off)
    {
      /* only copy if this fragment adds data */
      const unsigned char *payload = DDSI_RMSG_PAYLOADOFF (fragchain->rmsg, DDSI_RDATA_PAYLOAD_OFF (fragchain));
      serdata_cdr_append_blob (&d, fragchain->maxp1 - off, payload + off - fragchain->min);
      off = fragchain->maxp1;
    }
    fragchain = fragchain->nextfrag;
  }

  const bool needs_bswap = !DDSI_RTPS_CDR_ENC_IS_NATIVE (d->hdr.identifier);
  d->hdr.identifier = DDSI_RTPS_CDR_ENC_TO_NATIVE (d->hdr.identifier);
  const uint32_t pad = ddsrt_fromBE2u (d->hdr.options) & DDS_CDR_HDR_PADDING_MASK;
  const uint32_t xcdr_version = ddsi_sertype_enc_id_xcdr_version (d->hdr.identifier);
  const uint32_t encoding_format = ddsi_sertype_enc_id_enc_format (d->hdr.identifier);
  if (xcdr_version != DDSI_RTPS_CDR_ENC_VERSION_2 || encoding_format != tp->encoding_format)
    goto err;

  uint32_t actual_size;
  if (d->pos < pad || !dds_stream_normalize (d->data, d->pos - pad, needs_bswap, DDSI_RTPS_CDR_ENC_VERSION_2, &tp->type, false, &actual_size))
    goto err;

  dds_istream_t is;
  dds_istream_init (&is, actual_size, d->data, DDSI_RTPS_CDR_ENC_VERSION_2);
  return d;

err:
  ddsi_serdata_unref (&d->c);
  return NULL;
}

static struct ddsi_serdata *serdata_cdr_from_ser (const struct ddsi_sertype *tpcmn, enum ddsi_serdata_kind kind, const struct ddsi_rdata *fragchain, size_t size)
{
  struct ddsi_serdata_cdr *d;
  if ((d = serdata_cdr_from_ser_common (tpcmn, kind, fragchain, size)) == NULL)
    return NULL;
  return fix_serdata_cdr (d, tpcmn->serdata_basehash);
}

static void istream_from_serdata_cdr (dds_istream_t * __restrict s, const struct ddsi_serdata_cdr * __restrict d)
{
  s->m_buffer = (const unsigned char *) d;
  s->m_index = (uint32_t) offsetof (struct ddsi_serdata_cdr, data);
  s->m_size = d->size + s->m_index;
#if DDSRT_ENDIAN == DDSRT_LITTLE_ENDIAN
  assert (DDSI_RTPS_CDR_ENC_LE (d->hdr.identifier));
#elif DDSRT_ENDIAN == DDSRT_BIG_ENDIAN
  assert (!DDSI_RTPS_CDR_ENC_LE (d->hdr.identifier));
#endif
  s->m_xcdr_version = ddsi_sertype_enc_id_xcdr_version (d->hdr.identifier);
}

static void ostream_from_serdata_cdr (dds_ostream_t * __restrict s, const struct ddsi_serdata_cdr * __restrict d)
{
  s->m_buffer = (unsigned char *) d;
  s->m_index = (uint32_t) offsetof (struct ddsi_serdata_cdr, data);
  s->m_size = d->size + s->m_index;
#if DDSRT_ENDIAN == DDSRT_LITTLE_ENDIAN
  assert (DDSI_RTPS_CDR_ENC_LE (d->hdr.identifier));
#elif DDSRT_ENDIAN == DDSRT_BIG_ENDIAN
  assert (!DDSI_RTPS_CDR_ENC_LE (d->hdr.identifier));
#endif
  s->m_xcdr_version = ddsi_sertype_enc_id_xcdr_version (d->hdr.identifier);
}

static void ostream_add_to_serdata_cdr (dds_ostream_t * __restrict s, struct ddsi_serdata_cdr ** __restrict d)
{
  /* DDSI requires 4 byte alignment */
  const uint32_t pad = dds_cdr_alignto4_clear_and_resize (s, &dds_cdrstream_default_allocator, s->m_xcdr_version);
  assert (pad <= 3);

  /* Reset data pointer as stream may have reallocated */
  (*d) = (void *) s->m_buffer;
  (*d)->pos = (s->m_index - (uint32_t) offsetof (struct ddsi_serdata_cdr, data));
  (*d)->size = (s->m_size - (uint32_t) offsetof (struct ddsi_serdata_cdr, data));
  (*d)->hdr.options = ddsrt_toBE2u ((uint16_t) pad);
}


static struct ddsi_serdata_cdr *serdata_cdr_from_sample_cdr_common (const struct ddsi_sertype *tpcmn, enum ddsi_serdata_kind kind, const void *sample)
{
  const struct ddsi_sertype_cdr *tp = (const struct ddsi_sertype_cdr *)tpcmn;
  struct ddsi_serdata_cdr *d = serdata_cdr_new (tp, kind);
  if (d == NULL)
    return NULL;

  dds_ostream_t os;
  ostream_from_serdata_cdr (&os, d);
  switch (kind)
  {
    case SDK_EMPTY:
      ostream_add_to_serdata_cdr (&os, &d);
      break;
    case SDK_KEY:
      abort ();
      break;
    case SDK_DATA:
      if (!dds_stream_write_sample (&os, &dds_cdrstream_default_allocator, sample, &tp->type))
        return NULL;
      ostream_add_to_serdata_cdr (&os, &d);
      break;
  }
  return d;
}

static struct ddsi_serdata *serdata_cdr_from_sample (const struct ddsi_sertype *tpcmn, enum ddsi_serdata_kind kind, const void *sample)
{
  struct ddsi_serdata_cdr *d;
  if ((d = serdata_cdr_from_sample_cdr_common (tpcmn, kind, sample)) == NULL)
    return NULL;
  return fix_serdata_cdr (d, tpcmn->serdata_basehash);
}

static struct ddsi_serdata *serdata_cdr_to_untyped (const struct ddsi_serdata *serdata_common)
{
  const struct ddsi_serdata_cdr *d = (const struct ddsi_serdata_cdr *)serdata_common;
  const struct ddsi_sertype_cdr *tp = (const struct ddsi_sertype_cdr *)d->c.type;

  assert (DDSI_RTPS_CDR_ENC_IS_NATIVE (d->hdr.identifier));
  struct ddsi_serdata_cdr *d_tl = serdata_cdr_new (tp, SDK_KEY);
  if (d_tl == NULL)
    return NULL;
  d_tl->c.type = NULL;
  d_tl->c.hash = d->c.hash;
  d_tl->c.timestamp.v = INT64_MIN;
  return (struct ddsi_serdata *) d_tl;
}

/* Fill buffer with 'size' bytes of serialised data, starting from 'off'; 0 <= off < off+sz <= alignup4(size(d)) */
static void serdata_cdr_to_ser (const struct ddsi_serdata *serdata_common, size_t off, size_t sz, void *buf)
{
  const struct ddsi_serdata_cdr *d = (const struct ddsi_serdata_cdr *)serdata_common;
  assert (off < d->pos + sizeof(struct dds_cdr_header));
  assert (sz <= alignup_size (d->pos + sizeof(struct dds_cdr_header), 4) - off);
  memcpy (buf, (char *) &d->hdr + off, sz);
}

static struct ddsi_serdata *serdata_cdr_to_ser_ref (const struct ddsi_serdata *serdata_common, size_t off, size_t sz, ddsrt_iovec_t *ref)
{
  const struct ddsi_serdata_cdr *d = (const struct ddsi_serdata_cdr *)serdata_common;
  assert (off < d->pos + sizeof(struct dds_cdr_header));
  assert (sz <= alignup_size (d->pos + sizeof(struct dds_cdr_header), 4) - off);
  ref->iov_base = (char *)&d->hdr + off;
  ref->iov_len = (ddsrt_iov_len_t)sz;
  return ddsi_serdata_ref (serdata_common);
}

static void serdata_cdr_to_ser_unref (struct ddsi_serdata *serdata_common, const ddsrt_iovec_t *ref)
{
  (void)ref;
  ddsi_serdata_unref(serdata_common);
}

static bool serdata_cdr_to_sample_cdr (const struct ddsi_serdata *serdata_common, void *sample, void **bufptr, void *buflim)
{
  const struct ddsi_serdata_cdr *d = (const struct ddsi_serdata_cdr *)serdata_common;
  const struct ddsi_sertype_cdr *tp = (const struct ddsi_sertype_cdr *) d->c.type;
  dds_istream_t is;
  if (bufptr) abort(); else { (void)buflim; } /* FIXME: haven't implemented that bit yet! */
  assert (DDSI_RTPS_CDR_ENC_IS_NATIVE (d->hdr.identifier));
  istream_from_serdata_cdr(&is, d);
  dds_stream_read_sample (&is, sample, &dds_cdrstream_default_allocator, &tp->type);
  return true; /* FIXME: can't conversion to sample fail? */
}

static bool serdata_cdr_untyped_to_sample_cdr (const struct ddsi_sertype *sertype_common, const struct ddsi_serdata *serdata_common, void *sample, void **bufptr, void *buflim)
{
  (void)sertype_common; (void)sample; (void)bufptr; (void)buflim; (void)serdata_common;
  assert (serdata_common->type == NULL);
  assert (serdata_common->kind == SDK_KEY);
  return true;
}

static size_t serdata_cdr_print_cdr (const struct ddsi_sertype *sertype_common, const struct ddsi_serdata *serdata_common, char *buf, size_t size)
{
  const struct ddsi_serdata_cdr *d = (const struct ddsi_serdata_cdr *)serdata_common;
  const struct ddsi_sertype_cdr *tp = (const struct ddsi_sertype_cdr *)sertype_common;
  dds_istream_t is;
  istream_from_serdata_cdr (&is, d);
  return dds_stream_print_sample (&is, &tp->type, buf, size);
}

const struct ddsi_serdata_ops ddsi_serdata_ops_cdr = {
  .get_size = serdata_cdr_get_size,
  .eqkey = serdata_cdr_eqkey,
  .free = serdata_cdr_free,
  .from_ser = serdata_cdr_from_ser,
  .from_ser_iov = 0,
  .from_keyhash = 0,
  .from_sample = serdata_cdr_from_sample,
  .to_ser = serdata_cdr_to_ser,
  .to_sample = serdata_cdr_to_sample_cdr,
  .to_ser_ref = serdata_cdr_to_ser_ref,
  .to_ser_unref = serdata_cdr_to_ser_unref,
  .to_untyped = serdata_cdr_to_untyped,
  .untyped_to_sample = serdata_cdr_untyped_to_sample_cdr,
  .print = serdata_cdr_print_cdr,
  .get_keyhash = 0,
#ifdef DDS_HAS_SHM
  .get_sample_size = 0,
  .from_iox_buffer = 0
#endif
};

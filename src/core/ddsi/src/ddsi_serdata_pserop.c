// Copyright(c) 2020 to 2022 ZettaScale Technology and others
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
#include "ddsi__serdata_pserop.h"
#include "dds/cdr/dds_cdrstream.h"

static uint32_t serdata_pserop_get_size (const struct ddsi_serdata *dcmn)
{
  const struct ddsi_serdata_pserop *d = (const struct ddsi_serdata_pserop *) dcmn;
  return 4 + d->pos; // FIXME: +4 for CDR header should be eliminated
}

static bool serdata_pserop_eqkey (const struct ddsi_serdata *acmn, const struct ddsi_serdata *bcmn)
{
  const struct ddsi_serdata_pserop *a = (const struct ddsi_serdata_pserop *) acmn;
  const struct ddsi_serdata_pserop *b = (const struct ddsi_serdata_pserop *) bcmn;
  if (a->keyless != b->keyless)
    return false;
  else if (a->keyless)
    return true;
  else
    return memcmp (a->sample, b->sample, 16) == 0;
}

static void serdata_pserop_free (struct ddsi_serdata *dcmn)
{
  struct ddsi_serdata_pserop *d = (struct ddsi_serdata_pserop *) dcmn;
  const struct ddsi_sertype_pserop *tp = (const struct ddsi_sertype_pserop *) d->c.type;
  if (d->c.kind == SDK_DATA)
    ddsi_plist_fini_generic (d->sample, tp->ops, true);
  if (d->sample)
    ddsrt_free (d->sample);
  ddsrt_free (d);
}

static struct ddsi_serdata_pserop *serdata_pserop_new (const struct ddsi_sertype_pserop *tp, enum ddsi_serdata_kind kind, size_t size, const void *cdr_header)
{
  /* FIXME: check whether this really is the correct maximum: offsets are relative
     to the CDR header, but there are also some places that use a serdata as-if it
     were a stream, and those use offsets (m_index) relative to the start of the
     serdata */
  assert (kind != SDK_EMPTY);
  if (size < 4 || size > UINT32_MAX - offsetof (struct ddsi_serdata_pserop, identifier))
    return NULL;
  struct ddsi_serdata_pserop *d = ddsrt_malloc (sizeof (*d) + size);
  if (d == NULL)
    return NULL;
  ddsi_serdata_init (&d->c, &tp->c, kind);
  d->keyless = (tp->ops_key == NULL);
  d->pos = 0;
  d->size = (uint32_t) size;
  const uint16_t *hdrsrc = cdr_header;
  d->identifier = hdrsrc[0];
  d->options = hdrsrc[1];
  assert (d->identifier == DDSI_RTPS_CDR_LE || d->identifier == DDSI_RTPS_CDR_BE);
  if (kind == SDK_KEY && d->keyless)
    d->sample = NULL;
  else if ((d->sample = ddsrt_malloc ((kind == SDK_DATA) ? tp->memsize : 16)) == NULL)
  {
    ddsrt_free (d);
    return NULL;
  }
  return d;
}

static struct ddsi_serdata *serdata_pserop_fix (const struct ddsi_sertype_pserop *tp, struct ddsi_serdata_pserop *d)
{
  const bool needs_bswap = !DDSI_RTPS_CDR_ENC_IS_NATIVE (d->identifier);
  const enum ddsi_pserop *ops = (d->c.kind == SDK_DATA) ? tp->ops : tp->ops_key;
  d->c.hash = tp->c.serdata_basehash;
  if (ops != NULL)
  {
    assert (d->pos >= 16 && tp->memsize >= 16);
    if (ddsi_plist_deser_generic (d->sample, d->data, d->pos, needs_bswap, (d->c.kind == SDK_DATA) ? tp->ops : tp->ops_key) < 0)
    {
      ddsrt_free (d->sample);
      ddsrt_free (d);
      return NULL;
    }
    if (tp->ops_key)
    {
      assert (d->pos >= 16 && tp->memsize >= 16);
      d->c.hash ^= ddsrt_mh3 (d->sample, 16, 0);
    }
  }
  return &d->c;
}

static struct ddsi_serdata *serdata_pserop_from_ser (const struct ddsi_sertype *tpcmn, enum ddsi_serdata_kind kind, const struct ddsi_rdata *fragchain, size_t size)
{
  const struct ddsi_sertype_pserop *tp = (const struct ddsi_sertype_pserop *)tpcmn;
  struct ddsi_serdata_pserop *d = serdata_pserop_new (tp, kind, size, DDSI_RMSG_PAYLOADOFF (fragchain->rmsg, DDSI_RDATA_PAYLOAD_OFF (fragchain)));
  if (d == NULL)
    return NULL;
  uint32_t off = 4; /* must skip the CDR header */
  assert (fragchain->min == 0);
  assert (fragchain->maxp1 >= off); /* CDR header must be in first fragment */
  while (fragchain)
  {
    assert (fragchain->min <= off);
    assert (fragchain->maxp1 <= size);
    if (fragchain->maxp1 > off)
    {
      /* only copy if this fragment adds data */
      const unsigned char *payload = DDSI_RMSG_PAYLOADOFF (fragchain->rmsg, DDSI_RDATA_PAYLOAD_OFF (fragchain));
      uint32_t n = fragchain->maxp1 - off;
      memcpy (d->data + d->pos, payload + off - fragchain->min, n);
      d->pos += n;
      off = fragchain->maxp1;
    }
    fragchain = fragchain->nextfrag;
  }
  return serdata_pserop_fix (tp, d);
}

static struct ddsi_serdata *serdata_pserop_from_ser_iov (const struct ddsi_sertype *tpcmn, enum ddsi_serdata_kind kind, ddsrt_msg_iovlen_t niov, const ddsrt_iovec_t *iov, size_t size)
{
  const struct ddsi_sertype_pserop *tp = (const struct ddsi_sertype_pserop *)tpcmn;
  assert (niov >= 1);
  struct ddsi_serdata_pserop *d = serdata_pserop_new (tp, kind, size, iov[0].iov_base);
  if (d == NULL)
    return NULL;
  const uint16_t *hdrsrc = (uint16_t *) iov[0].iov_base;
  d->identifier = hdrsrc[0];
  d->options = hdrsrc[1];
  assert (d->identifier == DDSI_RTPS_CDR_LE || d->identifier == DDSI_RTPS_CDR_BE);
  memcpy (d->data + d->pos, (const char *) iov[0].iov_base + 4, iov[0].iov_len - 4);
  d->pos += (uint32_t) iov[0].iov_len - 4;
  for (ddsrt_msg_iovlen_t i = 1; i < niov; i++)
  {
    memcpy (d->data + d->pos, (const char *) iov[i].iov_base, iov[i].iov_len);
    d->pos += (uint32_t) iov[i].iov_len;
  }
  return serdata_pserop_fix (tp, d);
}

static struct ddsi_serdata *serdata_pserop_from_keyhash (const struct ddsi_sertype *tpcmn, const ddsi_keyhash_t *keyhash)
{
  const struct { uint16_t identifier, options; ddsi_keyhash_t kh; } in = { DDSI_RTPS_CDR_BE, 0, *keyhash };
  const ddsrt_iovec_t iov = { .iov_base = (void *) &in, .iov_len = sizeof (in) };
  return serdata_pserop_from_ser_iov (tpcmn, SDK_KEY, 1, &iov, sizeof (in) - 4);
}

static bool serdata_pserop_to_sample (const struct ddsi_serdata *serdata_common, void *sample, void **bufptr, void *buflim)
{
  const struct ddsi_serdata_pserop *d = (const struct ddsi_serdata_pserop *)serdata_common;
  const struct ddsi_sertype_pserop *tp = (const struct ddsi_sertype_pserop *) d->c.type;
  if (bufptr) abort(); else { (void)buflim; } /* FIXME: haven't implemented that bit yet! */
  if (d->c.kind == SDK_KEY)
    memcpy (sample, d->sample, 16);
  else
  {
    const bool needs_bswap = !DDSI_RTPS_CDR_ENC_IS_NATIVE (d->identifier);
    dds_return_t ret = ddsi_plist_deser_generic (sample, d->data, d->pos, needs_bswap, tp->ops);
    ddsi_plist_unalias_generic (sample, tp->ops);
    assert (ret >= 0);
    (void) ret;
  }
  return true; /* FIXME: can't conversion to sample fail? */
}

static void serdata_pserop_to_ser (const struct ddsi_serdata *serdata_common, size_t off, size_t sz, void *buf)
{
  const struct ddsi_serdata_pserop *d = (const struct ddsi_serdata_pserop *)serdata_common;
  memcpy (buf, (char *) &d->identifier + off, sz);
}

static struct ddsi_serdata *serdata_pserop_to_ser_ref (const struct ddsi_serdata *serdata_common, size_t off, size_t sz, ddsrt_iovec_t *ref)
{
  const struct ddsi_serdata_pserop *d = (const struct ddsi_serdata_pserop *)serdata_common;
  ref->iov_base = (char *) &d->identifier + off;
  ref->iov_len = (ddsrt_iov_len_t) sz;
  return ddsi_serdata_ref (serdata_common);
}

static void serdata_pserop_to_ser_unref (struct ddsi_serdata *serdata_common, const ddsrt_iovec_t *ref)
{
  (void) ref;
  ddsi_serdata_unref (serdata_common);
}

static struct ddsi_serdata *serdata_pserop_from_sample (const struct ddsi_sertype *tpcmn, enum ddsi_serdata_kind kind, const void *sample)
{
  const struct ddsi_sertype_pserop *tp = (const struct ddsi_sertype_pserop *)tpcmn;
  const struct { uint16_t identifier, options; } header = { ddsi_sertype_get_native_enc_identifier (DDSI_RTPS_CDR_ENC_VERSION_1, tp->encoding_format), 0 };
  struct ddsi_serdata_pserop *d;
  if (kind == SDK_KEY && tp->ops_key == NULL)
  {
    if ((d = serdata_pserop_new (tp, kind, 0, &header)) == NULL)
      return NULL;
  }
  else
  {
    void *data;
    size_t size;
    if (ddsi_plist_ser_generic (&data, &size, sample, (kind == SDK_DATA) ? tp->ops : tp->ops_key) < 0)
      return NULL;
    const size_t size4 = (size + 3) & ~(size_t)3;
    if ((d = serdata_pserop_new (tp, kind, size4, &header)) == NULL)
    {
      ddsrt_free (data);
      return NULL;
    }
    assert (tp->ops_key == NULL || (size >= 16 && tp->memsize >= 16));
    assert (d->data != NULL); // clang static analyzer
    memcpy (d->data, data, size);
    memset (d->data + size, 0, size4 - size);
    d->pos = (uint32_t) size;
    ddsrt_free (data); // FIXME: shouldn't allocate twice & copy
    // FIXME: and then this silly thing deserialises it immediately again -- perhaps it should be a bit lazier
  }
  return serdata_pserop_fix (tp, d);
}

static struct ddsi_serdata *serdata_pserop_to_untyped (const struct ddsi_serdata *serdata_common)
{
  const struct ddsi_serdata_pserop *d = (const struct ddsi_serdata_pserop *)serdata_common;
  const struct ddsi_sertype_pserop *tp = (const struct ddsi_sertype_pserop *)d->c.type;
  ddsrt_iovec_t iov = { .iov_base = (char *) &d->identifier, .iov_len = (ddsrt_iov_len_t) (4 + d->pos) };
  struct ddsi_serdata *dcmn_tl = serdata_pserop_from_ser_iov (&tp->c, SDK_KEY, 1, &iov, iov.iov_len);
  assert (dcmn_tl != NULL);
  dcmn_tl->type = NULL;
  return dcmn_tl;
}

static bool serdata_pserop_untyped_to_sample (const struct ddsi_sertype *type_common, const struct ddsi_serdata *serdata_common, void *sample, void **bufptr, void *buflim)
{
  const struct ddsi_serdata_pserop *d = (const struct ddsi_serdata_pserop *)serdata_common;
  const struct ddsi_sertype_pserop *tp = (const struct ddsi_sertype_pserop *)type_common;
  if (bufptr) abort(); else { (void)buflim; } /* FIXME: haven't implemented that bit yet! */
  if (tp->ops_key)
    memcpy (sample, d->sample, 16);
  return true;
}

static void serdata_pserop_get_keyhash (const struct ddsi_serdata *serdata_common, struct ddsi_keyhash *buf, bool force_md5)
{
  const struct ddsi_serdata_pserop *d = (const struct ddsi_serdata_pserop *)serdata_common;
  const struct ddsi_sertype_pserop *tp = (const struct ddsi_sertype_pserop *)d->c.type;
  if (tp->ops_key == NULL)
    memset (buf, 0, 16);
  else
  {
    /* need big-endian representation for key hash, so be lazy & re-serialize
       (and yes, it costs another malloc ...); note that key at offset 0 implies
       ops_key is a prefix of ops */
    void *be;
    size_t besize;
    (void) ddsi_plist_ser_generic_be (&be, &besize, d->sample, tp->ops_key);
    assert (besize == 16); /* that's the deal with keys for now */
    if (!force_md5)
      memcpy (buf, be, 16);
    else
    {
      ddsrt_md5_state_t md5st;
      ddsrt_md5_init (&md5st);
      ddsrt_md5_append (&md5st, (ddsrt_md5_byte_t *) be, 16);
      ddsrt_md5_finish (&md5st, (ddsrt_md5_byte_t *) buf->value);
    }
    ddsrt_free (be);
  }
}

static size_t serdata_pserop_print_pserop (const struct ddsi_sertype *sertype_common, const struct ddsi_serdata *serdata_common, char *buf, size_t size)
{
  const struct ddsi_serdata_pserop *d = (const struct ddsi_serdata_pserop *)serdata_common;
  const struct ddsi_sertype_pserop *tp = (const struct ddsi_sertype_pserop *)sertype_common;
  return ddsi_plist_print_generic (buf, size, d->sample, tp->ops);
}

const struct ddsi_serdata_ops ddsi_serdata_ops_pserop = {
  .get_size = serdata_pserop_get_size,
  .eqkey = serdata_pserop_eqkey,
  .free = serdata_pserop_free,
  .from_ser = serdata_pserop_from_ser,
  .from_ser_iov = serdata_pserop_from_ser_iov,
  .from_keyhash = serdata_pserop_from_keyhash,
  .from_sample = serdata_pserop_from_sample,
  .to_ser = serdata_pserop_to_ser,
  .to_sample = serdata_pserop_to_sample,
  .to_ser_ref = serdata_pserop_to_ser_ref,
  .to_ser_unref = serdata_pserop_to_ser_unref,
  .to_untyped = serdata_pserop_to_untyped,
  .untyped_to_sample = serdata_pserop_untyped_to_sample,
  .print = serdata_pserop_print_pserop,
  .get_keyhash = serdata_pserop_get_keyhash
};

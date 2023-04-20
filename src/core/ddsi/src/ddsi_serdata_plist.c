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
#include "ddsi__serdata_plist.h"
#include "ddsi__xmsg.h"
#include "ddsi__misc.h"
#include "ddsi__plist.h"
#include "ddsi__protocol.h"
#include "ddsi__vendor.h"
#include "dds/cdr/dds_cdrstream.h"

static uint32_t serdata_plist_get_size (const struct ddsi_serdata *dcmn)
{
  const struct ddsi_serdata_plist *d = (const struct ddsi_serdata_plist *) dcmn;
  return 4 + d->pos; // FIXME: +4 for CDR header should be eliminated
}

static bool serdata_plist_eqkey (const struct ddsi_serdata *acmn, const struct ddsi_serdata *bcmn)
{
  const struct ddsi_serdata_plist *a = (const struct ddsi_serdata_plist *) acmn;
  const struct ddsi_serdata_plist *b = (const struct ddsi_serdata_plist *) bcmn;
  return memcmp (&a->keyhash, &b->keyhash, sizeof (a->keyhash)) == 0;
}

static void serdata_plist_free (struct ddsi_serdata *dcmn)
{
  struct ddsi_serdata_plist *d = (struct ddsi_serdata_plist *) dcmn;
  ddsrt_free (d);
}

static struct ddsi_serdata_plist *serdata_plist_new (const struct ddsi_sertype_plist *tp, enum ddsi_serdata_kind kind, size_t size, const void *cdr_header)
{
  /* FIXME: check whether this really is the correct maximum: offsets are relative
     to the CDR header, but there are also some places that use a serdata as-if it
     were a stream, and those use offsets (m_index) relative to the start of the
     serdata */
  if (size < 4 || size > UINT32_MAX - offsetof (struct ddsi_serdata_plist, identifier))
    return NULL;
  struct ddsi_serdata_plist *d = ddsrt_malloc (sizeof (*d) + size);
  if (d == NULL)
    return NULL;
  ddsi_serdata_init (&d->c, &tp->c, kind);
  d->pos = 0;
  d->size = (uint32_t) size;
  // FIXME: vendorid/protoversion are not available when creating a serdata
  // these should be overruled by the one creating the serdata
  d->vendorid = DDSI_VENDORID_UNKNOWN;
  d->protoversion.major = DDSI_RTPS_MAJOR;
  d->protoversion.minor = DDSI_RTPS_MINOR;
  const uint16_t *hdrsrc = cdr_header;
  d->identifier = hdrsrc[0];
  d->options = hdrsrc[1];
  if (d->identifier != DDSI_RTPS_PL_CDR_LE && d->identifier != DDSI_RTPS_PL_CDR_BE)
  {
    ddsrt_free (d);
    return NULL;
  }
  return d;
}

static struct ddsi_serdata *serdata_plist_fix (const struct ddsi_sertype_plist *tp, struct ddsi_serdata_plist *d)
{
  assert (tp->keyparam != DDSI_PID_SENTINEL);
  void *needlep;
  size_t needlesz;
  if (ddsi_plist_findparam_checking (d->data, d->pos, d->identifier, tp->keyparam, &needlep, &needlesz) != DDS_RETCODE_OK)
  {
    ddsrt_free (d);
    return NULL;
  }
  assert (needlep);
  if (needlesz != sizeof (d->keyhash))
  {
    ddsrt_free (d);
    return NULL;
  }
  memcpy (&d->keyhash, needlep, 16);
  d->c.hash = ddsrt_mh3 (&d->keyhash, sizeof (d->keyhash), 0) ^ tp->c.serdata_basehash;
  return &d->c;
}

static struct ddsi_serdata *serdata_plist_from_ser (const struct ddsi_sertype *tpcmn, enum ddsi_serdata_kind kind, const struct ddsi_rdata *fragchain, size_t size)
{
  const struct ddsi_sertype_plist *tp = (const struct ddsi_sertype_plist *) tpcmn;
  struct ddsi_serdata_plist *d = serdata_plist_new (tp, kind, size, DDSI_RMSG_PAYLOADOFF (fragchain->rmsg, DDSI_RDATA_PAYLOAD_OFF (fragchain)));
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
  return serdata_plist_fix (tp, d);
}

static struct ddsi_serdata *serdata_plist_from_ser_iov (const struct ddsi_sertype *tpcmn, enum ddsi_serdata_kind kind, ddsrt_msg_iovlen_t niov, const ddsrt_iovec_t *iov, size_t size)
{
  const struct ddsi_sertype_plist *tp = (const struct ddsi_sertype_plist *) tpcmn;
  assert (niov >= 1);
  struct ddsi_serdata_plist *d = serdata_plist_new (tp, kind, size, iov[0].iov_base);
  if (d == NULL)
    return NULL;
  memcpy (d->data + d->pos, (const char *) iov[0].iov_base + 4, iov[0].iov_len - 4);
  d->pos += (uint32_t) iov[0].iov_len - 4;
  for (ddsrt_msg_iovlen_t i = 1; i < niov; i++)
  {
    memcpy (d->data + d->pos, (const char *) iov[i].iov_base, iov[i].iov_len);
    d->pos += (uint32_t) iov[i].iov_len;
  }
  return serdata_plist_fix (tp, d);
}

static struct ddsi_serdata *serdata_plist_from_keyhash (const struct ddsi_sertype *tpcmn, const ddsi_keyhash_t *keyhash)
{
  const struct ddsi_sertype_plist *tp = (const struct ddsi_sertype_plist *) tpcmn;
  const struct { uint16_t identifier, options; ddsi_parameter_t par; ddsi_keyhash_t kh; ddsi_parameter_t sentinel; } in = {
    .identifier = DDSI_RTPS_PL_CDR_BE,
    .options = 0,
    .par = {
      .parameterid = ddsrt_toBE2u (tp->keyparam),
      .length = ddsrt_toBE2u ((uint16_t) sizeof (*keyhash))
    },
    .kh = *keyhash,
    .sentinel = {
      .parameterid = ddsrt_toBE2u (DDSI_PID_SENTINEL),
      .length = 0
    }
  };
  const ddsrt_iovec_t iov = { .iov_base = (void *) &in, .iov_len = sizeof (in) };
  return serdata_plist_from_ser_iov (tpcmn, SDK_KEY, 1, &iov, sizeof (in) - 4);
}

static enum ddsi_plist_context_kind get_plist_context_kind (ddsi_parameterid_t keypid)
{
  switch (keypid)
  {
    case DDSI_PID_PARTICIPANT_GUID:
      return DDSI_PLIST_CONTEXT_PARTICIPANT;
    case DDSI_PID_ENDPOINT_GUID:
    case DDSI_PID_ADLINK_ENDPOINT_GUID:
      return DDSI_PLIST_CONTEXT_ENDPOINT;
    case DDSI_PID_CYCLONE_TOPIC_GUID:
      return DDSI_PLIST_CONTEXT_TOPIC;
    default:
      return DDSI_PLIST_CONTEXT_INLINE_QOS;
  }
}

static bool serdata_plist_untyped_to_sample (const struct ddsi_sertype *tpcmn, const struct ddsi_serdata *serdata_common, void *sample, void **bufptr, void *buflim)
{
  const struct ddsi_serdata_plist *d = (const struct ddsi_serdata_plist *)serdata_common;
  const struct ddsi_sertype_plist *tp = (const struct ddsi_sertype_plist *)tpcmn;
  struct ddsi_domaingv * const gv = ddsrt_atomic_ldvoidp (&tp->c.gv);
  if (bufptr) abort(); else { (void)buflim; } /* FIXME: haven't implemented that bit yet! */
  ddsi_plist_src_t src = {
    .buf = (const unsigned char *) d->data,
    .bufsz = d->pos,
    .encoding = d->identifier,
    .protocol_version = d->protoversion,
    .strict = DDSI_SC_STRICT_P (gv->config),
    .vendorid = d->vendorid
  };
  const enum ddsi_plist_context_kind context_kind = get_plist_context_kind (tp->keyparam);
  const dds_return_t rc = ddsi_plist_init_frommsg (sample, NULL, ~(uint64_t)0, ~(uint64_t)0, &src, gv, context_kind);
  // FIXME: need a more informative return type
  if (rc != DDS_RETCODE_OK && rc != DDS_RETCODE_UNSUPPORTED)
    GVWARNING ("Invalid %s (vendor %u.%u): invalid qos/parameters\n", tpcmn->type_name, src.vendorid.id[0], src.vendorid.id[1]);
  return (rc == DDS_RETCODE_OK);
}

static bool serdata_plist_to_sample (const struct ddsi_serdata *serdata_common, void *sample, void **bufptr, void *buflim)
{
  /* the "plist" topics only differ in the parameter that is used as the key value */
  return serdata_plist_untyped_to_sample (serdata_common->type, serdata_common, sample, bufptr, buflim);
}

static void serdata_plist_to_ser (const struct ddsi_serdata *serdata_common, size_t off, size_t sz, void *buf)
{
  const struct ddsi_serdata_plist *d = (const struct ddsi_serdata_plist *)serdata_common;
  memcpy (buf, (char *) &d->identifier + off, sz);
}

static struct ddsi_serdata *serdata_plist_to_ser_ref (const struct ddsi_serdata *serdata_common, size_t off, size_t sz, ddsrt_iovec_t *ref)
{
  const struct ddsi_serdata_plist *d = (const struct ddsi_serdata_plist *)serdata_common;
  ref->iov_base = (char *) &d->identifier + off;
  ref->iov_len = (ddsrt_iov_len_t) sz;
  return ddsi_serdata_ref (serdata_common);
}

static void serdata_plist_to_ser_unref (struct ddsi_serdata *serdata_common, const ddsrt_iovec_t *ref)
{
  (void) ref;
  ddsi_serdata_unref (serdata_common);
}

static struct ddsi_serdata *serdata_plist_from_sample (const struct ddsi_sertype *tpcmn, enum ddsi_serdata_kind kind, const void *sample)
{
  const struct ddsi_sertype_plist *tp = (const struct ddsi_sertype_plist *)tpcmn;
  const struct { uint16_t identifier, options; } header = { ddsi_sertype_get_native_enc_identifier (DDSI_RTPS_CDR_ENC_VERSION_1, tp->encoding_format), 0 };

  // FIXME: key must not require byteswapping (GUIDs are ok)
  // FIXME: rework plist stuff so it doesn't need an ddsi_xmsg
  struct ddsi_domaingv * const gv = ddsrt_atomic_ldvoidp (&tp->c.gv);
  struct ddsi_xmsg *mpayload = ddsi_xmsg_new (gv->xmsgpool, &ddsi_nullguid, NULL, 0, DDSI_XMSG_KIND_DATA);
  memcpy (ddsi_xmsg_append (mpayload, NULL, 4), &header, 4);
  const enum ddsi_plist_context_kind context_kind = get_plist_context_kind (tp->keyparam);
  ddsi_plist_addtomsg (mpayload, sample, ~(uint64_t)0, ~(uint64_t)0, context_kind);
  ddsi_xmsg_addpar_sentinel (mpayload);

  size_t sz;
  unsigned char *blob = ddsi_xmsg_payload (&sz, mpayload);
#ifndef NDEBUG
  void *needle;
  size_t needlesz;
  assert (ddsi_plist_findparam_checking (blob + 4, sz, header.identifier, tp->keyparam, &needle, &needlesz) == DDS_RETCODE_OK);
  assert (needle && needlesz == 16);
#endif
  ddsrt_iovec_t iov = { .iov_base = blob, .iov_len = (ddsrt_iov_len_t) sz };
  struct ddsi_serdata *d = serdata_plist_from_ser_iov (tpcmn, kind, 1, &iov, sz - 4);
  ddsi_xmsg_free (mpayload);

  /* we know the vendor when we construct a serdata from a sample */
  struct ddsi_serdata_plist *d_plist = (struct ddsi_serdata_plist *) d;
  d_plist->vendorid = DDSI_VENDORID_ECLIPSE;
  return d;
}

static struct ddsi_serdata *serdata_plist_to_untyped (const struct ddsi_serdata *serdata_common)
{
  const struct ddsi_serdata_plist *d = (const struct ddsi_serdata_plist *) serdata_common;
  const struct ddsi_sertype_plist *tp = (const struct ddsi_sertype_plist *) d->c.type;
  ddsrt_iovec_t iov = { .iov_base = (char *) &d->identifier, .iov_len = 4 + d->pos };
  struct ddsi_serdata *dcmn_tl = serdata_plist_from_ser_iov (&tp->c, SDK_KEY, 1, &iov, d->pos);
  assert (dcmn_tl != NULL);
  dcmn_tl->type = NULL;
  return dcmn_tl;
}

static void serdata_plist_get_keyhash (const struct ddsi_serdata *serdata_common, struct ddsi_keyhash *buf, bool force_md5)
{
  const struct ddsi_serdata_plist *d = (const struct ddsi_serdata_plist *)serdata_common;
  if (!force_md5)
    memcpy (buf, &d->keyhash, 16);
  else
  {
    ddsrt_md5_state_t md5st;
    ddsrt_md5_init (&md5st);
    ddsrt_md5_append (&md5st, (ddsrt_md5_byte_t *) &d->keyhash, 16);
    ddsrt_md5_finish (&md5st, (ddsrt_md5_byte_t *) buf->value);
  }
}

static size_t serdata_plist_print_plist (const struct ddsi_sertype *sertype_common, const struct ddsi_serdata *serdata_common, char *buf, size_t size)
{
  const struct ddsi_serdata_plist *d = (const struct ddsi_serdata_plist *) serdata_common;
  const struct ddsi_sertype_plist *tp = (const struct ddsi_sertype_plist *) sertype_common;
  struct ddsi_domaingv * const gv = ddsrt_atomic_ldvoidp (&tp->c.gv);
  ddsi_plist_src_t src = {
    .buf = (const unsigned char *) d->data,
    .bufsz = d->pos,
    .encoding = d->identifier,
    .protocol_version = d->protoversion,
    .strict = false,
    .vendorid = d->vendorid
  };
  ddsi_plist_t tmp;
  const enum ddsi_plist_context_kind context_kind = get_plist_context_kind (tp->keyparam);
  if (ddsi_plist_init_frommsg (&tmp, NULL, ~(uint64_t)0, ~(uint64_t)0, &src, gv, context_kind) < 0)
    return (size_t) snprintf (buf, size, "(unparseable-plist)");
  else
  {
    size_t ret = ddsi_plist_print (buf, size, &tmp);
    ddsi_plist_fini (&tmp);
    return ret;
  }
}

const struct ddsi_serdata_ops ddsi_serdata_ops_plist = {
  .get_size = serdata_plist_get_size,
  .eqkey = serdata_plist_eqkey,
  .free = serdata_plist_free,
  .from_ser = serdata_plist_from_ser,
  .from_ser_iov = serdata_plist_from_ser_iov,
  .from_keyhash = serdata_plist_from_keyhash,
  .from_sample = serdata_plist_from_sample,
  .to_ser = serdata_plist_to_ser,
  .to_sample = serdata_plist_to_sample,
  .to_ser_ref = serdata_plist_to_ser_ref,
  .to_ser_unref = serdata_plist_to_ser_unref,
  .to_untyped = serdata_plist_to_untyped,
  .untyped_to_sample = serdata_plist_untyped_to_sample,
  .print = serdata_plist_print_plist,
  .get_keyhash = serdata_plist_get_keyhash
};

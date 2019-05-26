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
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <ctype.h>

#include "dds/ddsrt/log.h"
#include "dds/ddsrt/heap.h"
#include "dds/ddsrt/static_assert.h"

#include "dds/ddsi/q_log.h"

#include "dds/ddsi/q_bswap.h"
#include "dds/ddsi/q_unused.h"
#include "dds/ddsi/q_plist.h"
#include "dds/ddsi/q_time.h"
#include "dds/ddsi/q_xmsg.h"

#include "dds/ddsi/q_config.h"
#include "dds/ddsi/q_globals.h"
#include "dds/ddsi/q_protocol.h" /* for NN_STATUSINFO_... */
#include "dds/ddsi/q_radmin.h" /* for nn_plist_quickscan */

#include "dds/ddsrt/avl.h"
#include "dds/ddsi/q_misc.h" /* for vendor_is_... */

/* I am tempted to change LENGTH_UNLIMITED to 0 in the API (with -1
   supported for backwards compatibility) ... on the wire however
   it must be -1 */
DDSRT_STATIC_ASSERT(DDS_LENGTH_UNLIMITED == -1);

/* These are internal to the parameter list processing. We never
   generate them, and we never want to do see them anywhere outside
   the actual parsing of an incoming parameter list. (There are
   entries in nn_plist, but they are never to be inspected and
   the bits corresponding to them must be 0 except while processing an
   incoming parameter list.) */
#define PPTMP_MULTICAST_IPADDRESS               (1 << 0)
#define PPTMP_DEFAULT_UNICAST_IPADDRESS         (1 << 1)
#define PPTMP_DEFAULT_UNICAST_PORT              (1 << 2)
#define PPTMP_METATRAFFIC_UNICAST_IPADDRESS     (1 << 3)
#define PPTMP_METATRAFFIC_UNICAST_PORT          (1 << 4)
#define PPTMP_METATRAFFIC_MULTICAST_IPADDRESS   (1 << 5)
#define PPTMP_METATRAFFIC_MULTICAST_PORT        (1 << 6)

typedef struct nn_ipaddress_params_tmp {
  uint32_t present;

  nn_ipv4address_t multicast_ipaddress;
  nn_ipv4address_t default_unicast_ipaddress;
  nn_port_t default_unicast_port;
  nn_ipv4address_t metatraffic_unicast_ipaddress;
  nn_port_t metatraffic_unicast_port;
  nn_ipv4address_t metatraffic_multicast_ipaddress;
  nn_port_t metatraffic_multicast_port;
} nn_ipaddress_params_tmp_t;

struct dd {
  const unsigned char *buf;
  size_t bufsz;
  unsigned bswap: 1;
  nn_protocol_version_t protocol_version;
  nn_vendorid_t vendorid;
};

struct cdroctetseq {
  uint32_t len;
  unsigned char value[1];
};

static void log_octetseq (uint32_t cat, uint32_t n, const unsigned char *xs);

static size_t align4u (size_t x)
{
  return (x + 3u) & ~(size_t)3;
}

static int protocol_version_is_newer (nn_protocol_version_t pv)
{
  return (pv.major < RTPS_MAJOR) ? 0 : (pv.major > RTPS_MAJOR) ? 1 : (pv.minor > RTPS_MINOR);
}

static dds_return_t validate_string (const struct dd *dd, size_t *len)
{
  const struct cdrstring *x = (const struct cdrstring *) dd->buf;
  if (dd->bufsz < sizeof (struct cdrstring))
  {
    DDS_TRACE("plist/validate_string: buffer too small (header)\n");
    return DDS_RETCODE_BAD_PARAMETER;
  }
  *len = dd->bswap ? bswap4u (x->length) : x->length;
  if (*len < 1 || *len > dd->bufsz - offsetof (struct cdrstring, contents))
  {
    DDS_TRACE("plist/validate_string: length %" PRIuSIZE " out of range\n", *len);
    return DDS_RETCODE_BAD_PARAMETER;
  }
  if (x->contents[*len-1] != 0)
  {
    DDS_TRACE("plist/validate_string: terminator missing\n");
    return DDS_RETCODE_BAD_PARAMETER;
  }
  return 0;
}

static dds_return_t alias_string (const unsigned char **ptr, const struct dd *dd, size_t *len)
{
  dds_return_t rc;
  if ((rc = validate_string (dd, len)) < 0)
    return rc;
  else
  {
    const struct cdrstring *x = (const struct cdrstring *) dd->buf;
    *ptr = x->contents;
    return 0;
  }
}

static void unalias_string (char **str, int bswap)
{
  const char *alias = *str;
  uint32_t len;
  if (bswap == 0 || bswap == 1)
  {
    const uint32_t *plen = (const uint32_t *) alias - 1;
    len = bswap ? bswap4u (*plen) : *plen;
  }
  else
  {
    len = (uint32_t) strlen (alias) + 1;
  }
  *str = ddsrt_malloc (len);
  memcpy (*str, alias, len);
}

static dds_return_t validate_octetseq (const struct dd *dd, size_t *len)
{
  const struct cdroctetseq *x = (const struct cdroctetseq *) dd->buf;
  if (dd->bufsz < offsetof (struct cdroctetseq, value))
    return DDS_RETCODE_BAD_PARAMETER;
  *len = dd->bswap ? bswap4u (x->len) : x->len;
  if (*len > dd->bufsz - offsetof (struct cdroctetseq, value) || *len >= UINT32_MAX)
    return DDS_RETCODE_BAD_PARAMETER;
  return 0;
}

static dds_return_t alias_octetseq (ddsi_octetseq_t *oseq, const struct dd *dd)
{
  size_t len;
  dds_return_t rc;
  if ((rc = validate_octetseq (dd, &len)) < 0)
    return rc;
  else
  {
    const struct cdroctetseq *x = (const struct cdroctetseq *) dd->buf;
    assert(len < UINT32_MAX); /* it really is an uint32_t on the wire */
    oseq->length = (uint32_t)len;
    oseq->value = (len == 0) ? NULL : (unsigned char *) x->value;
    return 0;
  }
}

static dds_return_t alias_blob (ddsi_octetseq_t *oseq, const struct dd *dd)
{
  assert (dd->bufsz < UINT32_MAX);
  oseq->length = (uint32_t)dd->bufsz;
  oseq->value = (oseq->length == 0) ? NULL : (unsigned char *) dd->buf;
  return 0;
}

static void unalias_octetseq (ddsi_octetseq_t *oseq, UNUSED_ARG (int bswap))
{
  if (oseq->length != 0)
  {
    unsigned char *vs;
    vs = ddsrt_malloc (oseq->length);
    memcpy (vs, oseq->value, oseq->length);
    oseq->value = vs;
  }
}

static dds_return_t validate_stringseq (const struct dd *dd)
{
  const unsigned char *seq = dd->buf;
  const unsigned char *seqend = seq + dd->bufsz;
  struct dd dd1 = *dd;
  int i, n;
  if (dd->bufsz < sizeof (int))
  {
    DDS_TRACE("plist/validate_stringseq: buffer too small (header)\n");
    return DDS_RETCODE_BAD_PARAMETER;
  }
  memcpy (&n, seq, sizeof (n));
  if (dd->bswap)
    n = bswap4 (n);
  seq += sizeof (int);
  if (n < 0)
  {
    DDS_TRACE("plist/validate_stringseq: length %d out of range\n", n);
    return DDS_RETCODE_BAD_PARAMETER;
  }
  else if (n == 0)
  {
    return 0;
  }
  else
  {
    for (i = 0; i < n && seq <= seqend; i++)
    {
      size_t len1;
      int rc;
      dd1.buf = seq;
      dd1.bufsz = (size_t) (seqend - seq);
      if ((rc = validate_string (&dd1, &len1)) < 0)
      {
        DDS_TRACE("plist/validate_stringseq: invalid string\n");
        return rc;
      }
      seq += sizeof (uint32_t) + align4u (len1);
    }
    if (i < n)
    {
      DDS_TRACE("plist/validate_stringseq: buffer too small (contents)\n");
      return DDS_RETCODE_BAD_PARAMETER;
    }
  }
  /* Should I worry about junk between the last string & the end of
     the parameter? */
  return 0;
}

static dds_return_t alias_stringseq (ddsi_stringseq_t *strseq, const struct dd *dd)
{
  /* Not truly an alias: it allocates an array of pointers that alias
     the individual null-terminated strings. Also: see
     validate_stringseq */
  const unsigned char *seq = dd->buf;
  const unsigned char *seqend = seq + dd->bufsz;
  struct dd dd1 = *dd;
  char **strs;
  uint32_t i;
  dds_return_t result;
  if (dd->bufsz < sizeof (int))
  {
    DDS_TRACE("plist/alias_stringseq: buffer too small (header)\n");
    return DDS_RETCODE_BAD_PARAMETER;
  }
  memcpy (&strseq->n, seq, sizeof (strseq->n));
  if (dd->bswap)
    strseq->n = bswap4u (strseq->n);
  seq += sizeof (uint32_t);
  if (strseq->n >= UINT_MAX / sizeof(*strs))
  {
    DDS_TRACE("plist/alias_stringseq: length %"PRIu32" out of range\n", strseq->n);
    return DDS_RETCODE_BAD_PARAMETER;
  }
  else if (strseq->n == 0)
  {
    strseq->strs = NULL;
  }
  else
  {
    strs = ddsrt_malloc (strseq->n * sizeof (*strs));
    for (i = 0; i < strseq->n && seq <= seqend; i++)
    {
      size_t len1;
      dd1.buf = seq;
      dd1.bufsz = (size_t)(seqend - seq);
      /* (const char **) to silence the compiler, unfortunately strseq
         can't have a const char **strs, that would require a const
         and a non-const version of it. */
      if ((result = alias_string ((const unsigned char **) &strs[i], &dd1, &len1)) < 0)
      {
        DDS_TRACE("plist/alias_stringseq: invalid string\n");
        goto fail;
      }
      seq += sizeof (uint32_t) + align4u (len1);
    }
    if (i != strseq->n)
    {
      DDS_TRACE("plist/validate_stringseq: buffer too small (contents)\n");
      result = DDS_RETCODE_BAD_PARAMETER;
      goto fail;
    }
    strseq->strs = strs;
  }
  return 0;
 fail:
  ddsrt_free (strs);
  return result;
}

static void free_stringseq (ddsi_stringseq_t *strseq)
{
  uint32_t i;
  for (i = 0; i < strseq->n; i++)
    if (strseq->strs[i])
      ddsrt_free (strseq->strs[i]);
  ddsrt_free (strseq->strs);
}

static dds_return_t unalias_stringseq (ddsi_stringseq_t *strseq, int bswap)
{
  uint32_t i;
  char **strs;
  if (strseq->n != 0)
  {
    strs = ddsrt_malloc (strseq->n * sizeof (*strs));
    for (i = 0; i < strseq->n; i++)
    {
      strs[i] = strseq->strs[i];
      unalias_string (&strs[i], bswap);
    }
    ddsrt_free (strseq->strs);
    strseq->strs = strs;
  }
  return 0;
}

static void duplicate_stringseq (ddsi_stringseq_t *dest, const ddsi_stringseq_t *src)
{
  uint32_t i;
  dest->n = src->n;
assert (dest->strs == NULL);
  if (dest->n == 0)
  {
    dest->strs = NULL;
    return;
  }
  dest->strs = ddsrt_malloc (dest->n * sizeof (*dest->strs));
  for (i = 0; i < dest->n; i++)
  {
    dest->strs[i] = src->strs[i];
    unalias_string (&dest->strs[i], -1);
  }
}

static void free_locators (nn_locators_t *locs)
{
  while (locs->first)
  {
    struct nn_locators_one *l = locs->first;
    locs->first = l->next;
    ddsrt_free (l);
  }
}

static void unalias_locators (nn_locators_t *locs, UNUSED_ARG (int bswap))
{
  nn_locators_t newlocs;
  struct nn_locators_one *lold;
  /* Copy it, without reversing the order. On failure, free the copy,
     on success overwrite *locs. */
  newlocs.n = locs->n;
  newlocs.first = NULL;
  newlocs.last = NULL;
  for (lold = locs->first; lold != NULL; lold = lold->next)
  {
    struct nn_locators_one *n;
    n = ddsrt_malloc (sizeof (*n));
    n->next = NULL;
    n->loc = lold->loc;
    if (newlocs.first == NULL)
      newlocs.first = n;
    else
      newlocs.last->next = n;
    newlocs.last = n;
  }
  *locs = newlocs;
}

static void unalias_eotinfo (nn_prismtech_eotinfo_t *txnid, UNUSED_ARG (int bswap))
{
  if (txnid->n > 0)
  {
    nn_prismtech_eotgroup_tid_t *vs;
    vs = ddsrt_malloc (txnid->n * sizeof (*vs));
    memcpy (vs, txnid->tids, txnid->n * sizeof (*vs));
    txnid->tids = vs;
  }
}

void nn_plist_fini (nn_plist_t *ps)
{
  struct t { uint64_t fl; size_t off; };
  static const struct t simple[] = {
    { PP_ENTITY_NAME, offsetof (nn_plist_t, entity_name) },
    { PP_PRISMTECH_NODE_NAME, offsetof (nn_plist_t, node_name) },
    { PP_PRISMTECH_EXEC_NAME, offsetof (nn_plist_t, exec_name) },
    { PP_PRISMTECH_PARTICIPANT_VERSION_INFO, offsetof (nn_plist_t, prismtech_participant_version_info.internals) },
    { PP_PRISMTECH_TYPE_DESCRIPTION, offsetof (nn_plist_t, type_description) },
    { PP_PRISMTECH_EOTINFO, offsetof (nn_plist_t, eotinfo.tids) }
  };
  static const struct t locs[] = {
    { PP_UNICAST_LOCATOR, offsetof (nn_plist_t, unicast_locators) },
    { PP_MULTICAST_LOCATOR, offsetof (nn_plist_t, multicast_locators) },
    { PP_DEFAULT_UNICAST_LOCATOR, offsetof (nn_plist_t, default_unicast_locators) },
    { PP_DEFAULT_MULTICAST_LOCATOR, offsetof (nn_plist_t, default_multicast_locators) },
    { PP_METATRAFFIC_UNICAST_LOCATOR, offsetof (nn_plist_t, metatraffic_unicast_locators) },
    { PP_METATRAFFIC_MULTICAST_LOCATOR, offsetof (nn_plist_t, metatraffic_multicast_locators) }
  };
  int i;
  nn_xqos_fini (&ps->qos);

  /* The compiler doesn't understand how offsetof is used in the arrays. */
  DDSRT_WARNING_MSVC_OFF(6001);
  for (i = 0; i < (int) (sizeof (simple) / sizeof (*simple)); i++)
  {
    if ((ps->present & simple[i].fl) && !(ps->aliased & simple[i].fl))
    {
      void **pp = (void **) ((char *) ps + simple[i].off);
      ddsrt_free (*pp);
    }
  }
  for (i = 0; i < (int) (sizeof (locs) / sizeof (*locs)); i++)
  {
    if ((ps->present & locs[i].fl) && !(ps->aliased & locs[i].fl))
      free_locators ((nn_locators_t *) ((char *) ps + locs[i].off));
  }
  DDSRT_WARNING_MSVC_ON(6001);

  ps->present = 0;
}

#if 0 /* not currently needed */
void nn_plist_unalias (nn_plist_t *ps)
{
#define P(name_, func_, field_) do {                                    \
    if ((ps->present & PP_##name_) && (ps->aliased & PP_##name_)) {     \
      unalias_##func_ (&ps->field_, -1);                                \
      ps->aliased &= ~PP_##name_;                                       \
    }                                                                   \
  } while (0)
  nn_xqos_unalias (&ps->qos);
  P (ENTITY_NAME, string, entity_name);
  P (UNICAST_LOCATOR, locators, unicast_locators);
  P (MULTICAST_LOCATOR, locators, multicast_locators);
  P (DEFAULT_UNICAST_LOCATOR, locators, default_unicast_locators);
  P (DEFAULT_MULTICAST_LOCATOR, locators, default_multicast_locators);
  P (METATRAFFIC_UNICAST_LOCATOR, locators, metatraffic_unicast_locators);
  P (METATRAFFIC_MULTICAST_LOCATOR, locators, metatraffic_multicast_locators);
  P (PRISMTECH_NODE_NAME, string, node_name);
  P (PRISMTECH_EXEC_NAME, string, exec_name);
  P (PRISMTECH_TYPE_DESCRIPTION, string, type_description);
  P (PRISMTECH_EOTINFO, eotinfo, eotinfo);
#undef P
  if ((ps->present & PP_PRISMTECH_PARTICIPANT_VERSION_INFO) &&
      (ps->aliased & PP_PRISMTECH_PARTICIPANT_VERSION_INFO))
  {
    unalias_string (&ps->prismtech_participant_version_info.internals, -1);
    ps->aliased &= ~PP_PRISMTECH_PARTICIPANT_VERSION_INFO;
  }

  assert (ps->aliased == 0);
}
#endif

static dds_return_t do_octetseq (ddsi_octetseq_t *dst, uint64_t *present, uint64_t *aliased, uint64_t wanted, uint64_t fl, const struct dd *dd)
{
  dds_return_t res;
  size_t len;
  if (!(wanted & fl))
    return NN_STRICT_P ? validate_octetseq (dd, &len) : 0;
  if ((res = alias_octetseq (dst, dd)) >= 0)
  {
    *present |= fl;
    *aliased |= fl;
  }
  return res;
}

static dds_return_t do_blob (ddsi_octetseq_t *dst, uint64_t *present, uint64_t *aliased, uint64_t wanted, uint64_t fl, const struct dd *dd)
{
  dds_return_t res;
  if (!(wanted & fl))
    return 0;
  if ((res = alias_blob (dst, dd)) >= 0)
  {
    *present |= fl;
    *aliased |= fl;
  }
  return res;
}

static dds_return_t do_string (char **dst, uint64_t *present, uint64_t *aliased, uint64_t wanted, uint64_t fl, const struct dd *dd)
{
  dds_return_t res;
  size_t len;
  if (!(wanted & fl))
    return NN_STRICT_P ? validate_string (dd, &len) : 0;
  if ((res = alias_string ((const unsigned char **) dst, dd, &len)) >= 0)
  {
    *present |= fl;
    *aliased |= fl;
  }
  return res;
}

static dds_return_t do_stringseq (ddsi_stringseq_t *dst, uint64_t *present, uint64_t *aliased, uint64_t wanted, uint64_t fl, const struct dd *dd)
{
  dds_return_t res;
  if (!(wanted & fl))
    return NN_STRICT_P ? validate_stringseq (dd) : 0;
  if ((res = alias_stringseq (dst, dd)) >= 0)
  {
    *present |= fl;
    *aliased |= fl;
  }
  return res;
}

static void bswap_time (ddsi_time_t *t)
{
  t->seconds = bswap4 (t->seconds);
  t->fraction = bswap4u (t->fraction);
}

static dds_return_t validate_time (const ddsi_time_t *t)
{
  /* Accepted are zero, positive, infinite or invalid as defined in
     the DDS 2.1 spec, table 9.4. */
  if (t->seconds >= 0)
    return 0;
  else if (t->seconds == -1 && t->fraction == UINT32_MAX)
    return 0;
  else
  {
    DDS_TRACE("plist/validate_time: invalid timestamp (%08x.%08x)\n", t->seconds, t->fraction);
    return DDS_RETCODE_BAD_PARAMETER;
  }
}

static void bswap_external_duration (ddsi_duration_t *d)
{
  bswap_time (d);
}

static dds_return_t validate_external_duration (const ddsi_duration_t *d)
{
  return validate_time (d);
}

dds_return_t validate_duration (const dds_duration_t d)
{
  return (d >= 0 && d <= DDS_INFINITY) ? DDS_RETCODE_OK : DDS_RETCODE_BAD_PARAMETER;
}

static dds_return_t do_duration (dds_duration_t *q, uint64_t *present, uint64_t fl, const struct dd *dd)
{
  ddsi_duration_t extq;
  dds_return_t res;
  if (dd->bufsz < sizeof (extq))
  {
    DDS_TRACE("plist/do_duration: buffer too small\n");
    return DDS_RETCODE_BAD_PARAMETER;
  }
  memcpy (&extq, dd->buf, sizeof (extq));
  if (dd->bswap)
    bswap_external_duration (&extq);
  if ((res = validate_external_duration (&extq)) < 0)
    return res;
  *q = nn_from_ddsi_duration (extq);
  *present |= fl;
  return 0;
}

static void bswap_durability_qospolicy (dds_durability_qospolicy_t *q)
{
  q->kind = bswap4u (q->kind);
}

dds_return_t validate_durability_qospolicy (const dds_durability_qospolicy_t *q)
{
  switch (q->kind)
  {
    case DDS_DURABILITY_VOLATILE:
    case DDS_DURABILITY_TRANSIENT_LOCAL:
    case DDS_DURABILITY_TRANSIENT:
    case DDS_DURABILITY_PERSISTENT:
      break;
    default:
      DDS_TRACE("plist/validate_durability_qospolicy: invalid kind (%d)\n", (int) q->kind);
      return DDS_RETCODE_BAD_PARAMETER;
  }
  return 0;
}

static void bswap_history_qospolicy (dds_history_qospolicy_t *q)
{
  q->kind = bswap4u (q->kind);
  q->depth = bswap4 (q->depth);
}

static int history_qospolicy_allzero (const dds_history_qospolicy_t *q)
{
  return q->kind == DDS_HISTORY_KEEP_LAST && q->depth == 0;
}

dds_return_t validate_history_qospolicy (const dds_history_qospolicy_t *q)
{
  /* Validity of history setting and of resource limits are dependent,
     but we don't have access to the resource limits here ... the
     combination can only be validated once all the qos policies have
     been parsed.

     Why is KEEP_LAST n or KEEP_ALL instead of just KEEP_LAST n, with
     n possibly unlimited. */
  switch (q->kind)
  {
    case DDS_HISTORY_KEEP_LAST:
    case DDS_HISTORY_KEEP_ALL:
      break;
    default:
      DDS_TRACE("plist/validate_history_qospolicy: invalid kind (%d)\n", (int) q->kind);
      return DDS_RETCODE_BAD_PARAMETER;
  }
  /* Accept all values for depth if kind = ALL */
  if (q->kind == DDS_HISTORY_KEEP_LAST)
  {
    if (q->depth < 1)
    {
      DDS_TRACE("plist/validate_history_qospolicy: invalid depth (%d)\n", (int) q->depth);
      return DDS_RETCODE_BAD_PARAMETER;
    }
  }
  return 0;
}

static void bswap_resource_limits_qospolicy (dds_resource_limits_qospolicy_t *q)
{
  q->max_samples = bswap4 (q->max_samples);
  q->max_instances = bswap4 (q->max_instances);
  q->max_samples_per_instance = bswap4 (q->max_samples_per_instance);
}

static int resource_limits_qospolicy_allzero (const dds_resource_limits_qospolicy_t *q)
{
  return q->max_samples == 0 && q->max_instances == 0 && q->max_samples_per_instance == 0;
}

dds_return_t validate_resource_limits_qospolicy (const dds_resource_limits_qospolicy_t *q)
{
  const int unlimited = DDS_LENGTH_UNLIMITED;
  /* Note: dependent on history setting as well (see
     validate_history_qospolicy). Verifying only the internal
     consistency of the resource limits. */
  if (q->max_samples < 1 && q->max_samples != unlimited)
  {
    DDS_TRACE("plist/validate_resource_limits_qospolicy: max_samples invalid (%d)\n", (int) q->max_samples);
    return DDS_RETCODE_BAD_PARAMETER;
  }
  if (q->max_instances < 1 && q->max_instances != unlimited)
  {
    DDS_TRACE("plist/validate_resource_limits_qospolicy: max_instances invalid (%d)\n", (int) q->max_instances);
    return DDS_RETCODE_BAD_PARAMETER;
  }
  if (q->max_samples_per_instance < 1 && q->max_samples_per_instance != unlimited)
  {
    DDS_TRACE("plist/validate_resource_limits_qospolicy: max_samples_per_instance invalid (%d)\n", (int) q->max_samples_per_instance);
    return DDS_RETCODE_BAD_PARAMETER;
  }
  if (q->max_samples != unlimited && q->max_samples_per_instance != unlimited)
  {
    /* Interpreting 7.1.3.19 as if "unlimited" is meant to mean "don't
       care" and any conditions related to it must be ignored. */
    if (q->max_samples < q->max_samples_per_instance)
    {
      DDS_TRACE("plist/validate_resource_limits_qospolicy: max_samples (%d) and max_samples_per_instance (%d) incompatible\n", (int) q->max_samples, (int) q->max_samples_per_instance);
      return DDS_RETCODE_BAD_PARAMETER;
    }
  }
  return 0;
}

dds_return_t validate_history_and_resource_limits (const dds_history_qospolicy_t *qh, const dds_resource_limits_qospolicy_t *qr)
{
  const int unlimited = DDS_LENGTH_UNLIMITED;
  dds_return_t res;
  if ((res = validate_history_qospolicy (qh)) < 0)
  {
    DDS_TRACE("plist/validate_history_and_resource_limits: history policy invalid\n");
    return res;
  }
  if ((res = validate_resource_limits_qospolicy (qr)) < 0)
  {
    DDS_TRACE("plist/validate_history_and_resource_limits: resource_limits policy invalid\n");
    return res;
  }
  switch (qh->kind)
  {
    case DDS_HISTORY_KEEP_ALL:
#if 0 /* See comment in validate_resource_limits, ref'ing 7.1.3.19 */
      if (qr->max_samples_per_instance != unlimited)
      {
        DDS_TRACE("plist/validate_history_and_resource_limits: max_samples_per_instance (%d) incompatible with KEEP_ALL policy\n", (int) qr->max_samples_per_instance);
        return DDS_RETCODE_BAD_PARAMETER;
      }
#endif
      break;
    case DDS_HISTORY_KEEP_LAST:
      if (qr->max_samples_per_instance != unlimited && qh->depth > qr->max_samples_per_instance)
      {
        DDS_TRACE("plist/validate_history_and_resource_limits: depth (%d) and max_samples_per_instance (%d) incompatible with KEEP_LAST policy\n", (int) qh->depth, (int) qr->max_samples_per_instance);
        return DDS_RETCODE_BAD_PARAMETER;
      }
      break;
  }
  return 0;
}

static void bswap_external_durability_service_qospolicy (dds_external_durability_service_qospolicy_t *q)
{
  bswap_external_duration (&q->service_cleanup_delay);
  bswap_history_qospolicy (&q->history);
  bswap_resource_limits_qospolicy (&q->resource_limits);
}

static int durability_service_qospolicy_allzero (const dds_durability_service_qospolicy_t *q)
{
  return (history_qospolicy_allzero (&q->history) &&
          resource_limits_qospolicy_allzero (&q->resource_limits) &&
          q->service_cleanup_delay == 0);
}

static int external_durability_service_qospolicy_allzero (const dds_external_durability_service_qospolicy_t *q)
{
  return (history_qospolicy_allzero (&q->history) &&
          resource_limits_qospolicy_allzero (&q->resource_limits) &&
          q->service_cleanup_delay.seconds == 0 && q->service_cleanup_delay.fraction == 0);
}

static dds_return_t validate_external_durability_service_qospolicy_acceptzero (const dds_external_durability_service_qospolicy_t *q, bool acceptzero)
{
  dds_return_t res;
  if (acceptzero && external_durability_service_qospolicy_allzero (q))
    return 0;
  if ((res = validate_external_duration (&q->service_cleanup_delay)) < 0)
  {
    DDS_TRACE("plist/validate_durability_service_qospolicy: duration invalid\n");
    return res;
  }
  if ((res = validate_history_and_resource_limits (&q->history, &q->resource_limits)) < 0)
  {
    DDS_TRACE("plist/validate_durability_service_qospolicy: invalid history and/or resource limits\n");
    return res;
  }
  return 0;
}

dds_return_t validate_durability_service_qospolicy (const dds_durability_service_qospolicy_t *q)
{
  dds_external_durability_service_qospolicy_t qext;
  qext.history = q->history;
  qext.resource_limits = q->resource_limits;
  qext.service_cleanup_delay = nn_to_ddsi_duration (q->service_cleanup_delay);
  return validate_external_durability_service_qospolicy_acceptzero (&qext, false);
}

static void bswap_external_liveliness_qospolicy (dds_external_liveliness_qospolicy_t *q)
{
  q->kind = bswap4u (q->kind);
  bswap_external_duration (&q->lease_duration);
}

static dds_return_t validate_external_liveliness_qospolicy (const dds_external_liveliness_qospolicy_t *q)
{
  dds_return_t res;
  switch (q->kind)
  {
    case DDS_LIVELINESS_AUTOMATIC:
    case DDS_LIVELINESS_MANUAL_BY_PARTICIPANT:
    case DDS_LIVELINESS_MANUAL_BY_TOPIC:
      if ((res = validate_external_duration (&q->lease_duration)) < 0)
        DDS_TRACE("plist/validate_liveliness_qospolicy: invalid lease duration\n");
      return res;
    default:
      DDS_TRACE("plist/validate_liveliness_qospolicy: invalid kind (%d)\n", (int) q->kind);
      return DDS_RETCODE_BAD_PARAMETER;
  }
}

dds_return_t validate_liveliness_qospolicy (const dds_liveliness_qospolicy_t *q)
{
  dds_external_liveliness_qospolicy_t qext;
  qext.kind = q->kind;
  qext.lease_duration = nn_to_ddsi_duration (q->lease_duration);
  return validate_external_liveliness_qospolicy (&qext);
}

static void bswap_external_reliability_qospolicy (dds_external_reliability_qospolicy_t *qext)
{
  qext->kind = bswap4u (qext->kind);
  bswap_external_duration (&qext->max_blocking_time);
}

static dds_return_t validate_xform_reliability_qospolicy (dds_reliability_qospolicy_t *qdst, const dds_external_reliability_qospolicy_t *qext)
{
  dds_return_t res;
  qdst->max_blocking_time = nn_from_ddsi_duration (qext->max_blocking_time);
  switch (qext->kind)
  {
    case DDS_EXTERNAL_RELIABILITY_BEST_EFFORT:
      qdst->kind = DDS_RELIABILITY_BEST_EFFORT;
      return 0;
    case DDS_EXTERNAL_RELIABILITY_RELIABLE:
      qdst->kind = DDS_RELIABILITY_RELIABLE;
      if ((res = validate_external_duration (&qext->max_blocking_time)) < 0)
        DDS_TRACE("plist/validate_xform_reliability_qospolicy[!pedantic]: max_blocking time invalid\n");
      return res;
    default:
      DDS_TRACE("plist/validate_xform_reliability_qospolicy[!pedantic]: invalid kind (%d)\n", (int) qext->kind);
      return DDS_RETCODE_BAD_PARAMETER;
  }
}

static void bswap_destination_order_qospolicy (dds_destination_order_qospolicy_t *q)
{
  q->kind = bswap4u (q->kind);
}

dds_return_t validate_destination_order_qospolicy (const dds_destination_order_qospolicy_t *q)
{
  switch (q->kind)
  {
    case DDS_DESTINATIONORDER_BY_RECEPTION_TIMESTAMP:
    case DDS_DESTINATIONORDER_BY_SOURCE_TIMESTAMP:
      return 0;
    default:
      DDS_TRACE("plist/validate_destination_order_qospolicy: invalid kind (%d)\n", (int) q->kind);
      return DDS_RETCODE_BAD_PARAMETER;
  }
}

static void bswap_ownership_qospolicy (dds_ownership_qospolicy_t *q)
{
  q->kind = bswap4u (q->kind);
}

dds_return_t validate_ownership_qospolicy (const dds_ownership_qospolicy_t *q)
{
  switch (q->kind)
  {
    case DDS_OWNERSHIP_SHARED:
    case DDS_OWNERSHIP_EXCLUSIVE:
      return 0;
    default:
      DDS_TRACE("plist/validate_ownership_qospolicy: invalid kind (%d)\n", (int) q->kind);
      return DDS_RETCODE_BAD_PARAMETER;
  }
}

static void bswap_ownership_strength_qospolicy (dds_ownership_strength_qospolicy_t *q)
{
  q->value = bswap4 (q->value);
}

dds_return_t validate_ownership_strength_qospolicy (UNUSED_ARG (const dds_ownership_strength_qospolicy_t *q))
{
  return 0;
}

static void bswap_presentation_qospolicy (dds_presentation_qospolicy_t *q)
{
  q->access_scope = bswap4u (q->access_scope);
}

dds_return_t validate_presentation_qospolicy (const dds_presentation_qospolicy_t *q)
{
  switch (q->access_scope)
  {
    case DDS_PRESENTATION_INSTANCE:
    case DDS_PRESENTATION_TOPIC:
    case DDS_PRESENTATION_GROUP:
      break;
    default:
      DDS_TRACE("plist/validate_presentation_qospolicy: invalid access_scope (%d)\n", (int) q->access_scope);
      return DDS_RETCODE_BAD_PARAMETER;
  }
  /* Bools must be 0 or 1, i.e., only the lsb may be set */
  if (q->coherent_access & ~1)
  {
    DDS_TRACE("plist/validate_presentation_qospolicy: coherent_access invalid (%d)\n", (int) q->coherent_access);
    return DDS_RETCODE_BAD_PARAMETER;
  }
  if (q->ordered_access & ~1)
  {
    DDS_TRACE("plist/validate_presentation_qospolicy: ordered_access invalid (%d)\n", (int) q->ordered_access);
    return DDS_RETCODE_BAD_PARAMETER;
  }
  /* coherent_access & ordered_access are a bit irrelevant for
     instance presentation qos, but it appears as if their values are
     not prescribed in that case. */
  return 0;
}

static void bswap_transport_priority_qospolicy (dds_transport_priority_qospolicy_t *q)
{
  q->value = bswap4 (q->value);
}

dds_return_t validate_transport_priority_qospolicy (UNUSED_ARG (const dds_transport_priority_qospolicy_t *q))
{
  return 0;
}

static dds_return_t add_locator (nn_locators_t *ls, uint64_t *present, uint64_t wanted, uint64_t fl, const nn_locator_t *loc)
{
  if (wanted & fl)
  {
    struct nn_locators_one *nloc;
    if (!(*present & fl))
    {
      ls->n = 0;
      ls->first = NULL;
      ls->last = NULL;
    }
    nloc = ddsrt_malloc (sizeof (*nloc));
    nloc->loc = *loc;
    nloc->next = NULL;
    if (ls->first == NULL)
      ls->first = nloc;
    else
    {
      assert (ls->last != NULL);
      ls->last->next = nloc;
    }
    ls->last = nloc;
    ls->n++;
    *present |= fl;
  }
  return 0;
}

static int locator_address_prefix12_zero (const nn_locator_t *loc)
{
  /* loc has has 32 bit ints preceding the address, hence address is
     4-byte aligned; reading char* as unsigneds isn't illegal type
     punning */
  const uint32_t *u = (const uint32_t *) loc->address;
  return (u[0] == 0 && u[1] == 0 && u[2] == 0);
}

static int locator_address_zero (const nn_locator_t *loc)
{
  /* see locator_address_prefix12_zero */
  const uint32_t *u = (const uint32_t *) loc->address;
  return (u[0] == 0 && u[1] == 0 && u[2] == 0 && u[3] == 0);
}

static dds_return_t do_locator
(
  nn_locators_t *ls,
  uint64_t *present,
  uint64_t wanted,
  uint64_t fl,
  const struct dd *dd
)
{
  nn_locator_t loc;

  if (dd->bufsz < sizeof (loc))
  {
    DDS_TRACE("plist/do_locator: buffer too small\n");
    return DDS_RETCODE_BAD_PARAMETER;
  }
  memcpy (&loc, dd->buf, sizeof (loc));
  if (dd->bswap)
  {
    loc.kind = bswap4 (loc.kind);
    loc.port = bswap4u (loc.port);
  }
  switch (loc.kind)
  {
    case NN_LOCATOR_KIND_UDPv4:
    case NN_LOCATOR_KIND_TCPv4:
      if (loc.port <= 0 || loc.port > 65535)
      {
        DDS_TRACE("plist/do_locator[kind=IPv4]: invalid port (%d)\n", (int) loc.port);
        return DDS_RETCODE_BAD_PARAMETER;
      }
      if (!locator_address_prefix12_zero (&loc))
      {
        DDS_TRACE("plist/do_locator[kind=IPv4]: junk in address prefix\n");
        return DDS_RETCODE_BAD_PARAMETER;
      }
      break;
    case NN_LOCATOR_KIND_UDPv6:
    case NN_LOCATOR_KIND_TCPv6:
      if (loc.port <= 0 || loc.port > 65535)
      {
        DDS_TRACE("plist/do_locator[kind=IPv6]: invalid port (%d)\n", (int) loc.port);
        return DDS_RETCODE_BAD_PARAMETER;
      }
      break;
    case NN_LOCATOR_KIND_UDPv4MCGEN: {
      const nn_udpv4mcgen_address_t *x = (const nn_udpv4mcgen_address_t *) loc.address;
      if (!ddsi_factory_supports(gv.m_factory, NN_LOCATOR_KIND_UDPv4))
        return 0;
      if (loc.port <= 0 || loc.port > 65536)
      {
        DDS_TRACE("plist/do_locator[kind=IPv4MCGEN]: invalid port (%d)\n", (int) loc.port);
        return DDS_RETCODE_BAD_PARAMETER;
      }
      if ((int)x->base + x->count >= 28 || x->count == 0 || x->idx >= x->count)
      {
        DDS_TRACE("plist/do_locator[kind=IPv4MCGEN]: invalid base/count/idx (%u,%u,%u)\n", x->base, x->count, x->idx);
        return DDS_RETCODE_BAD_PARAMETER;
      }
      break;
    }
    case NN_LOCATOR_KIND_INVALID:
      if (!locator_address_zero (&loc))
      {
        DDS_TRACE("plist/do_locator[kind=INVALID]: junk in address\n");
        return DDS_RETCODE_BAD_PARAMETER;
      }
      if (loc.port != 0)
      {
        DDS_TRACE("plist/do_locator[kind=INVALID]: junk in port\n");
        return DDS_RETCODE_BAD_PARAMETER;
      }
      /* silently dropped correctly formatted "invalid" locators. */
      return 0;
    case NN_LOCATOR_KIND_RESERVED:
      /* silently dropped "reserved" locators. */
      return 0;
    default:
      DDS_TRACE("plist/do_locator: invalid kind (%d)\n", (int) loc.kind);
      return NN_PEDANTIC_P ? DDS_RETCODE_BAD_PARAMETER : 0;
  }
  return add_locator (ls, present, wanted, fl, &loc);
}

static void locator_from_ipv4address_port (nn_locator_t *loc, const nn_ipv4address_t *a, const nn_port_t *p)
{
  loc->kind = gv.m_factory->m_connless ? NN_LOCATOR_KIND_UDPv4 : NN_LOCATOR_KIND_TCPv4;
  loc->port = *p;
  memset (loc->address, 0, 12);
  memcpy (loc->address + 12, a, 4);
}

static dds_return_t do_ipv4address (nn_plist_t *dest, nn_ipaddress_params_tmp_t *dest_tmp, uint64_t wanted, uint32_t fl_tmp, const struct dd *dd)
{
  nn_ipv4address_t *a;
  nn_port_t *p;
  nn_locators_t *ls;
  uint32_t fl1_tmp;
  uint64_t fldest;
  if (dd->bufsz < sizeof (*a))
  {
    DDS_TRACE("plist/do_ipv4address: buffer too small\n");
    return DDS_RETCODE_BAD_PARAMETER;
  }
  switch (fl_tmp)
  {
    case PPTMP_MULTICAST_IPADDRESS:
      a = &dest_tmp->multicast_ipaddress;
      p = NULL; /* don't know which port to use ... */
      fl1_tmp = 0;
      fldest = PP_MULTICAST_LOCATOR;
      ls = &dest->multicast_locators;
      break;
    case PPTMP_DEFAULT_UNICAST_IPADDRESS:
      a = &dest_tmp->default_unicast_ipaddress;
      p = &dest_tmp->default_unicast_port;
      fl1_tmp = PPTMP_DEFAULT_UNICAST_PORT;
      fldest = PP_DEFAULT_UNICAST_LOCATOR;
      ls = &dest->unicast_locators;
      break;
    case PPTMP_METATRAFFIC_UNICAST_IPADDRESS:
      a = &dest_tmp->metatraffic_unicast_ipaddress;
      p = &dest_tmp->metatraffic_unicast_port;
      fl1_tmp = PPTMP_METATRAFFIC_UNICAST_PORT;
      fldest = PP_METATRAFFIC_UNICAST_LOCATOR;
      ls = &dest->metatraffic_unicast_locators;
      break;
    case PPTMP_METATRAFFIC_MULTICAST_IPADDRESS:
      a = &dest_tmp->metatraffic_multicast_ipaddress;
      p = &dest_tmp->metatraffic_multicast_port;
      fl1_tmp = PPTMP_METATRAFFIC_MULTICAST_PORT;
      fldest = PP_METATRAFFIC_MULTICAST_LOCATOR;
      ls = &dest->metatraffic_multicast_locators;
      break;
    default:
      abort ();
  }
  memcpy (a, dd->buf, sizeof (*a));
  dest_tmp->present |= fl_tmp;

  /* PPTMP_MULTICAST_IPADDRESS must fail because we don't have a port.
     (There are of course other ways of failing ...)  Option 1: set
     fl1 to a value to bit that's never set; option 2: explicit check.
     Since this code hardly ever gets executed, use option 2. */

  if (fl1_tmp && ((dest_tmp->present & (fl_tmp | fl1_tmp)) == (fl_tmp | fl1_tmp)))
  {
    /* If port already known, add corresponding locator and discard
       both address & port from the set of present plist: this
       allows adding another pair. */

    nn_locator_t loc;
    locator_from_ipv4address_port (&loc, a, p);
    dest_tmp->present &= ~(fl_tmp | fl1_tmp);
    return add_locator (ls, &dest->present, wanted, fldest, &loc);
  }
  else
  {
    return 0;
  }
}

static dds_return_t do_port (nn_plist_t *dest, nn_ipaddress_params_tmp_t *dest_tmp, uint64_t wanted, uint32_t fl_tmp, const struct dd *dd)
{
  nn_ipv4address_t *a;
  nn_port_t *p;
  nn_locators_t *ls;
  uint64_t fldest;
  uint32_t fl1_tmp;
  if (dd->bufsz < sizeof (*p))
  {
    DDS_TRACE("plist/do_port: buffer too small\n");
    return DDS_RETCODE_BAD_PARAMETER;
  }
  switch (fl_tmp)
  {
    case PPTMP_DEFAULT_UNICAST_PORT:
      a = &dest_tmp->default_unicast_ipaddress;
      p = &dest_tmp->default_unicast_port;
      fl1_tmp = PPTMP_DEFAULT_UNICAST_IPADDRESS;
      fldest = PP_DEFAULT_UNICAST_LOCATOR;
      ls = &dest->unicast_locators;
      break;
    case PPTMP_METATRAFFIC_UNICAST_PORT:
      a = &dest_tmp->metatraffic_unicast_ipaddress;
      p = &dest_tmp->metatraffic_unicast_port;
      fl1_tmp = PPTMP_METATRAFFIC_UNICAST_IPADDRESS;
      fldest = PP_METATRAFFIC_UNICAST_LOCATOR;
      ls = &dest->metatraffic_unicast_locators;
      break;
    case PPTMP_METATRAFFIC_MULTICAST_PORT:
      a = &dest_tmp->metatraffic_multicast_ipaddress;
      p = &dest_tmp->metatraffic_multicast_port;
      fl1_tmp = PPTMP_METATRAFFIC_MULTICAST_IPADDRESS;
      fldest = PP_METATRAFFIC_MULTICAST_LOCATOR;
      ls = &dest->metatraffic_multicast_locators;
      break;
    default:
      abort ();
  }
  memcpy (p, dd->buf, sizeof (*p));
  if (dd->bswap)
    *p = bswap4u (*p);
  if (*p <= 0 || *p > 65535)
  {
    DDS_TRACE("plist/do_port: invalid port (%d)\n", (int) *p);
    return DDS_RETCODE_BAD_PARAMETER;
  }
  dest_tmp->present |= fl_tmp;
  if ((dest_tmp->present & (fl_tmp | fl1_tmp)) == (fl_tmp | fl1_tmp))
  {
    /* If port already known, add corresponding locator and discard
       both address & port from the set of present plist: this
       allows adding another pair. */
    nn_locator_t loc;
    locator_from_ipv4address_port (&loc, a, p);
    dest_tmp->present &= ~(fl_tmp | fl1_tmp);
    return add_locator (ls, &dest->present, wanted, fldest, &loc);
  }
  else
  {
    return 0;
  }
}

static dds_return_t valid_participant_guid (const nn_guid_t *g, UNUSED_ARG (const struct dd *dd))
{
  /* All 0 is GUID_UNKNOWN, which is a defined GUID */
  if (g->prefix.u[0] == 0 && g->prefix.u[1] == 0 && g->prefix.u[2] == 0)
  {
    if (g->entityid.u == 0)
      return 0;
    else
    {
      DDS_TRACE("plist/valid_participant_guid: prefix is 0 but entityid is not (%"PRIu32")\n", g->entityid.u);
      return DDS_RETCODE_BAD_PARAMETER;
    }
  }
  else if (g->entityid.u == NN_ENTITYID_PARTICIPANT)
  {
    return 0;
  }
  else
  {
    DDS_TRACE("plist/valid_participant_guid: entityid not a participant entityid (%"PRIu32")\n", g->entityid.u);
    return DDS_RETCODE_BAD_PARAMETER;
  }
}

static dds_return_t valid_group_guid (const nn_guid_t *g, UNUSED_ARG (const struct dd *dd))
{
  /* All 0 is GUID_UNKNOWN, which is a defined GUID */
  if (g->prefix.u[0] == 0 && g->prefix.u[1] == 0 && g->prefix.u[2] == 0)
  {
    if (g->entityid.u == 0)
      return 0;
    else
    {
      DDS_TRACE("plist/valid_group_guid: prefix is 0 but entityid is not (%"PRIu32")\n", g->entityid.u);
      return DDS_RETCODE_BAD_PARAMETER;
    }
  }
  else if (g->entityid.u != 0)
  {
    /* accept any entity id */
    return 0;
  }
  else
  {
    DDS_TRACE("plist/valid_group_guid: entityid is 0\n");
    return DDS_RETCODE_BAD_PARAMETER;
  }
}

static dds_return_t valid_endpoint_guid (const nn_guid_t *g, const struct dd *dd)
{
  /* All 0 is GUID_UNKNOWN, which is a defined GUID */
  if (g->prefix.u[0] == 0 && g->prefix.u[1] == 0 && g->prefix.u[2] == 0)
  {
    if (g->entityid.u == 0)
      return 0;
    else
    {
      DDS_TRACE("plist/valid_endpoint_guid: prefix is 0 but entityid is not (%"PRIx32")\n", g->entityid.u);
      return DDS_RETCODE_BAD_PARAMETER;
    }
  }
  switch (g->entityid.u & NN_ENTITYID_SOURCE_MASK)
  {
    case NN_ENTITYID_SOURCE_USER:
      switch (g->entityid.u & NN_ENTITYID_KIND_MASK)
      {
        case NN_ENTITYID_KIND_WRITER_WITH_KEY:
        case NN_ENTITYID_KIND_WRITER_NO_KEY:
        case NN_ENTITYID_KIND_READER_NO_KEY:
        case NN_ENTITYID_KIND_READER_WITH_KEY:
          return 0;
        default:
          if (protocol_version_is_newer (dd->protocol_version))
            return 0;
          else
          {
            DDS_TRACE("plist/valid_endpoint_guid[src=USER,proto=%u.%u]: invalid kind (%"PRIx32")\n",
                    dd->protocol_version.major, dd->protocol_version.minor,
                    g->entityid.u & NN_ENTITYID_KIND_MASK);
            return DDS_RETCODE_BAD_PARAMETER;
          }
      }
    case NN_ENTITYID_SOURCE_BUILTIN:
      switch (g->entityid.u)
      {
        case NN_ENTITYID_SEDP_BUILTIN_TOPIC_WRITER:
        case NN_ENTITYID_SEDP_BUILTIN_TOPIC_READER:
        case NN_ENTITYID_SEDP_BUILTIN_PUBLICATIONS_WRITER:
        case NN_ENTITYID_SEDP_BUILTIN_PUBLICATIONS_READER:
        case NN_ENTITYID_SEDP_BUILTIN_SUBSCRIPTIONS_WRITER:
        case NN_ENTITYID_SEDP_BUILTIN_SUBSCRIPTIONS_READER:
        case NN_ENTITYID_SPDP_BUILTIN_PARTICIPANT_WRITER:
        case NN_ENTITYID_SPDP_BUILTIN_PARTICIPANT_READER:
        case NN_ENTITYID_P2P_BUILTIN_PARTICIPANT_MESSAGE_WRITER:
        case NN_ENTITYID_P2P_BUILTIN_PARTICIPANT_MESSAGE_READER:
          return 0;
        default:
          if (protocol_version_is_newer (dd->protocol_version))
            return 0;
          else
          {
            DDS_TRACE("plist/valid_endpoint_guid[src=BUILTIN,proto=%u.%u]: invalid entityid (%"PRIx32")\n",
                    dd->protocol_version.major, dd->protocol_version.minor, g->entityid.u);
            return DDS_RETCODE_BAD_PARAMETER;
          }
      }
    case NN_ENTITYID_SOURCE_VENDOR:
      if (!vendor_is_eclipse (dd->vendorid))
        return 0;
      else
      {
        switch (g->entityid.u)
        {
          case NN_ENTITYID_SEDP_BUILTIN_CM_PARTICIPANT_WRITER:
          case NN_ENTITYID_SEDP_BUILTIN_CM_PARTICIPANT_READER:
          case NN_ENTITYID_SEDP_BUILTIN_CM_PUBLISHER_WRITER:
          case NN_ENTITYID_SEDP_BUILTIN_CM_PUBLISHER_READER:
          case NN_ENTITYID_SEDP_BUILTIN_CM_SUBSCRIBER_WRITER:
          case NN_ENTITYID_SEDP_BUILTIN_CM_SUBSCRIBER_READER:
            return 0;
          default:
            if (protocol_version_is_newer (dd->protocol_version))
              return 0;
            else
            {
              DDS_TRACE("plist/valid_endpoint_guid[src=VENDOR,proto=%u.%u]: unexpected entityid (%"PRIx32")\n",
                      dd->protocol_version.major, dd->protocol_version.minor, g->entityid.u);
              return 0;
            }
        }
      }
    default:
      DDS_TRACE("plist/valid_endpoint_guid: invalid source (%"PRIx32")\n", g->entityid.u);
      return DDS_RETCODE_BAD_PARAMETER;
  }
}

static dds_return_t do_guid (nn_guid_t *dst, uint64_t *present, uint64_t fl, int (*valid) (const nn_guid_t *g, const struct dd *dd), const struct dd *dd)
{
  if (dd->bufsz < sizeof (*dst))
  {
    DDS_TRACE("plist/do_guid: buffer too small\n");
    return DDS_RETCODE_BAD_PARAMETER;
  }
  memcpy (dst, dd->buf, sizeof (*dst));
  *dst = nn_ntoh_guid (*dst);
  if (valid (dst, dd) < 0)
  {
    /* CoreDX once upon a time used to send out PARTICIPANT_GUID parameters with a 0 entity id, but it
       that has long since changed (even if I don't know exactly when) */
    if (fl == PP_PARTICIPANT_GUID && vendor_is_twinoaks (dd->vendorid) && dst->entityid.u == 0 && ! NN_STRICT_P)
    {
      DDS_LOG(DDS_LC_DISCOVERY, "plist(vendor %u.%u): rewriting invalid participant guid "PGUIDFMT,
              dd->vendorid.id[0], dd->vendorid.id[1], PGUID (*dst));
      dst->entityid.u = NN_ENTITYID_PARTICIPANT;
    }
    else
    {
      return DDS_RETCODE_BAD_PARAMETER;
    }
  }
  *present |= fl;
  return 0;
}


static void bswap_prismtech_participant_version_info (nn_prismtech_participant_version_info_t *pvi)
{
  int i;
  pvi->version = bswap4u (pvi->version);
  pvi->flags = bswap4u (pvi->flags);
  for (i = 0; i < 3; i++)
      pvi->unused[i] = bswap4u (pvi->unused[i]);
}

static dds_return_t do_prismtech_participant_version_info (nn_prismtech_participant_version_info_t *pvi, uint64_t *present, uint64_t *aliased, const struct dd *dd)
{
  if (!vendor_is_eclipse_or_prismtech (dd->vendorid))
    return 0;
  else if (dd->bufsz < NN_PRISMTECH_PARTICIPANT_VERSION_INFO_FIXED_CDRSIZE)
  {
    DDS_TRACE("plist/do_prismtech_participant_version_info[pid=PRISMTECH_PARTICIPANT_VERSION_INFO]: buffer too small\n");
    return DDS_RETCODE_BAD_PARAMETER;
  }
  else
  {
    dds_return_t res;
    uint32_t sz = NN_PRISMTECH_PARTICIPANT_VERSION_INFO_FIXED_CDRSIZE - sizeof(uint32_t);
    uint32_t *pu = (uint32_t *)dd->buf;
    size_t len;
    struct dd dd1 = *dd;

    memcpy (pvi, dd->buf, sz);
    if (dd->bswap)
      bswap_prismtech_participant_version_info(pvi);

    dd1.buf = (unsigned char *) &pu[5];
    dd1.bufsz = dd->bufsz - sz;
    if ((res = alias_string ((const unsigned char **) &pvi->internals, &dd1, &len)) >= 0) {
      *present |= PP_PRISMTECH_PARTICIPANT_VERSION_INFO;
      *aliased |= PP_PRISMTECH_PARTICIPANT_VERSION_INFO;
      res = 0;
    }

    return res;
  }
}

static dds_return_t do_subscription_keys_qospolicy (dds_subscription_keys_qospolicy_t *q, uint64_t *present, uint64_t *aliased, uint64_t fl, const struct dd *dd)
{
  struct dd dd1;
  dds_return_t res;
  if (dd->bufsz < 4)
  {
    DDS_TRACE("plist/do_subscription_keys: buffer too small\n");
    return DDS_RETCODE_BAD_PARAMETER;
  }
  q->use_key_list = (unsigned char) dd->buf[0];
  if (q->use_key_list != 0 && q->use_key_list != 1)
  {
    DDS_TRACE("plist/do_subscription_keys: invalid use_key_list (%d)\n", (int) q->use_key_list);
    return DDS_RETCODE_BAD_PARAMETER;
  }
  dd1 = *dd;
  dd1.buf += 4;
  dd1.bufsz -= 4;
  if ((res = alias_stringseq (&q->key_list, &dd1)) >= 0)
  {
    *present |= fl;
    *aliased |= fl;
  }
  return res;
}

static dds_return_t unalias_subscription_keys_qospolicy (dds_subscription_keys_qospolicy_t *q, int bswap)
{
  return unalias_stringseq (&q->key_list, bswap);
}

static dds_return_t do_reader_lifespan_qospolicy (dds_reader_lifespan_qospolicy_t *q, uint64_t *present, uint64_t fl, const struct dd *dd)
{
  dds_external_reader_lifespan_qospolicy_t qext;
  dds_return_t res;
  if (dd->bufsz < sizeof (qext))
  {
    DDS_TRACE("plist/do_reader_lifespan: buffer too small\n");
    return DDS_RETCODE_BAD_PARAMETER;
  }
  memcpy (&qext, dd->buf, sizeof (qext));
  if (dd->bswap)
    bswap_external_duration (&qext.duration);
  if (qext.use_lifespan != 0 && qext.use_lifespan != 1)
  {
    DDS_TRACE("plist/do_reader_lifespan: invalid use_lifespan (%d)\n", (int) qext.use_lifespan);
    return DDS_RETCODE_BAD_PARAMETER;
  }
  if ((res = validate_external_duration (&qext.duration)) < 0)
    return res;
  q->use_lifespan = qext.use_lifespan;
  q->duration = nn_from_ddsi_duration (qext.duration);
  *present |= fl;
  return 0;
}

static dds_return_t do_entity_factory_qospolicy (dds_entity_factory_qospolicy_t *q, uint64_t *present, uint64_t fl, const struct dd *dd)
{
  if (dd->bufsz < sizeof (*q))
  {
    DDS_TRACE("plist/do_entity_factory: buffer too small\n");
    return DDS_RETCODE_BAD_PARAMETER;
  }
  q->autoenable_created_entities = dd->buf[0];
  if (q->autoenable_created_entities != 0 && q->autoenable_created_entities != 1)
  {
    DDS_TRACE("plist/do_entity_factory: invalid autoenable_created_entities (%d)\n", (int) q->autoenable_created_entities);
    return DDS_RETCODE_BAD_PARAMETER;
  }
  *present |= fl;
  return 0;
}

static dds_return_t validate_external_reader_data_lifecycle (const dds_external_reader_data_lifecycle_qospolicy_t *q)
{
  if (validate_external_duration (&q->autopurge_nowriter_samples_delay) < 0 ||
      validate_external_duration (&q->autopurge_disposed_samples_delay) < 0)
  {
    DDS_TRACE("plist/init_one_parameter[pid=PRISMTECH_READER_DATA_LIFECYCLE]: invalid autopurge_nowriter_sample_delay or autopurge_disposed_samples_delay\n");
    return DDS_RETCODE_BAD_PARAMETER;
  }
  return 0;
}

dds_return_t validate_reader_data_lifecycle (const dds_reader_data_lifecycle_qospolicy_t *q)
{
  dds_external_reader_data_lifecycle_qospolicy_t qext;
  qext.autopurge_disposed_samples_delay = nn_to_ddsi_duration (q->autopurge_disposed_samples_delay);
  qext.autopurge_nowriter_samples_delay = nn_to_ddsi_duration (q->autopurge_nowriter_samples_delay);
  return validate_external_reader_data_lifecycle (&qext);
}

static dds_return_t do_reader_data_lifecycle (dds_reader_data_lifecycle_qospolicy_t *q, const struct dd *dd)
{
  dds_external_reader_data_lifecycle_qospolicy_t qext;
  dds_return_t ret;
  memcpy (&qext, dd->buf, sizeof (qext));
  if (dd->bswap)
  {
    bswap_external_duration (&qext.autopurge_nowriter_samples_delay);
    bswap_external_duration (&qext.autopurge_disposed_samples_delay);
  }
  if ((ret = validate_external_reader_data_lifecycle (&qext)) < 0)
    return ret;
  q->autopurge_disposed_samples_delay = nn_from_ddsi_duration (qext.autopurge_disposed_samples_delay);
  q->autopurge_nowriter_samples_delay = nn_from_ddsi_duration (qext.autopurge_nowriter_samples_delay);
  return 0;
}

static dds_return_t init_one_parameter
(
  nn_plist_t *dest,
  nn_ipaddress_params_tmp_t *dest_tmp,
  uint64_t pwanted,
  uint64_t qwanted,
  uint16_t pid,
  const struct dd *dd
)
{
  dds_return_t res;
  switch (pid)
  {
    case PID_PAD:
    case PID_SENTINEL:
      return 0;

      /* Extended QoS data: */
#define Q(NAME_, prefix_, name_) case PID_##NAME_:                      \
    if (dd->bufsz < sizeof (prefix_##_##name_##_qospolicy_t))           \
    {                                                                   \
      DDS_TRACE("plist/init_one_parameter[pid=%s]: buffer too small\n", #NAME_); \
      return DDS_RETCODE_BAD_PARAMETER;                                          \
    }                                                                   \
    else                                                                \
    {                                                                   \
      prefix_##_##name_##_qospolicy_t *q = &dest->qos.name_;            \
      memcpy (q, dd->buf, sizeof (*q));                                 \
      if (dd->bswap) bswap_##name_##_qospolicy (q);                     \
      if ((res = validate_##name_##_qospolicy (q)) < 0)                 \
        return res;                                                     \
      dest->qos.present |= QP_##NAME_;                                  \
    }                                                                   \
    return 0
      Q (DURABILITY, dds, durability);
      Q (DESTINATION_ORDER, dds, destination_order);
      Q (HISTORY, dds, history);
      Q (RESOURCE_LIMITS, dds, resource_limits);
      Q (OWNERSHIP, dds, ownership);
      Q (OWNERSHIP_STRENGTH, dds, ownership_strength);
      Q (PRESENTATION, dds, presentation);
      Q (TRANSPORT_PRIORITY, dds, transport_priority);
#undef Q

    case PID_LIVELINESS:
      if (dd->bufsz < sizeof (dds_external_liveliness_qospolicy_t))
      {
        DDS_TRACE("plist/init_one_parameter[pid=LIVELINESS]: buffer too small\n");
        return DDS_RETCODE_BAD_PARAMETER;
      }
      else
      {
        dds_external_liveliness_qospolicy_t qext;
        dds_liveliness_qospolicy_t *q = &dest->qos.liveliness;
        memcpy (&qext, dd->buf, sizeof (qext));
        if (dd->bswap)
          bswap_external_liveliness_qospolicy (&qext);
        if ((res = validate_external_liveliness_qospolicy (&qext)) < 0)
          return res;
        q->kind = qext.kind;
        q->lease_duration = nn_from_ddsi_duration (qext.lease_duration);
        dest->qos.present |= QP_LIVELINESS;
      }
      return 0;

    case PID_DURABILITY_SERVICE:
      if (dd->bufsz < sizeof (dds_external_durability_service_qospolicy_t))
      {
        DDS_TRACE("plist/init_one_parameter[pid=DURABILITY_SERVICE]: buffer too small\n");
        return DDS_RETCODE_BAD_PARAMETER;
      }
      else
      {
        dds_external_durability_service_qospolicy_t qext;
        dds_durability_service_qospolicy_t *q = &dest->qos.durability_service;
        /* All-zero durability service is illegal, but at least CoreDX sometimes advertises
           it in some harmless cases. So accept all-zero durability service, then handle it
           in final_validation, where we can determine whether it really is harmless or not */
        memcpy (&qext, dd->buf, sizeof (qext));
        if (dd->bswap)
          bswap_external_durability_service_qospolicy (&qext);
        if ((res = validate_external_durability_service_qospolicy_acceptzero (&qext, true)) < 0)
          return res;
        q->history = qext.history;
        q->resource_limits = qext.resource_limits;
        q->service_cleanup_delay = nn_from_ddsi_duration (qext.service_cleanup_delay);
        dest->qos.present |= QP_DURABILITY_SERVICE;
      }
      return 0;

      /* PID_RELIABILITY handled differently because it (formally, for
         static typing reasons) has a different type on the network
         than internally, with the transformation between the two
         dependent on wheter we are being pedantic.  If that weren't
         the case, it would've been an ordinary Q (RELIABILITY,
         reliability). */
    case PID_RELIABILITY:
      if (dd->bufsz < sizeof (dds_external_reliability_qospolicy_t))
      {
        DDS_TRACE("plist/init_one_parameter[pid=RELIABILITY]: buffer too small\n");
        return DDS_RETCODE_BAD_PARAMETER;
      }
      else
      {
        dds_reliability_qospolicy_t *q = &dest->qos.reliability;
        dds_external_reliability_qospolicy_t qext;
        memcpy (&qext, dd->buf, sizeof (qext));
        if (dd->bswap)
          bswap_external_reliability_qospolicy (&qext);
        if ((res = validate_xform_reliability_qospolicy (q, &qext)) < 0)
          return res;
        dest->qos.present |= QP_RELIABILITY;
      }
      return 0;

    case PID_TOPIC_NAME:
      return do_string (&dest->qos.topic_name, &dest->qos.present, &dest->qos.aliased, qwanted, QP_TOPIC_NAME, dd);
    case PID_TYPE_NAME:
      return do_string (&dest->qos.type_name, &dest->qos.present, &dest->qos.aliased, qwanted, QP_TYPE_NAME, dd);

    case PID_USER_DATA:
      return do_octetseq (&dest->qos.user_data, &dest->qos.present, &dest->qos.aliased, qwanted, QP_USER_DATA, dd);
    case PID_GROUP_DATA:
      return do_octetseq (&dest->qos.group_data, &dest->qos.present, &dest->qos.aliased, qwanted, QP_GROUP_DATA, dd);
    case PID_TOPIC_DATA:
      return do_octetseq (&dest->qos.topic_data, &dest->qos.present, &dest->qos.aliased, qwanted, QP_TOPIC_DATA, dd);

    case PID_DEADLINE:
      return do_duration (&dest->qos.deadline.deadline, &dest->qos.present, QP_DEADLINE, dd);
    case PID_LATENCY_BUDGET:
      return do_duration (&dest->qos.latency_budget.duration, &dest->qos.present, QP_LATENCY_BUDGET, dd);
    case PID_LIFESPAN:
      return do_duration (&dest->qos.lifespan.duration, &dest->qos.present, QP_LIFESPAN, dd);
    case PID_TIME_BASED_FILTER:
      return do_duration (&dest->qos.time_based_filter.minimum_separation, &dest->qos.present, QP_TIME_BASED_FILTER, dd);

    case PID_PARTITION:
      return do_stringseq (&dest->qos.partition, &dest->qos.present, &dest->qos.aliased, qwanted, QP_PARTITION, dd);

    case PID_PRISMTECH_READER_DATA_LIFECYCLE: /* PrismTech specific */
      {
        dds_return_t ret;
        if (!vendor_is_eclipse_or_prismtech (dd->vendorid))
          return 0;
        if (dd->bufsz >= sizeof (dds_external_reader_data_lifecycle_qospolicy_t))
          ret = do_reader_data_lifecycle (&dest->qos.reader_data_lifecycle, dd);
        else
        {
          DDS_TRACE("plist/init_one_parameter[pid=PRISMTECH_READER_DATA_LIFECYCLE]: buffer too small\n");
          ret = DDS_RETCODE_BAD_PARAMETER;
        }
        if (ret >= 0)
          dest->qos.present |= QP_PRISMTECH_READER_DATA_LIFECYCLE;
        return ret;
      }
    case PID_PRISMTECH_WRITER_DATA_LIFECYCLE: /* PrismTech specific */
      if (!vendor_is_eclipse_or_prismtech (dd->vendorid))
        return 0;
      else
      {
        dds_writer_data_lifecycle_qospolicy_t *q = &dest->qos.writer_data_lifecycle;
        if (dd->bufsz < sizeof (*q))
        {
          DDS_TRACE("plist/init_one_parameter[pid=PRISMTECH_WRITER_DATA_LIFECYCLE]: buffer too small\n");
          return DDS_RETCODE_BAD_PARAMETER;
        }
        q->autodispose_unregistered_instances = dd->buf[0];
        if (q->autodispose_unregistered_instances & ~1)
        {
          DDS_TRACE("plist/init_one_parameter[pid=PRISMTECH_WRITER_DATA_LIFECYCLE]: invalid autodispose_unregistered_instances (%d)\n", (int) q->autodispose_unregistered_instances);
          return DDS_RETCODE_BAD_PARAMETER;
        }
        dest->qos.present |= QP_PRISMTECH_WRITER_DATA_LIFECYCLE;
        return 0;
      }

      /* Other plist */
    case PID_PROTOCOL_VERSION:
      if (dd->bufsz < sizeof (nn_protocol_version_t))
      {
        DDS_TRACE("plist/init_one_parameter[pid=PROTOCOL_VERSION]: buffer too small\n");
        return DDS_RETCODE_BAD_PARAMETER;
      }
      memcpy (&dest->protocol_version, dd->buf, sizeof (dest->protocol_version));
      if (NN_STRICT_P &&
          (dest->protocol_version.major != dd->protocol_version.major ||
           dest->protocol_version.minor != dd->protocol_version.minor))
      {
        /* Not accepting a submessage advertising a protocol version
           other than that advertised by the message header, unless I
           have good reason to, at least not when being strict. */
        DDS_TRACE("plist/init_one_parameter[pid=PROTOCOL_VERSION,mode=STRICT]: version (%u.%u) mismatch with message (%u.%u)\n",
                dest->protocol_version.major, dest->protocol_version.minor,
                dd->protocol_version.major, dd->protocol_version.minor);
        return DDS_RETCODE_BAD_PARAMETER;
      }
      dest->present |= PP_PROTOCOL_VERSION;
      return 0;

    case PID_VENDORID:
      if (dd->bufsz < sizeof (nn_vendorid_t))
        return DDS_RETCODE_BAD_PARAMETER;
      memcpy (&dest->vendorid, dd->buf, sizeof (dest->vendorid));
      if (NN_STRICT_P &&
          (dest->vendorid.id[0] != dd->vendorid.id[0] ||
           dest->vendorid.id[1] != dd->vendorid.id[1]))
      {
        /* see PROTOCOL_VERSION */
        DDS_TRACE("plist/init_one_parameter[pid=VENDORID,mode=STRICT]: vendor (%u.%u) mismatch with message (%u.%u)\n",
                dest->vendorid.id[0], dest->vendorid.id[1], dd->vendorid.id[0], dd->vendorid.id[1]);
        return DDS_RETCODE_BAD_PARAMETER;
      }
      dest->present |= PP_VENDORID;
      return 0;

      /* Locators: there may be lists, so we have to allocate memory for them */
#define XL(NAME_, name_) case PID_##NAME_##_LOCATOR: return do_locator (&dest->name_##_locators, &dest->present, pwanted, PP_##NAME_##_LOCATOR, dd)
      XL (UNICAST, unicast);
      XL (MULTICAST, multicast);
      XL (DEFAULT_UNICAST, default_unicast);
      XL (DEFAULT_MULTICAST, default_multicast);
      XL (METATRAFFIC_UNICAST, metatraffic_unicast);
      XL (METATRAFFIC_MULTICAST, metatraffic_multicast);
#undef XL

      /* IPADDRESS + PORT entries are a nuisance ... I'd prefer
         converting them to locators right away, so that the rest of
         the code only has to deal with locators, but that is
         impossible because the locators require both the address &
         the port to be known.

         The wireshark dissector suggests IPvAdress_t is just the 32
         bits of the IP address but it doesn't say so anywhere
         ... Similarly for ports, but contrary to the expections they
         seem to be 32-bits, too. Apparently in host-endianness.

         And, to be honest, I have no idea what port to use for
         MULTICAST_IPADDRESS ... */
#define XA(NAME_) case PID_##NAME_##_IPADDRESS: return do_ipv4address (dest, dest_tmp, pwanted, PPTMP_##NAME_##_IPADDRESS, dd)
#define XP(NAME_) case PID_##NAME_##_PORT: return do_port (dest, dest_tmp, pwanted, PPTMP_##NAME_##_PORT, dd)
      XA (MULTICAST);
      XA (DEFAULT_UNICAST);
      XP (DEFAULT_UNICAST);
      XA (METATRAFFIC_UNICAST);
      XP (METATRAFFIC_UNICAST);
      XA (METATRAFFIC_MULTICAST);
      XP (METATRAFFIC_MULTICAST);
#undef XP
#undef XA

    case PID_EXPECTS_INLINE_QOS:
      if (dd->bufsz < sizeof (dest->expects_inline_qos))
      {
        DDS_TRACE("plist/init_one_parameter[pid=EXPECTS_INLINE_QOS]: buffer too small\n");
        return DDS_RETCODE_BAD_PARAMETER;
      }
      dest->expects_inline_qos = dd->buf[0];
      /* boolean: only lsb may be set */
      if (dest->expects_inline_qos & ~1)
      {
        DDS_TRACE("plist/init_one_parameter[pid=EXPECTS_INLINE_QOS]: invalid expects_inline_qos (%d)\n",
                (int) dest->expects_inline_qos);
        return DDS_RETCODE_BAD_PARAMETER;
      }
      dest->present |= PP_EXPECTS_INLINE_QOS;
      return 0;

    case PID_PARTICIPANT_MANUAL_LIVELINESS_COUNT:
      /* Spec'd as "incremented monotonically" (DDSI 2.1, table 8.13),
         but 32 bits signed is not such a smart choice for that. We'll
         simply accept any value. */
      if (dd->bufsz < sizeof (dest->participant_manual_liveliness_count))
      {
        DDS_TRACE("plist/init_one_parameter[pid=PARTICIPANT_MANUAL_LIVELINESS_COUNT]: buffer too small\n");
        return DDS_RETCODE_BAD_PARAMETER;
      }
      memcpy (&dest->participant_manual_liveliness_count, dd->buf, sizeof (dest->participant_manual_liveliness_count));
      if (dd->bswap)
        dest->participant_manual_liveliness_count = bswap4 (dest->participant_manual_liveliness_count);
      dest->present |= PP_PARTICIPANT_MANUAL_LIVELINESS_COUNT;
      return 0;

    case PID_PARTICIPANT_LEASE_DURATION:
      return do_duration (&dest->participant_lease_duration, &dest->present, PP_PARTICIPANT_LEASE_DURATION, dd);

    case PID_CONTENT_FILTER_PROPERTY:
      /* FIXME */
      return 0;

    case PID_PARTICIPANT_GUID:
      return do_guid (&dest->participant_guid, &dest->present, PP_PARTICIPANT_GUID, valid_participant_guid, dd);

    case PID_GROUP_GUID:
      return do_guid (&dest->group_guid, &dest->present, PP_GROUP_GUID, valid_group_guid, dd);

    case PID_PARTICIPANT_ENTITYID:
    case PID_GROUP_ENTITYID:
      /* DDSI 2.1 table 9.13: reserved for future use */
      return 0;

    case PID_PARTICIPANT_BUILTIN_ENDPOINTS:
      /* FIXME: I assume it is the same as the BUILTIN_ENDPOINT_SET,
         which is the set that DDSI2 has been using so far. */
      /* FALLS THROUGH */
    case PID_BUILTIN_ENDPOINT_SET:
      if (dd->bufsz < sizeof (dest->builtin_endpoint_set))
      {
        DDS_TRACE("plist/init_one_parameter[pid=BUILTIN_ENDPOINT_SET(%u)]: buffer too small\n", pid);
        return DDS_RETCODE_BAD_PARAMETER;
      }
      memcpy (&dest->builtin_endpoint_set, dd->buf, sizeof (dest->builtin_endpoint_set));
      if (dd->bswap)
        dest->builtin_endpoint_set = bswap4u (dest->builtin_endpoint_set);
      if (NN_STRICT_P && !protocol_version_is_newer (dd->protocol_version) &&
          (dest->builtin_endpoint_set & ~(NN_DISC_BUILTIN_ENDPOINT_PARTICIPANT_ANNOUNCER |
                                          NN_DISC_BUILTIN_ENDPOINT_PARTICIPANT_DETECTOR |
                                          NN_DISC_BUILTIN_ENDPOINT_PUBLICATION_ANNOUNCER |
                                          NN_DISC_BUILTIN_ENDPOINT_PUBLICATION_DETECTOR |
                                          NN_DISC_BUILTIN_ENDPOINT_SUBSCRIPTION_ANNOUNCER |
                                          NN_DISC_BUILTIN_ENDPOINT_SUBSCRIPTION_DETECTOR |
                                          NN_DISC_BUILTIN_ENDPOINT_TOPIC_ANNOUNCER |
                                          NN_DISC_BUILTIN_ENDPOINT_TOPIC_DETECTOR |
                                          /* undefined ones: */
                                          NN_DISC_BUILTIN_ENDPOINT_PARTICIPANT_PROXY_ANNOUNCER |
                                          NN_DISC_BUILTIN_ENDPOINT_PARTICIPANT_PROXY_DETECTOR |
                                          NN_DISC_BUILTIN_ENDPOINT_PARTICIPANT_STATE_ANNOUNCER |
                                          NN_DISC_BUILTIN_ENDPOINT_PARTICIPANT_STATE_DETECTOR |
                                          /* defined ones again: */
                                          NN_BUILTIN_ENDPOINT_PARTICIPANT_MESSAGE_DATA_WRITER |
                                          NN_BUILTIN_ENDPOINT_PARTICIPANT_MESSAGE_DATA_READER)) != 0)
      {
        DDS_TRACE("plist/init_one_parameter[pid=BUILTIN_ENDPOINT_SET(%u),mode=STRICT,proto=%u.%u]: invalid set (0x%x)\n",
                pid, dd->protocol_version.major, dd->protocol_version.minor, dest->builtin_endpoint_set);
        return DDS_RETCODE_BAD_PARAMETER;
      }
      dest->present |= PP_BUILTIN_ENDPOINT_SET;
      return 0;

    case PID_PRISMTECH_BUILTIN_ENDPOINT_SET:
      if (!vendor_is_eclipse_or_prismtech (dd->vendorid))
        return 0;
      else if (dd->bufsz < sizeof (dest->prismtech_builtin_endpoint_set))
      {
        DDS_TRACE("plist/init_one_parameter[pid=PRISMTECH_BUILTIN_ENDPOINT_SET(%u)]: buffer too small\n", pid);
        return DDS_RETCODE_BAD_PARAMETER;
      }
      else
      {
        memcpy (&dest->prismtech_builtin_endpoint_set, dd->buf, sizeof (dest->prismtech_builtin_endpoint_set));
        if (dd->bswap)
          dest->prismtech_builtin_endpoint_set = bswap4u (dest->prismtech_builtin_endpoint_set);
        dest->present |= PP_PRISMTECH_BUILTIN_ENDPOINT_SET;
      }
      return 0;

    case PID_PROPERTY_LIST:
    case PID_TYPE_MAX_SIZE_SERIALIZED:
      /* FIXME */
      return 0;

    case PID_ENTITY_NAME:
      return do_string (&dest->entity_name, &dest->present, &dest->aliased, pwanted, PP_ENTITY_NAME, dd);

    case PID_KEYHASH:
      if (dd->bufsz < sizeof (dest->keyhash))
      {
        DDS_TRACE("plist/init_one_parameter[pid=KEYHASH]: buffer too small\n");
        return DDS_RETCODE_BAD_PARAMETER;
      }
      memcpy (&dest->keyhash, dd->buf, sizeof (dest->keyhash));
      dest->present |= PP_KEYHASH;
      return 0;

    case PID_STATUSINFO:
      if (dd->bufsz < sizeof (dest->statusinfo))
      {
        DDS_TRACE("plist/init_one_parameter[pid=STATUSINFO]: buffer too small\n");
        return DDS_RETCODE_BAD_PARAMETER;
      }
      memcpy (&dest->statusinfo, dd->buf, sizeof (dest->statusinfo));
      dest->statusinfo = fromBE4u (dest->statusinfo);
      if (NN_STRICT_P && !protocol_version_is_newer (dd->protocol_version) &&
          (dest->statusinfo & ~NN_STATUSINFO_STANDARDIZED))
      {
        /* Spec says I may not interpret the reserved bits. But no-one
           may use them in this version of the specification */
        DDS_TRACE("plist/init_one_parameter[pid=STATUSINFO,mode=STRICT,proto=%u.%u]: invalid statusinfo (0x%x)\n",
                dd->protocol_version.major, dd->protocol_version.minor, dest->statusinfo);
        return DDS_RETCODE_BAD_PARAMETER;
      }
      /* Clear all bits we don't understand, then add the extended bits if present */
      dest->statusinfo &= NN_STATUSINFO_STANDARDIZED;
      if (dd->bufsz >= 2 * sizeof (dest->statusinfo) && vendor_is_eclipse_or_opensplice(dd->vendorid))
      {
        uint32_t statusinfox;
        DDSRT_STATIC_ASSERT_CODE (sizeof(statusinfox) == sizeof(dest->statusinfo));
        memcpy (&statusinfox, dd->buf + sizeof (dest->statusinfo), sizeof (statusinfox));
        statusinfox = fromBE4u (statusinfox);
        if (statusinfox & NN_STATUSINFOX_OSPL_AUTO)
          dest->statusinfo |= NN_STATUSINFO_OSPL_AUTO;
      }
      dest->present |= PP_STATUSINFO;
      return 0;

    case PID_COHERENT_SET:
      if (dd->bufsz < sizeof (dest->coherent_set_seqno))
      {
        DDS_TRACE("plist/init_one_parameter[pid=COHERENT_SET]: buffer too small\n");
        return DDS_RETCODE_BAD_PARAMETER;
      }
      else
      {
        nn_sequence_number_t *q = &dest->coherent_set_seqno;
        seqno_t seqno;
        memcpy (q, dd->buf, sizeof (*q));
        if (dd->bswap)
        {
          q->high = bswap4 (q->high);
          q->low = bswap4u (q->low);
        }
        seqno = fromSN(dest->coherent_set_seqno);
        if (seqno <= 0 && seqno != NN_SEQUENCE_NUMBER_UNKNOWN)
        {
          DDS_TRACE("plist/init_one_parameter[pid=COHERENT_SET]: invalid sequence number (%" PRId64 ")\n", seqno);
          return DDS_RETCODE_BAD_PARAMETER;
        }
        dest->present |= PP_COHERENT_SET;
        return 0;
      }

    case PID_CONTENT_FILTER_INFO:
    case PID_DIRECTED_WRITE:
    case PID_ORIGINAL_WRITER_INFO:
      /* FIXME */
      return 0;

    case PID_ENDPOINT_GUID:
      if (NN_PEDANTIC_P && !protocol_version_is_newer (dd->protocol_version))
      {
        /* ENDPOINT_GUID is not specified in the 2.1 standard, so
           reject it: in (really) strict mode we do not accept
           undefined things, even though we are -arguably- supposed to
           ignore it. */
        DDS_TRACE("plist/init_one_parameter[pid=ENDPOINT_GUID,mode=PEDANTIC,proto=%u.%u]: undefined pid\n",
                dd->protocol_version.major, dd->protocol_version.minor);
        return DDS_RETCODE_BAD_PARAMETER;
      }
      return do_guid (&dest->endpoint_guid, &dest->present, PP_ENDPOINT_GUID, valid_endpoint_guid, dd);

    case PID_PRISMTECH_ENDPOINT_GUID: /* case PID_RTI_TYPECODE: */
      if (vendor_is_eclipse_or_prismtech (dd->vendorid))
      {
        /* PrismTech specific variant of ENDPOINT_GUID, for strict compliancy */
        return do_guid (&dest->endpoint_guid, &dest->present, PP_ENDPOINT_GUID, valid_endpoint_guid, dd);
      }
      else if (vendor_is_rti (dd->vendorid))
      {
        /* For RTI it is a typecode */
        return do_blob (&dest->qos.rti_typecode, &dest->qos.present, &dest->qos.aliased, qwanted, QP_RTI_TYPECODE, dd);

      }
      else
      {
        return 0;
      }

    case PID_PRISMTECH_PARTICIPANT_VERSION_INFO:
      return do_prismtech_participant_version_info(&dest->prismtech_participant_version_info, &dest->present, &dest->aliased, dd);

    case PID_PRISMTECH_SUBSCRIPTION_KEYS:
      if (!vendor_is_eclipse_or_prismtech (dd->vendorid))
        return 0;
      return do_subscription_keys_qospolicy (&dest->qos.subscription_keys, &dest->qos.present, &dest->qos.aliased, QP_PRISMTECH_SUBSCRIPTION_KEYS, dd);

    case PID_PRISMTECH_READER_LIFESPAN:
      if (!vendor_is_eclipse_or_prismtech (dd->vendorid))
        return 0;
      return do_reader_lifespan_qospolicy (&dest->qos.reader_lifespan, &dest->qos.present, QP_PRISMTECH_READER_LIFESPAN, dd);

    case PID_PRISMTECH_ENTITY_FACTORY:
      if (!vendor_is_eclipse_or_prismtech (dd->vendorid))
        return 0;
      return do_entity_factory_qospolicy (&dest->qos.entity_factory, &dest->qos.present, QP_PRISMTECH_ENTITY_FACTORY, dd);

    case PID_PRISMTECH_NODE_NAME:
      if (!vendor_is_eclipse_or_prismtech (dd->vendorid))
        return 0;
      return do_string (&dest->node_name, &dest->present, &dest->aliased, pwanted, PP_PRISMTECH_NODE_NAME, dd);

    case PID_PRISMTECH_EXEC_NAME:
      if (!vendor_is_eclipse_or_prismtech (dd->vendorid))
        return 0;
      return do_string (&dest->exec_name, &dest->present, &dest->aliased, pwanted, PP_PRISMTECH_EXEC_NAME, dd);

    case PID_PRISMTECH_SERVICE_TYPE:
      if (!vendor_is_eclipse_or_prismtech (dd->vendorid))
        return 0;
      if (dd->bufsz < sizeof (dest->service_type))
      {
        DDS_TRACE("plist/init_one_parameter[pid=PRISMTECH_SERVICE_TYPE]: buffer too small\n");
        return DDS_RETCODE_BAD_PARAMETER;
      }
      memcpy (&dest->service_type, dd->buf, sizeof (dest->service_type));
      if (dd->bswap)
        dest->service_type = bswap4u (dest->service_type);
      dest->present |= PP_PRISMTECH_SERVICE_TYPE;
      return 0;

    case PID_PRISMTECH_PROCESS_ID:
      if (!vendor_is_eclipse_or_prismtech (dd->vendorid))
        return 0;
      if (dd->bufsz < sizeof (dest->process_id))
      {
        DDS_TRACE("plist/init_one_parameter[pid=PRISMTECH_PROCESS_ID]: buffer too small\n");
        return DDS_RETCODE_BAD_PARAMETER;
      }
      memcpy (&dest->process_id, dd->buf, sizeof (dest->process_id));
      if (dd->bswap)
        dest->process_id = bswap4u (dest->process_id);
      dest->present |= PP_PRISMTECH_PROCESS_ID;
      return 0;

    case PID_PRISMTECH_TYPE_DESCRIPTION:
      if (!vendor_is_eclipse_or_prismtech (dd->vendorid))
        return 0;
      return do_string (&dest->type_description, &dest->present, &dest->aliased, pwanted, PP_PRISMTECH_TYPE_DESCRIPTION, dd);

    case PID_PRISMTECH_EOTINFO:
      if (!vendor_is_eclipse_or_opensplice (dd->vendorid))
        return 0;
      else if (dd->bufsz < 2*sizeof (uint32_t))
      {
        DDS_TRACE("plist/init_one_parameter[pid=PRISMTECH_EOTINFO]: buffer too small (1)\n");
        return DDS_RETCODE_BAD_PARAMETER;
      }
      else
      {
        nn_prismtech_eotinfo_t *q = &dest->eotinfo;
        uint32_t i;
        q->transactionId = ((const uint32_t *) dd->buf)[0];
        q->n = ((const uint32_t *) dd->buf)[1];
        if (dd->bswap)
        {
          q->n = bswap4u (q->n);
          q->transactionId = bswap4u (q->transactionId);
        }
        if (q->n > (dd->bufsz - 2*sizeof (uint32_t)) / sizeof (nn_prismtech_eotgroup_tid_t))
        {
          DDS_TRACE("plist/init_one_parameter[pid=PRISMTECH_EOTINFO]: buffer too small (2)\n");
          return DDS_RETCODE_BAD_PARAMETER;
        }
        if (q->n == 0)
          q->tids = NULL;
        else
          q->tids = (nn_prismtech_eotgroup_tid_t *) (dd->buf + 2*sizeof (uint32_t));
        for (i = 0; i < q->n; i++)
        {
          q->tids[i].writer_entityid.u = fromBE4u (q->tids[i].writer_entityid.u);
          if (dd->bswap)
            q->tids[i].transactionId = bswap4u (q->tids[i].transactionId);
        }
        dest->present |= PP_PRISMTECH_EOTINFO;
        dest->aliased |= PP_PRISMTECH_EOTINFO;
        if (dds_get_log_mask() & DDS_LC_PLIST)
        {
          DDS_LOG(DDS_LC_PLIST, "eotinfo: txn %"PRIu32" {", q->transactionId);
          for (i = 0; i < q->n; i++)
            DDS_LOG(DDS_LC_PLIST, " %"PRIx32":%"PRIu32, q->tids[i].writer_entityid.u, q->tids[i].transactionId);
          DDS_LOG(DDS_LC_PLIST, " }\n");
        }
        return 0;
      }

#ifdef DDSI_INCLUDE_SSM
    case PID_READER_FAVOURS_SSM:
      if (dd->bufsz < sizeof (dest->reader_favours_ssm))
      {
        DDS_TRACE("plist/init_one_parameter[pid=READER_FAVOURS_SSM]: buffer too small\n");
        return DDS_RETCODE_BAD_PARAMETER;
      }
      else
      {
        nn_reader_favours_ssm_t *rfssm = &dest->reader_favours_ssm;
        memcpy (rfssm, dd->buf, sizeof (*rfssm));
        if (dd->bswap)
          rfssm->state = bswap4u (rfssm->state);
        if (rfssm->state != 0 && rfssm->state != 1)
        {
          DDS_TRACE("plist/init_one_parameter[pid=READER_FAVOURS_SSM]: unsupported value: %u\n", rfssm->state);
          rfssm->state = 0;
        }
        dest->present |= PP_READER_FAVOURS_SSM;
        return 0;
      }
#endif

      /* Deprecated ones (used by RTI, but not relevant to DDSI) */
    case PID_PERSISTENCE:
    case PID_TYPE_CHECKSUM:
    case PID_TYPE2_NAME:
    case PID_TYPE2_CHECKSUM:
    case PID_EXPECTS_ACK:
    case PID_MANAGER_KEY:
    case PID_SEND_QUEUE_SIZE:
    case PID_RELIABILITY_ENABLED:
    case PID_VARGAPPS_SEQUENCE_NUMBER_LAST:
    case PID_RECV_QUEUE_SIZE:
    case PID_RELIABILITY_OFFERED:
      return 0;

    default:
      /* Ignore unrecognised parameters (disregarding vendor-specific
         ones, of course) if the protocol version is newer than the
         one implemented, and fail it if it isn't. I know all RFPs say
         to be tolerant in what is accepted, but that is where the
         bugs & the buffer overflows originate! */
      if (pid & PID_UNRECOGNIZED_INCOMPATIBLE_FLAG) {
        dest->present |= PP_INCOMPATIBLE;
        return DDS_RETCODE_UNSUPPORTED;
      } else if (pid & PID_VENDORSPECIFIC_FLAG) {
        return 0;
      } else if (!protocol_version_is_newer (dd->protocol_version) && NN_STRICT_P) {
        DDS_TRACE("plist/init_one_parameter[pid=%u,mode=STRICT,proto=%u.%u]: undefined paramter id\n",
                pid, dd->protocol_version.major, dd->protocol_version.minor);
        return DDS_RETCODE_BAD_PARAMETER;
      } else {
        return 0;
      }
  }

  assert (0);
  DDS_TRACE("plist/init_one_parameter: can't happen\n");
  return DDS_RETCODE_BAD_PARAMETER;
}

static void default_resource_limits (dds_resource_limits_qospolicy_t *q)
{
  q->max_instances = DDS_LENGTH_UNLIMITED;
  q->max_samples = DDS_LENGTH_UNLIMITED;
  q->max_samples_per_instance = DDS_LENGTH_UNLIMITED;
}

static void default_history (dds_history_qospolicy_t *q)
{
  q->kind = DDS_HISTORY_KEEP_LAST;
  q->depth = 1;
}

void nn_plist_init_empty (nn_plist_t *dest)
{
#ifndef NDEBUG
  memset (dest, 0, sizeof (*dest));
#endif
  dest->present = dest->aliased = 0;
  nn_xqos_init_empty (&dest->qos);
}

void nn_plist_mergein_missing (nn_plist_t *a, const nn_plist_t *b)
{
  /* Adds entries's from B to A (duplicating memory) (only those not
     present in A, obviously) */

  /* Simple ones (that don't need memory): everything but topic, type,
     partition, {group,topic|user} data */
#define CQ(fl_, name_) do {                                     \
    if (!(a->present & PP_##fl_) && (b->present & PP_##fl_)) {  \
      a->name_ = b->name_;                                      \
      a->present |= PP_##fl_;                                   \
    }                                                           \
  } while (0)
  CQ (PROTOCOL_VERSION, protocol_version);
  CQ (VENDORID, vendorid);
  CQ (EXPECTS_INLINE_QOS, expects_inline_qos);
  CQ (PARTICIPANT_MANUAL_LIVELINESS_COUNT, participant_manual_liveliness_count);
  CQ (PARTICIPANT_BUILTIN_ENDPOINTS, participant_builtin_endpoints);
  CQ (PARTICIPANT_LEASE_DURATION, participant_lease_duration);
  CQ (PARTICIPANT_GUID, participant_guid);
  CQ (ENDPOINT_GUID, endpoint_guid);
  CQ (GROUP_GUID, group_guid);
  CQ (BUILTIN_ENDPOINT_SET, builtin_endpoint_set);
  CQ (KEYHASH, keyhash);
  CQ (STATUSINFO, statusinfo);
  CQ (COHERENT_SET, coherent_set_seqno);
  CQ (PRISMTECH_SERVICE_TYPE, service_type);
  CQ (PRISMTECH_PROCESS_ID, process_id);
  CQ (PRISMTECH_BUILTIN_ENDPOINT_SET, prismtech_builtin_endpoint_set);
#ifdef DDSI_INCLUDE_SSM
  CQ (READER_FAVOURS_SSM, reader_favours_ssm);
#endif
#undef CQ

  /* For allocated ones it is Not strictly necessary to use tmp, as
     a->name_ may only be interpreted if the present flag is set, but
     this keeps a clean on failure and may thereby save us from a
     nasty surprise. */
#define CQ(fl_, name_, type_, tmp_type_) do {                   \
    if (!(a->present & PP_##fl_) && (b->present & PP_##fl_)) {  \
      tmp_type_ tmp = b->name_;                                 \
      unalias_##type_ (&tmp, -1);                               \
      a->name_ = tmp;                                           \
      a->present |= PP_##fl_;                                   \
    }                                                           \
  } while (0)
  CQ (UNICAST_LOCATOR, unicast_locators, locators, nn_locators_t);
  CQ (MULTICAST_LOCATOR, unicast_locators, locators, nn_locators_t);
  CQ (DEFAULT_UNICAST_LOCATOR, default_unicast_locators, locators, nn_locators_t);
  CQ (DEFAULT_MULTICAST_LOCATOR, default_multicast_locators, locators, nn_locators_t);
  CQ (METATRAFFIC_UNICAST_LOCATOR, metatraffic_unicast_locators, locators, nn_locators_t);
  CQ (METATRAFFIC_MULTICAST_LOCATOR, metatraffic_multicast_locators, locators, nn_locators_t);
  CQ (ENTITY_NAME, entity_name, string, char *);
  CQ (PRISMTECH_NODE_NAME, node_name, string, char *);
  CQ (PRISMTECH_EXEC_NAME, exec_name, string, char *);
  CQ (PRISMTECH_TYPE_DESCRIPTION, type_description, string, char *);
  CQ (PRISMTECH_EOTINFO, eotinfo, eotinfo, nn_prismtech_eotinfo_t);
#undef CQ
  if (!(a->present & PP_PRISMTECH_PARTICIPANT_VERSION_INFO) &&
      (b->present & PP_PRISMTECH_PARTICIPANT_VERSION_INFO))
  {
    nn_prismtech_participant_version_info_t tmp = b->prismtech_participant_version_info;
    unalias_string (&tmp.internals, -1);
    a->prismtech_participant_version_info = tmp;
    a->present |= PP_PRISMTECH_PARTICIPANT_VERSION_INFO;
  }

  nn_xqos_mergein_missing (&a->qos, &b->qos);
}

void nn_plist_copy (nn_plist_t *dst, const nn_plist_t *src)
{
  nn_plist_init_empty (dst);
  nn_plist_mergein_missing (dst, src);
}

nn_plist_t *nn_plist_dup (const nn_plist_t *src)
{
  nn_plist_t *dst;
  dst = ddsrt_malloc (sizeof (*dst));
  nn_plist_copy (dst, src);
  assert (dst->aliased == 0);
  return dst;
}

static dds_return_t final_validation (nn_plist_t *dest, nn_protocol_version_t protocol_version, nn_vendorid_t vendorid)
{
  /* Resource limits & history are related, so if only one is given,
     set the other to the default, claim it has been provided &
     validate the combination. They can't be changed afterward, so
     this is a reasonable interpretation. */
  if ((dest->qos.present & QP_HISTORY) && !(dest->qos.present & QP_RESOURCE_LIMITS))
  {
    default_resource_limits (&dest->qos.resource_limits);
    dest->qos.present |= QP_RESOURCE_LIMITS;
  }
  if (!(dest->qos.present & QP_HISTORY) && (dest->qos.present & QP_RESOURCE_LIMITS))
  {
    default_history (&dest->qos.history);
    dest->qos.present |= QP_HISTORY;
  }
  if (dest->qos.present & (QP_HISTORY | QP_RESOURCE_LIMITS))
  {
    dds_return_t res;
    assert ((dest->qos.present & (QP_HISTORY | QP_RESOURCE_LIMITS)) == (QP_HISTORY | QP_RESOURCE_LIMITS));
    if ((res = validate_history_and_resource_limits (&dest->qos.history, &dest->qos.resource_limits)) < 0)
      return res;
  }

  /* Durability service is sort-of accepted if all zeros, but only
     for some protocol versions and vendors.  We don't handle want
     to deal with that case internally. Now that all QoS have been
     parsed we know the setting of the durability QoS (the default
     is always VOLATILE), and hence we can verify that the setting
     is valid or delete it if irrelevant. */
  if (dest->qos.present & QP_DURABILITY_SERVICE)
  {
    const dds_durability_kind_t durkind = (dest->qos.present & QP_DURABILITY) ? dest->qos.durability.kind : DDS_DURABILITY_VOLATILE;
    bool acceptzero;
    /* Use a somewhat convoluted rule to decide whether or not to
       "accept" an all-zero durability service setting, to find a
       reasonable mix of strictness and compatibility */
    if (protocol_version_is_newer (protocol_version))
      acceptzero = true;
    else if (NN_STRICT_P)
      acceptzero = vendor_is_twinoaks (vendorid);
    else
      acceptzero = !vendor_is_eclipse (vendorid);
    switch (durkind)
    {
      case DDS_DURABILITY_VOLATILE:
      case DDS_DURABILITY_TRANSIENT_LOCAL:
        /* pretend we never saw it if it is all zero */
        if (acceptzero && durability_service_qospolicy_allzero (&dest->qos.durability_service))
          dest->qos.present &= ~QP_DURABILITY_SERVICE;
        break;
      case DDS_DURABILITY_TRANSIENT:
      case DDS_DURABILITY_PERSISTENT:
        break;
    }
    /* if it is still present, it must be valid */
    if (dest->qos.present & QP_DURABILITY_SERVICE)
    {
      dds_return_t res;
      if ((res = validate_durability_service_qospolicy (&dest->qos.durability_service)) < 0)
        return res;
    }
  }
  return 0;
}

dds_return_t nn_plist_init_frommsg
(
  nn_plist_t *dest,
  char **nextafterplist,
  uint64_t pwanted,
  uint64_t qwanted,
  const nn_plist_src_t *src
)
{
  const unsigned char *pl;
  struct dd dd;
  nn_ipaddress_params_tmp_t dest_tmp;

#ifndef NDEBUG
  memset (dest, 0, sizeof (*dest));
#endif

  if (nextafterplist)
    *nextafterplist = NULL;
  dd.protocol_version = src->protocol_version;
  dd.vendorid = src->vendorid;
  switch (src->encoding)
  {
    case PL_CDR_LE:
#if DDSRT_ENDIAN == DDSRT_LITTLE_ENDIAN
      dd.bswap = 0;
#else
      dd.bswap = 1;
#endif
      break;
    case PL_CDR_BE:
#if DDSRT_ENDIAN == DDSRT_LITTLE_ENDIAN
      dd.bswap = 1;
#else
      dd.bswap = 0;
#endif
      break;
    default:
      DDS_WARNING ("plist(vendor %u.%u): unknown encoding (%d)\n",
                   src->vendorid.id[0], src->vendorid.id[1], src->encoding);
      return DDS_RETCODE_BAD_PARAMETER;
  }
  nn_plist_init_empty (dest);
  dest->unalias_needs_bswap = dd.bswap;
  dest_tmp.present = 0;

  DDS_LOG(DDS_LC_PLIST, "NN_PLIST_INIT (bswap %d)\n", dd.bswap);

  pl = src->buf;
  while (pl + sizeof (nn_parameter_t) <= src->buf + src->bufsz)
  {
    nn_parameter_t *par = (nn_parameter_t *) pl;
    nn_parameterid_t pid;
    uint16_t length;
    dds_return_t res;
    /* swapping header partially based on wireshark dissector
       output, partially on intuition, and in a small part based on
       the spec */
    pid = (nn_parameterid_t) (dd.bswap ? bswap2u (par->parameterid) : par->parameterid);
    length = (uint16_t) (dd.bswap ? bswap2u (par->length) : par->length);
    if (pid == PID_SENTINEL)
    {
      /* Sentinel terminates list, the length is ignored, DDSI 9.4.2.11. */
      DDS_LOG(DDS_LC_PLIST, "%4"PRIx32" PID %"PRIx16"\n", (uint32_t) (pl - src->buf), pid);
      if ((res = final_validation (dest, src->protocol_version, src->vendorid)) < 0)
      {
        nn_plist_fini (dest);
        return DDS_RETCODE_BAD_PARAMETER;
      }
      else
      {
        pl += sizeof (*par);
        if (nextafterplist)
          *nextafterplist = (char *) pl;
        return 0;
      }
    }
    if (length > src->bufsz - sizeof (*par) - (uint32_t) (pl - src->buf))
    {
      DDS_WARNING("plist(vendor %u.%u): parameter length %"PRIu16" out of bounds\n",
                  src->vendorid.id[0], src->vendorid.id[1], length);
      nn_plist_fini (dest);
      return DDS_RETCODE_BAD_PARAMETER;
    }
    if ((length % 4) != 0) /* DDSI 9.4.2.11 */
    {
      DDS_WARNING("plist(vendor %u.%u): parameter length %"PRIu16" mod 4 != 0\n",
                  src->vendorid.id[0], src->vendorid.id[1], length);
      nn_plist_fini (dest);
      return DDS_RETCODE_BAD_PARAMETER;
    }

    if (dds_get_log_mask() & DDS_LC_PLIST)
    {
      DDS_LOG(DDS_LC_PLIST, "%4"PRIx32" PID %"PRIx16" len %"PRIu16" ", (uint32_t) (pl - src->buf), pid, length);
      log_octetseq(DDS_LC_PLIST, length, (const unsigned char *) (par + 1));
      DDS_LOG(DDS_LC_PLIST, "\n");
    }

    dd.buf = (const unsigned char *) (par + 1);
    dd.bufsz = length;
    if ((res = init_one_parameter (dest, &dest_tmp, pwanted, qwanted, pid, &dd)) < 0)
    {
      /* make sure we print a trace message on error */
      DDS_TRACE("plist(vendor %u.%u): failed at pid=%"PRIx16"\n", src->vendorid.id[0], src->vendorid.id[1], pid);
      nn_plist_fini (dest);
      return res;
    }
    pl += sizeof (*par) + length;
  }
  /* If we get here, that means we reached the end of the message
     without encountering a sentinel. That is an error */
  DDS_WARNING("plist(vendor %u.%u): invalid parameter list: sentinel missing\n",
              src->vendorid.id[0], src->vendorid.id[1]);
  nn_plist_fini (dest);
  return DDS_RETCODE_BAD_PARAMETER;
}

const unsigned char *nn_plist_findparam_native_unchecked (const void *src, nn_parameterid_t pid)
{
  /* Scans the parameter list starting at src looking just for pid, returning NULL if not found;
     no further checking is done and the input is assumed to valid and in native format.  Clearly
     this is only to be used for internally generated data -- to precise, for grabbing the key
     value from discovery data that is being sent out. */
  const nn_parameter_t *par = src;
  while (par->parameterid != pid)
  {
    if (pid == PID_SENTINEL)
      return NULL;
    par = (const nn_parameter_t *) ((const char *) (par + 1) + par->length);
  }
  return (unsigned char *) (par + 1);
}

unsigned char *nn_plist_quickscan (struct nn_rsample_info *dest, const struct nn_rmsg *rmsg, const nn_plist_src_t *src)
{
  /* Sets a few fields in dest, returns address of first byte
     following parameter list, or NULL on error.  Most errors will go
     undetected, unlike nn_plist_init_frommsg(). */
  const unsigned char *pl;
  (void)rmsg;
  dest->statusinfo = 0;
  dest->pt_wr_info_zoff = NN_OFF_TO_ZOFF (0);
  dest->complex_qos = 0;
  switch (src->encoding)
  {
    case PL_CDR_LE:
#if DDSRT_ENDIAN == DDSRT_LITTLE_ENDIAN
      dest->bswap = 0;
#else
      dest->bswap = 1;
#endif
      break;
    case PL_CDR_BE:
#if DDSRT_ENDIAN == DDSRT_LITTLE_ENDIAN
      dest->bswap = 1;
#else
      dest->bswap = 0;
#endif
      break;
    default:
      DDS_WARNING("plist(vendor %u.%u): quickscan: unknown encoding (%d)\n",
                  src->vendorid.id[0], src->vendorid.id[1], src->encoding);
      return NULL;
  }
  DDS_LOG(DDS_LC_PLIST, "NN_PLIST_QUICKSCAN (bswap %d)\n", dest->bswap);
  pl = src->buf;
  while (pl + sizeof (nn_parameter_t) <= src->buf + src->bufsz)
  {
    nn_parameter_t *par = (nn_parameter_t *) pl;
    nn_parameterid_t pid;
    uint16_t length;
    pid = (nn_parameterid_t) (dest->bswap ? bswap2u (par->parameterid) : par->parameterid);
    length = (uint16_t) (dest->bswap ? bswap2u (par->length) : par->length);
    pl += sizeof (*par);
    if (pid == PID_SENTINEL)
      return (unsigned char *) pl;
    if (length > src->bufsz - (size_t)(pl - src->buf))
    {
      DDS_WARNING("plist(vendor %u.%u): quickscan: parameter length %"PRIu16" out of bounds\n",
                  src->vendorid.id[0], src->vendorid.id[1], length);
      return NULL;
    }
    if ((length % 4) != 0) /* DDSI 9.4.2.11 */
    {
      DDS_WARNING("plist(vendor %u.%u): quickscan: parameter length %"PRIu16" mod 4 != 0\n",
                  src->vendorid.id[0], src->vendorid.id[1], length);
      return NULL;
    }
    switch (pid)
    {
      case PID_PAD:
        break;
      case PID_STATUSINFO:
        if (length < 4)
        {
          DDS_TRACE("plist(vendor %u.%u): quickscan(PID_STATUSINFO): buffer too small\n",
                    src->vendorid.id[0], src->vendorid.id[1]);
          return NULL;
        }
        else
        {
          uint32_t stinfo = fromBE4u (*((uint32_t *) pl));
          uint32_t stinfox = (length < 8 || !vendor_is_eclipse_or_opensplice(src->vendorid)) ? 0 : fromBE4u (*((uint32_t *) pl + 1));
#if (NN_STATUSINFO_DISPOSE | NN_STATUSINFO_UNREGISTER) != 3
#error "expected dispose/unregister to be in lowest 2 bits"
#endif
          dest->statusinfo = stinfo & 3u;
          if ((stinfo & ~3u) || stinfox)
            dest->complex_qos = 1;
        }
        break;
      default:
        DDS_LOG(DDS_LC_PLIST, "(pid=%"PRIx16" complex_qos=1)", pid);
        dest->complex_qos = 1;
        break;
    }
    pl += length;
  }
  /* If we get here, that means we reached the end of the message
     without encountering a sentinel. That is an error */
  DDS_WARNING("plist(vendor %u.%u): quickscan: invalid parameter list: sentinel missing\n",
              src->vendorid.id[0], src->vendorid.id[1]);
  return NULL;
}

void nn_xqos_init_empty (dds_qos_t *dest)
{
#ifndef NDEBUG
  memset (dest, 0, sizeof (*dest));
#endif
  dest->present = dest->aliased = 0;
}

void nn_plist_init_default_participant (nn_plist_t *plist)
{
  nn_plist_init_empty (plist);
  plist->qos.present |= QP_PRISMTECH_ENTITY_FACTORY;
  plist->qos.entity_factory.autoenable_created_entities = 0;
}

static void xqos_init_default_common (dds_qos_t *xqos)
{
  nn_xqos_init_empty (xqos);

  xqos->present |= QP_PARTITION;
  xqos->partition.n = 0;
  xqos->partition.strs = NULL;

  xqos->present |= QP_PRESENTATION;
  xqos->presentation.access_scope = DDS_PRESENTATION_INSTANCE;
  xqos->presentation.coherent_access = 0;
  xqos->presentation.ordered_access = 0;

  xqos->present |= QP_DURABILITY;
  xqos->durability.kind = DDS_DURABILITY_VOLATILE;

  xqos->present |= QP_DEADLINE;
  xqos->deadline.deadline = T_NEVER;

  xqos->present |= QP_LATENCY_BUDGET;
  xqos->latency_budget.duration = 0;

  xqos->present |= QP_LIVELINESS;
  xqos->liveliness.kind = DDS_LIVELINESS_AUTOMATIC;
  xqos->liveliness.lease_duration = T_NEVER;

  xqos->present |= QP_DESTINATION_ORDER;
  xqos->destination_order.kind = DDS_DESTINATIONORDER_BY_RECEPTION_TIMESTAMP;

  xqos->present |= QP_HISTORY;
  xqos->history.kind = DDS_HISTORY_KEEP_LAST;
  xqos->history.depth = 1;

  xqos->present |= QP_RESOURCE_LIMITS;
  xqos->resource_limits.max_samples = DDS_LENGTH_UNLIMITED;
  xqos->resource_limits.max_instances = DDS_LENGTH_UNLIMITED;
  xqos->resource_limits.max_samples_per_instance = DDS_LENGTH_UNLIMITED;

  xqos->present |= QP_TRANSPORT_PRIORITY;
  xqos->transport_priority.value = 0;

  xqos->present |= QP_OWNERSHIP;
  xqos->ownership.kind = DDS_OWNERSHIP_SHARED;

  xqos->present |= QP_CYCLONE_IGNORELOCAL;
  xqos->ignorelocal.value = DDS_IGNORELOCAL_NONE;
}

void nn_xqos_init_default_reader (dds_qos_t *xqos)
{
  xqos_init_default_common (xqos);

  xqos->present |= QP_RELIABILITY;
  xqos->reliability.kind = DDS_RELIABILITY_BEST_EFFORT;

  xqos->present |= QP_TIME_BASED_FILTER;
  xqos->time_based_filter.minimum_separation = 0;

  xqos->present |= QP_PRISMTECH_READER_DATA_LIFECYCLE;
  xqos->reader_data_lifecycle.autopurge_nowriter_samples_delay = T_NEVER;
  xqos->reader_data_lifecycle.autopurge_disposed_samples_delay = T_NEVER;

  xqos->present |= QP_PRISMTECH_READER_LIFESPAN;
  xqos->reader_lifespan.use_lifespan = 0;
  xqos->reader_lifespan.duration = T_NEVER;

  xqos->present |= QP_PRISMTECH_SUBSCRIPTION_KEYS;
  xqos->subscription_keys.use_key_list = 0;
  xqos->subscription_keys.key_list.n = 0;
  xqos->subscription_keys.key_list.strs = NULL;
}

void nn_xqos_init_default_writer (dds_qos_t *xqos)
{
  xqos_init_default_common (xqos);

  xqos->present |= QP_DURABILITY_SERVICE;
  xqos->durability_service.service_cleanup_delay = 0;
  xqos->durability_service.history.kind = DDS_HISTORY_KEEP_LAST;
  xqos->durability_service.history.depth = 1;
  xqos->durability_service.resource_limits.max_samples = DDS_LENGTH_UNLIMITED;
  xqos->durability_service.resource_limits.max_instances = DDS_LENGTH_UNLIMITED;
  xqos->durability_service.resource_limits.max_samples_per_instance = DDS_LENGTH_UNLIMITED;

  xqos->present |= QP_RELIABILITY;
  xqos->reliability.kind = DDS_RELIABILITY_RELIABLE;
  xqos->reliability.max_blocking_time = 100 * T_MILLISECOND;

  xqos->present |= QP_OWNERSHIP_STRENGTH;
  xqos->ownership_strength.value = 0;

  xqos->present |= QP_TRANSPORT_PRIORITY;
  xqos->transport_priority.value = 0;

  xqos->present |= QP_LIFESPAN;
  xqos->lifespan.duration = T_NEVER;

  xqos->present |= QP_PRISMTECH_WRITER_DATA_LIFECYCLE;
  xqos->writer_data_lifecycle.autodispose_unregistered_instances = 1;
}

void nn_xqos_init_default_writer_noautodispose (dds_qos_t *xqos)
{
  nn_xqos_init_default_writer (xqos);
  xqos->writer_data_lifecycle.autodispose_unregistered_instances = 0;
}

void nn_xqos_init_default_topic (dds_qos_t *xqos)
{
  xqos_init_default_common (xqos);

  xqos->present |= QP_DURABILITY_SERVICE;
  xqos->durability_service.service_cleanup_delay = 0;
  xqos->durability_service.history.kind = DDS_HISTORY_KEEP_LAST;
  xqos->durability_service.history.depth = 1;
  xqos->durability_service.resource_limits.max_samples = DDS_LENGTH_UNLIMITED;
  xqos->durability_service.resource_limits.max_instances = DDS_LENGTH_UNLIMITED;
  xqos->durability_service.resource_limits.max_samples_per_instance = DDS_LENGTH_UNLIMITED;

  xqos->present |= QP_RELIABILITY;
  xqos->reliability.kind = DDS_RELIABILITY_BEST_EFFORT;
  xqos->reliability.max_blocking_time = 100 * T_MILLISECOND;

  xqos->present |= QP_TRANSPORT_PRIORITY;
  xqos->transport_priority.value = 0;

  xqos->present |= QP_LIFESPAN;
  xqos->lifespan.duration = T_NEVER;

  xqos->present |= QP_PRISMTECH_SUBSCRIPTION_KEYS;
  xqos->subscription_keys.use_key_list = 0;
  xqos->subscription_keys.key_list.n = 0;
  xqos->subscription_keys.key_list.strs = NULL;
}

void nn_xqos_init_default_subscriber (dds_qos_t *xqos)
{
  nn_xqos_init_empty (xqos);

  xqos->present |= QP_PRISMTECH_ENTITY_FACTORY;
  xqos->entity_factory.autoenable_created_entities = 1;


  xqos->present |= QP_PARTITION;
  xqos->partition.n = 0;
  xqos->partition.strs = NULL;
}

void nn_xqos_init_default_publisher (dds_qos_t *xqos)
{
  nn_xqos_init_empty (xqos);

  xqos->present |= QP_PRISMTECH_ENTITY_FACTORY;
  xqos->entity_factory.autoenable_created_entities = 1;

  xqos->present |= QP_PARTITION;
  xqos->partition.n = 0;
  xqos->partition.strs = NULL;
}

void nn_xqos_mergein_missing (dds_qos_t *a, const dds_qos_t *b)
{
  /* Adds QoS's from B to A (duplicating memory) (only those not
     present in A, obviously) */

  /* Simple ones (that don't need memory): everything but topic, type,
     partition, {group,topic|user} data */
#define CQ(fl_, name_) do {                                     \
    if (!(a->present & QP_##fl_) && (b->present & QP_##fl_)) {  \
      a->name_ = b->name_;                                      \
      a->present |= QP_##fl_;                                   \
    }                                                           \
  } while (0)
  CQ (PRESENTATION, presentation);
  CQ (DURABILITY, durability);
  CQ (DURABILITY_SERVICE, durability_service);
  CQ (DEADLINE, deadline);
  CQ (LATENCY_BUDGET, latency_budget);
  CQ (LIVELINESS, liveliness);
  CQ (RELIABILITY, reliability);
  CQ (DESTINATION_ORDER, destination_order);
  CQ (HISTORY, history);
  CQ (RESOURCE_LIMITS, resource_limits);
  CQ (TRANSPORT_PRIORITY, transport_priority);
  CQ (LIFESPAN, lifespan);
  CQ (OWNERSHIP, ownership);
  CQ (OWNERSHIP_STRENGTH, ownership_strength);
  CQ (TIME_BASED_FILTER, time_based_filter);
  CQ (PRISMTECH_READER_DATA_LIFECYCLE, reader_data_lifecycle);
  CQ (PRISMTECH_WRITER_DATA_LIFECYCLE, writer_data_lifecycle);
  CQ (PRISMTECH_READER_LIFESPAN, reader_lifespan);
  CQ (PRISMTECH_ENTITY_FACTORY, entity_factory);
  CQ (CYCLONE_IGNORELOCAL, ignorelocal);
#undef CQ

  /* For allocated ones it is Not strictly necessary to use tmp, as
     a->name_ may only be interpreted if the present flag is set, but
     this keeps a clean on failure and may thereby save us from a
     nasty surprise. */
#define CQ(fl_, name_, type_, tmp_type_) do {                   \
    if (!(a->present & QP_##fl_) && (b->present & QP_##fl_)) {  \
      tmp_type_ tmp = b->name_;                                 \
      unalias_##type_ (&tmp, -1);                               \
      a->name_ = tmp;                                           \
      a->present |= QP_##fl_;                                   \
    }                                                           \
  } while (0)
  CQ (GROUP_DATA, group_data, octetseq, ddsi_octetseq_t);
  CQ (TOPIC_DATA, topic_data, octetseq, ddsi_octetseq_t);
  CQ (USER_DATA, user_data, octetseq, ddsi_octetseq_t);
  CQ (TOPIC_NAME, topic_name, string, char *);
  CQ (TYPE_NAME, type_name, string, char *);
  CQ (RTI_TYPECODE, rti_typecode, octetseq, ddsi_octetseq_t);
#undef CQ
  if (!(a->present & QP_PRISMTECH_SUBSCRIPTION_KEYS) && (b->present & QP_PRISMTECH_SUBSCRIPTION_KEYS))
  {
      a->subscription_keys.use_key_list = b->subscription_keys.use_key_list;
      duplicate_stringseq (&a->subscription_keys.key_list, &b->subscription_keys.key_list);
      a->present |= QP_PRISMTECH_SUBSCRIPTION_KEYS;
  }
  if (!(a->present & QP_PARTITION) && (b->present & QP_PARTITION))
  {
    duplicate_stringseq (&a->partition, &b->partition);
    a->present |= QP_PARTITION;
  }
}

void nn_xqos_copy (dds_qos_t *dst, const dds_qos_t *src)
{
  nn_xqos_init_empty (dst);
  nn_xqos_mergein_missing (dst, src);
}

void nn_xqos_unalias (dds_qos_t *xqos)
{
  DDS_LOG(DDS_LC_PLIST, "NN_XQOS_UNALIAS\n");
#define Q(name_, func_, field_) do {                                    \
    if ((xqos->present & QP_##name_) && (xqos->aliased & QP_##name_)) { \
      unalias_##func_ (&xqos->field_, -1);                              \
      xqos->aliased &= ~QP_##name_;                                     \
    }                                                                   \
  } while (0)
  Q (GROUP_DATA, octetseq, group_data);
  Q (TOPIC_DATA, octetseq, topic_data);
  Q (USER_DATA, octetseq, user_data);
  Q (TOPIC_NAME, string, topic_name);
  Q (TYPE_NAME, string, type_name);
  Q (PARTITION, stringseq, partition);
  Q (PRISMTECH_SUBSCRIPTION_KEYS, subscription_keys_qospolicy, subscription_keys);
  Q (RTI_TYPECODE, octetseq, rti_typecode);
#undef Q
  assert (xqos->aliased == 0);
}

void nn_xqos_fini (dds_qos_t *xqos)
{
  struct t { uint64_t fl; size_t off; };
  static const struct t qos_simple[] = {
    { QP_GROUP_DATA, offsetof (dds_qos_t, group_data.value) },
    { QP_TOPIC_DATA, offsetof (dds_qos_t, topic_data.value) },
    { QP_USER_DATA, offsetof (dds_qos_t, user_data.value) },
    { QP_TOPIC_NAME, offsetof (dds_qos_t, topic_name) },
    { QP_TYPE_NAME, offsetof (dds_qos_t, type_name) },
    { QP_RTI_TYPECODE, offsetof (dds_qos_t, rti_typecode.value) }
  };
  DDS_LOG(DDS_LC_PLIST, "NN_XQOS_FINI\n");
  for (size_t i = 0; i < sizeof (qos_simple) / sizeof (*qos_simple); i++)
  {
    if ((xqos->present & qos_simple[i].fl) && !(xqos->aliased & qos_simple[i].fl))
    {
      void **pp = (void **) ((char *) xqos + qos_simple[i].off);
      DDS_LOG(DDS_LC_PLIST, "NN_XQOS_FINI free %p\n", *pp);
      ddsrt_free (*pp);
    }
  }
  if (xqos->present & QP_PARTITION)
  {
    if (!(xqos->aliased & QP_PARTITION))
    {
      free_stringseq (&xqos->partition);
    }
    else
    {
      /* until proper message buffers arrive */
      DDS_LOG(DDS_LC_PLIST, "NN_XQOS_FINI free %p\n", (void *) xqos->partition.strs);
      ddsrt_free (xqos->partition.strs);
    }
  }
  if (xqos->present & QP_PRISMTECH_SUBSCRIPTION_KEYS)
  {
    if (!(xqos->aliased & QP_PRISMTECH_SUBSCRIPTION_KEYS))
      free_stringseq (&xqos->subscription_keys.key_list);
    else
    {
      /* until proper message buffers arrive */
      DDS_LOG(DDS_LC_PLIST, "NN_XQOS_FINI free %p\n", (void *) xqos->subscription_keys.key_list.strs);
      ddsrt_free (xqos->subscription_keys.key_list.strs);
    }
  }
  xqos->present = 0;
}

dds_qos_t * nn_xqos_dup (const dds_qos_t *src)
{
  dds_qos_t *dst = ddsrt_malloc (sizeof (*dst));
  nn_xqos_copy (dst, src);
  assert (dst->aliased == 0);
  return dst;
}

static int octetseqs_differ (const ddsi_octetseq_t *a, const ddsi_octetseq_t *b)
{
  return (a->length != b->length || memcmp (a->value, b->value, a->length) != 0);
}

static int stringseqs_differ (const ddsi_stringseq_t *a, const ddsi_stringseq_t *b)
{
  uint32_t i;
  if (a->n != b->n)
    return 1;
  for (i = 0; i < a->n; i++)
    if (strcmp (a->strs[i], b->strs[i]))
      return 1;
  return 0;
}

static int histories_differ (const dds_history_qospolicy_t *a, const dds_history_qospolicy_t *b)
{
  return (a->kind != b->kind || (a->kind == DDS_HISTORY_KEEP_LAST && a->depth != b->depth));
}

static int resource_limits_differ (const dds_resource_limits_qospolicy_t *a, const dds_resource_limits_qospolicy_t *b)
{
  return (a->max_samples != b->max_samples || a->max_instances != b->max_instances ||
          a->max_samples_per_instance != b->max_samples_per_instance);
}

static int partition_is_default (const dds_partition_qospolicy_t *a)
{
  uint32_t i;
  for (i = 0; i < a->n; i++)
    if (strcmp (a->strs[i], "") != 0)
      return 0;
  return 1;
}

static int partitions_equal_n2 (const dds_partition_qospolicy_t *a, const dds_partition_qospolicy_t *b)
{
  uint32_t i, j;
  for (i = 0; i < a->n; i++)
  {
    for (j = 0; j < b->n; j++)
      if (strcmp (a->strs[i], b->strs[j]) == 0)
        break;
    if (j == b->n)
      return 0;
  }
  return 1;
}

static int strcmp_wrapper (const void *a, const void *b)
{
  return strcmp (a, b);
}

static int partitions_equal_nlogn (const dds_partition_qospolicy_t *a, const dds_partition_qospolicy_t *b)
{
  char *statictab[8], **tab;
  int equal = 1;
  uint32_t i;

  if (a->n <= sizeof (statictab) / sizeof (*statictab))
    tab = statictab;
  else
    tab = ddsrt_malloc (a->n * sizeof (*tab));

  for (i = 0; i < a->n; i++)
    tab[i] = a->strs[i];
  qsort (tab, a->n, sizeof (*tab), strcmp_wrapper);
  for (i = 0; i < b->n; i++)
    if (bsearch (b->strs[i], tab, a->n, sizeof (*tab), strcmp_wrapper) == NULL)
    {
      equal = 0;
      break;
    }
  if (tab != statictab)
    ddsrt_free (tab);
  return equal;
}

static int partitions_equal (const dds_partition_qospolicy_t *a, const dds_partition_qospolicy_t *b)
{
  /* Return true iff (the set a->strs) equals (the set b->strs); that
     is, order doesn't matter. One could argue that "**" and "*" are
     equal, but we're not that precise here. */
  int b_is_def;

  if (a->n == 1 && b->n == 1)
    return (strcmp (a->strs[0], b->strs[0]) == 0);
  /* not the trivial case */
  b_is_def = partition_is_default (b);
  if (partition_is_default (a))
    return b_is_def;
  else if (b_is_def)
    return 0;

  /* Neither is default, go the expensive route. Which one depends
     on the actual number of partitions and both variants are written
     assuming that |A| >= |B|. */
  if (a->n < b->n)
  {
    const dds_partition_qospolicy_t *x = a;
    a = b;
    b = x;
  }
  if (a->n * b->n < 10)
  {
    /* for small sets, the quadratic version should be the fastest,
       the number has been pulled from thin air */
    return partitions_equal_n2 (a, b);
  }
  else
  {
    /* for larger sets, the n log(n) version should win */
    return partitions_equal_nlogn (a, b);
  }
}

uint64_t nn_xqos_delta (const dds_qos_t *a, const dds_qos_t *b, uint64_t mask)
{
  /* Returns QP_... set for RxO settings where a differs from b; if
     present in a but not in b (or in b but not in a) it counts as a
     difference. */
  uint64_t delta = (a->present ^ b->present) & mask;
  uint64_t check = (a->present & b->present) & mask;
  if (check & QP_TOPIC_NAME) {
    if (strcmp (a->topic_name, b->topic_name))
      delta |= QP_TOPIC_NAME;
  }
  if (check & QP_TYPE_NAME) {
    if (strcmp (a->type_name, b->type_name))
      delta |= QP_TYPE_NAME;
  }
  if (check & QP_PRESENTATION) {
    if (a->presentation.access_scope != b->presentation.access_scope ||
        a->presentation.coherent_access != b->presentation.coherent_access ||
        a->presentation.ordered_access != b->presentation.ordered_access)
      delta |= QP_PRESENTATION;
  }
  if (check & QP_PARTITION) {
    if (!partitions_equal (&a->partition, &b->partition))
      delta |= QP_PARTITION;
  }
  if (check & QP_GROUP_DATA) {
    if (octetseqs_differ (&a->group_data, &b->group_data))
      delta |= QP_GROUP_DATA;
  }
  if (check & QP_TOPIC_DATA) {
    if (octetseqs_differ (&a->topic_data, &b->topic_data))
      delta |= QP_TOPIC_DATA;
  }
  if (check & QP_DURABILITY) {
    if (a->durability.kind != b->durability.kind)
      delta |= QP_DURABILITY;
  }
  if (check & QP_DURABILITY_SERVICE)
  {
    const dds_durability_service_qospolicy_t *qa = &a->durability_service;
    const dds_durability_service_qospolicy_t *qb = &b->durability_service;
    if (qa->service_cleanup_delay != qb->service_cleanup_delay ||
        histories_differ (&qa->history, &qb->history) ||
        resource_limits_differ (&qa->resource_limits, &qb->resource_limits))
      delta |= QP_DURABILITY_SERVICE;
  }
  if (check & QP_DEADLINE) {
    if (a->deadline.deadline != b->deadline.deadline)
      delta |= QP_DEADLINE;
  }
  if (check & QP_LATENCY_BUDGET) {
    if (a->latency_budget.duration != b->latency_budget.duration)
      delta |= QP_LATENCY_BUDGET;
  }
  if (check & QP_LIVELINESS) {
    if (a->liveliness.kind != b->liveliness.kind || a->liveliness.lease_duration != b->liveliness.lease_duration)
      delta |= QP_LIVELINESS;
  }
  if (check & QP_RELIABILITY) {
    if (a->reliability.kind != b->reliability.kind || a->reliability.max_blocking_time != b->reliability.max_blocking_time)
      delta |= QP_RELIABILITY;
  }
  if (check & QP_DESTINATION_ORDER) {
    if (a->destination_order.kind != b->destination_order.kind)
      delta |= QP_DESTINATION_ORDER;
  }
  if (check & QP_HISTORY) {
    if (histories_differ (&a->history, &b->history))
      delta |= QP_HISTORY;
  }
  if (check & QP_RESOURCE_LIMITS) {
    if (resource_limits_differ (&a->resource_limits, &b->resource_limits))
      delta |= QP_RESOURCE_LIMITS;
  }
  if (check & QP_TRANSPORT_PRIORITY) {
    if (a->transport_priority.value != b->transport_priority.value)
      delta |= QP_TRANSPORT_PRIORITY;
  }
  if (check & QP_LIFESPAN) {
    if (a->lifespan.duration != b->lifespan.duration)
      delta |= QP_LIFESPAN;
  }
  if (check & QP_USER_DATA) {
    if (octetseqs_differ (&a->user_data, &b->user_data))
      delta |= QP_USER_DATA;
  }
  if (check & QP_OWNERSHIP) {
    if (a->ownership.kind != b->ownership.kind)
      delta |= QP_OWNERSHIP;
  }
  if (check & QP_OWNERSHIP_STRENGTH) {
    if (a->ownership_strength.value != b->ownership_strength.value)
      delta |= QP_OWNERSHIP_STRENGTH;
  }
  if (check & QP_TIME_BASED_FILTER) {
    if (a->time_based_filter.minimum_separation != b->time_based_filter.minimum_separation)
      delta |= QP_TIME_BASED_FILTER;
  }
  if (check & QP_PRISMTECH_READER_DATA_LIFECYCLE) {
    if (a->reader_data_lifecycle.autopurge_disposed_samples_delay != b->reader_data_lifecycle.autopurge_disposed_samples_delay ||
        a->reader_data_lifecycle.autopurge_nowriter_samples_delay != b->reader_data_lifecycle.autopurge_nowriter_samples_delay)
      delta |= QP_PRISMTECH_READER_DATA_LIFECYCLE;
  }
  if (check & QP_PRISMTECH_WRITER_DATA_LIFECYCLE) {
    if (a->writer_data_lifecycle.autodispose_unregistered_instances !=
        b->writer_data_lifecycle.autodispose_unregistered_instances)
      delta |= QP_PRISMTECH_WRITER_DATA_LIFECYCLE;
  }
  if (check & QP_PRISMTECH_READER_LIFESPAN) {
    /* Note: the conjunction need not test both a & b for having use_lifespan set */
    if (a->reader_lifespan.use_lifespan != b->reader_lifespan.use_lifespan ||
        (a->reader_lifespan.use_lifespan && b->reader_lifespan.use_lifespan &&
         a->reader_lifespan.duration != b->reader_lifespan.duration))
      delta |= QP_PRISMTECH_READER_LIFESPAN;
  }
  if (check & QP_PRISMTECH_SUBSCRIPTION_KEYS) {
    /* Note: the conjunction need not test both a & b for having use_lifespan set */
    if (a->subscription_keys.use_key_list != b->subscription_keys.use_key_list ||
        (a->subscription_keys.use_key_list && b->subscription_keys.use_key_list &&
         stringseqs_differ (&a->subscription_keys.key_list, &b->subscription_keys.key_list)))
      delta |= QP_PRISMTECH_SUBSCRIPTION_KEYS;
  }
  if (check & QP_PRISMTECH_ENTITY_FACTORY) {
    if (a->entity_factory.autoenable_created_entities !=
        b->entity_factory.autoenable_created_entities)
      delta |= QP_PRISMTECH_ENTITY_FACTORY;
  }
  if (check & QP_RTI_TYPECODE) {
    if (octetseqs_differ (&a->rti_typecode, &b->rti_typecode))
      delta |= QP_RTI_TYPECODE;
  }
  if (check & QP_CYCLONE_IGNORELOCAL) {
    if (a->ignorelocal.value != b->ignorelocal.value)
      delta |= QP_CYCLONE_IGNORELOCAL;
  }
  return delta;
}

/*************************/

void nn_xqos_addtomsg (struct nn_xmsg *m, const dds_qos_t *xqos, uint64_t wanted)
{
  /* Returns new nn_xmsg pointer (currently, reallocs may happen) */

  uint64_t w = xqos->present & wanted;
  char *tmp;
#define SIMPLE(name_, prefix_, field_) \
  do { \
    if (w & QP_##name_) { \
      tmp = nn_xmsg_addpar (m, PID_##name_, sizeof (xqos->field_)); \
      *((prefix_##_##field_##_qospolicy_t *) tmp) = xqos->field_; \
    } \
  } while (0)
#define FUNC_BY_REF(name_, field_, func_) \
  do { \
    if (w & QP_##name_) { \
      nn_xmsg_addpar_##func_ (m, PID_##name_, &xqos->field_); \
    } \
  } while (0)
#define FUNC_BY_VAL(name_, field_, func_) \
  do { \
    if (w & QP_##name_) { \
      nn_xmsg_addpar_##func_ (m, PID_##name_, xqos->field_); \
    } \
  } while (0)

  FUNC_BY_VAL (TOPIC_NAME, topic_name, string);
  FUNC_BY_VAL (TYPE_NAME, type_name, string);
  SIMPLE (PRESENTATION, dds, presentation);
  FUNC_BY_REF (PARTITION, partition, stringseq);
  FUNC_BY_REF (GROUP_DATA, group_data, octetseq);
  FUNC_BY_REF (TOPIC_DATA, topic_data, octetseq);
  SIMPLE (DURABILITY, dds, durability);
  FUNC_BY_REF (DURABILITY_SERVICE, durability_service, durability_service);
  FUNC_BY_VAL (DEADLINE, deadline.deadline, duration);
  FUNC_BY_VAL (LATENCY_BUDGET, latency_budget.duration, duration);
  FUNC_BY_REF (LIVELINESS, liveliness, liveliness);
  FUNC_BY_REF (RELIABILITY, reliability, reliability);
  SIMPLE (DESTINATION_ORDER, dds, destination_order);
  SIMPLE (HISTORY, dds, history);
  SIMPLE (RESOURCE_LIMITS, dds, resource_limits);
  SIMPLE (TRANSPORT_PRIORITY, dds, transport_priority);
  FUNC_BY_VAL (LIFESPAN, lifespan.duration, duration);
  FUNC_BY_REF (USER_DATA, user_data, octetseq);
  SIMPLE (OWNERSHIP, dds, ownership);
  SIMPLE (OWNERSHIP_STRENGTH, dds, ownership_strength);
  FUNC_BY_VAL (TIME_BASED_FILTER, time_based_filter.minimum_separation, duration);
  FUNC_BY_REF (PRISMTECH_READER_DATA_LIFECYCLE, reader_data_lifecycle, reader_data_lifecycle);
  SIMPLE (PRISMTECH_WRITER_DATA_LIFECYCLE, dds, writer_data_lifecycle);
  FUNC_BY_REF (PRISMTECH_READER_LIFESPAN, reader_lifespan, reader_lifespan);
  FUNC_BY_REF (PRISMTECH_SUBSCRIPTION_KEYS, subscription_keys, subscription_keys);
  SIMPLE (PRISMTECH_ENTITY_FACTORY, dds, entity_factory);
  FUNC_BY_REF (RTI_TYPECODE, rti_typecode, octetseq);
  /* CYCLONE_IGNORELOCAL is not visible on the wire */
#undef FUNC_BY_REF
#undef FUNC_BY_VAL
#undef SIMPLE
}

static void add_locators (struct nn_xmsg *m, uint64_t present, uint64_t flag, const nn_locators_t *ls, nn_parameterid_t pid)
{
  const struct nn_locators_one *l;
  if (present & flag)
  {
    for (l = ls->first; l != NULL; l = l->next)
    {
      char *tmp = nn_xmsg_addpar (m, pid, sizeof (nn_locator_t));
      memcpy (tmp, &l->loc, sizeof (nn_locator_t));
    }
  }
}

void nn_plist_addtomsg (struct nn_xmsg *m, const nn_plist_t *ps, uint64_t pwanted, uint64_t qwanted)
{
  /* Returns new nn_xmsg pointer (currently, reallocs may happen), or NULL
     on out-of-memory. (In which case the original nn_xmsg is freed, cos
     that is then required anyway */
  uint64_t w = ps->present & pwanted;
  char *tmp;
#define SIMPLE_TYPE(name_, field_, type_) \
  do { \
    if (w & PP_##name_) { \
      tmp = nn_xmsg_addpar (m, PID_##name_, sizeof (ps->field_)); \
      *((type_ *) tmp) = ps->field_; \
    } \
  } while (0)
#define FUNC_BY_VAL(name_, field_, func_) \
  do { \
    if (w & PP_##name_) { \
      nn_xmsg_addpar_##func_ (m, PID_##name_, ps->field_); \
    } \
  } while (0)
#define FUNC_BY_REF(name_, field_, func_) \
  do { \
    if (w & PP_##name_) { \
      nn_xmsg_addpar_##func_ (m, PID_##name_, &ps->field_); \
    } \
  } while (0)

  nn_xqos_addtomsg (m, &ps->qos, qwanted);
  SIMPLE_TYPE (PROTOCOL_VERSION, protocol_version, nn_protocol_version_t);
  SIMPLE_TYPE (VENDORID, vendorid, nn_vendorid_t);

  add_locators (m, ps->present, PP_UNICAST_LOCATOR, &ps->unicast_locators, PID_UNICAST_LOCATOR);
  add_locators (m, ps->present, PP_MULTICAST_LOCATOR, &ps->multicast_locators, PID_MULTICAST_LOCATOR);
  add_locators (m, ps->present, PP_DEFAULT_UNICAST_LOCATOR, &ps->default_unicast_locators, PID_DEFAULT_UNICAST_LOCATOR);
  add_locators (m, ps->present, PP_DEFAULT_MULTICAST_LOCATOR, &ps->default_multicast_locators, PID_DEFAULT_MULTICAST_LOCATOR);
  add_locators (m, ps->present, PP_METATRAFFIC_UNICAST_LOCATOR, &ps->metatraffic_unicast_locators, PID_METATRAFFIC_UNICAST_LOCATOR);
  add_locators (m, ps->present, PP_METATRAFFIC_MULTICAST_LOCATOR, &ps->metatraffic_multicast_locators, PID_METATRAFFIC_MULTICAST_LOCATOR);

  SIMPLE_TYPE (EXPECTS_INLINE_QOS, expects_inline_qos, unsigned char);
  FUNC_BY_VAL (PARTICIPANT_LEASE_DURATION, participant_lease_duration, duration);
  FUNC_BY_REF (PARTICIPANT_GUID, participant_guid, guid);
  SIMPLE_TYPE (BUILTIN_ENDPOINT_SET, builtin_endpoint_set, unsigned);
  SIMPLE_TYPE (KEYHASH, keyhash, nn_keyhash_t);
  if (w & PP_STATUSINFO)
    nn_xmsg_addpar_statusinfo (m, ps->statusinfo);
  SIMPLE_TYPE (COHERENT_SET, coherent_set_seqno, nn_sequence_number_t);
  if (! NN_PEDANTIC_P)
    FUNC_BY_REF (ENDPOINT_GUID, endpoint_guid, guid);
  else
  {
    if (w & PP_ENDPOINT_GUID)
    {
      nn_xmsg_addpar_guid (m, PID_PRISMTECH_ENDPOINT_GUID, &ps->endpoint_guid);
    }
  }
  FUNC_BY_REF (GROUP_GUID, group_guid, guid);
  SIMPLE_TYPE (PRISMTECH_BUILTIN_ENDPOINT_SET, prismtech_builtin_endpoint_set, unsigned);
  FUNC_BY_REF (PRISMTECH_PARTICIPANT_VERSION_INFO, prismtech_participant_version_info, parvinfo);
  FUNC_BY_VAL (ENTITY_NAME, entity_name, string);
  FUNC_BY_VAL (PRISMTECH_NODE_NAME, node_name, string);
  FUNC_BY_VAL (PRISMTECH_EXEC_NAME, exec_name, string);
  SIMPLE_TYPE (PRISMTECH_PROCESS_ID, process_id, unsigned);
  SIMPLE_TYPE (PRISMTECH_SERVICE_TYPE, service_type, unsigned);
  FUNC_BY_VAL (PRISMTECH_TYPE_DESCRIPTION, type_description, string);
  FUNC_BY_REF (PRISMTECH_EOTINFO, eotinfo, eotinfo);
#ifdef DDSI_INCLUDE_SSM
  SIMPLE_TYPE (READER_FAVOURS_SSM, reader_favours_ssm, nn_reader_favours_ssm_t);
#endif
#undef FUNC_BY_REF
#undef FUNC_BY_VAL
#undef SIMPLE
}

/*************************/

static uint32_t isprint_runlen (uint32_t n, const unsigned char *xs)
{
  uint32_t m;
  for (m = 0; m < n && xs[m] != '"' && isprint (xs[m]); m++)
    ;
  return m;
}


static void log_octetseq (uint32_t cat, uint32_t n, const unsigned char *xs)
{
  uint32_t i = 0;
  while (i < n)
  {
    uint32_t m = isprint_runlen(n - i, xs);
    if (m >= 4)
    {
      DDS_LOG(cat, "%s\"%*.*s\"", i == 0 ? "" : ",", m, m, xs);
      xs += m;
      i += m;
    }
    else
    {
      if (m == 0)
        m = 1;
      while (m--)
      {
        DDS_LOG(cat, "%s%u", i == 0 ? "" : ",", *xs++);
        i++;
      }
    }
  }
}

void nn_log_xqos (uint32_t cat, const dds_qos_t *xqos)
{
  uint64_t p = xqos->present;
  const char *prefix = "";
#define LOGB0(fmt_) DDS_LOG(cat, "%s" fmt_, prefix)
#define LOGB1(fmt_, arg0_) DDS_LOG(cat, "%s" fmt_, prefix, arg0_)
#define LOGB2(fmt_, arg0_, arg1_) DDS_LOG(cat, "%s" fmt_, prefix, arg0_, arg1_)
#define LOGB3(fmt_, arg0_, arg1_, arg2_) DDS_LOG(cat, "%s" fmt_, prefix, arg0_, arg1_, arg2_)
#define LOGB4(fmt_, arg0_, arg1_, arg2_, arg3_) DDS_LOG(cat, "%s" fmt_, prefix, arg0_, arg1_, arg2_, arg3_)
#define LOGB5(fmt_, arg0_, arg1_, arg2_, arg3_, arg4_) DDS_LOG(cat, "%s" fmt_, prefix, arg0_, arg1_, arg2_, arg3_, arg4_)
#define DO(name_, body_) do { if (p & QP_##name_) { { body_ } prefix = ","; } } while (0)

#define FMT_DUR "%"PRId64".%09"PRId32
#define PRINTARG_DUR(d) ((int64_t) ((d) / 1000000000)), ((int32_t) ((d) % 1000000000))

  DO (TOPIC_NAME, { LOGB1 ("topic=%s", xqos->topic_name); });
  DO (TYPE_NAME, { LOGB1 ("type=%s", xqos->type_name); });
  DO (PRESENTATION, { LOGB3 ("presentation=%d:%u:%u", xqos->presentation.access_scope, xqos->presentation.coherent_access, xqos->presentation.ordered_access); });
  DO (PARTITION, {
      LOGB0 ("partition={");
      for (uint32_t i = 0; i < xqos->partition.n; i++) {
        DDS_LOG(cat, "%s%s", (i == 0) ? "" : ",", xqos->partition.strs[i]);
      }
      DDS_LOG(cat, "}");
    });
  DO (GROUP_DATA, {
    LOGB1 ("group_data=%"PRIu32"<", xqos->group_data.length);
    log_octetseq (cat, xqos->group_data.length, xqos->group_data.value);
    DDS_LOG(cat, ">");
  });
  DO (TOPIC_DATA, {
    LOGB1 ("topic_data=%"PRIu32"<", xqos->topic_data.length);
    log_octetseq (cat, xqos->topic_data.length, xqos->topic_data.value);
    DDS_LOG(cat, ">");
  });
  DO (DURABILITY, { LOGB1 ("durability=%d", xqos->durability.kind); });
  DO (DURABILITY_SERVICE, {
      LOGB0 ("durability_service=");
      DDS_LOG(cat, FMT_DUR, PRINTARG_DUR (xqos->durability_service.service_cleanup_delay));
      DDS_LOG(cat, ":{%u:%"PRId32"}", xqos->durability_service.history.kind, xqos->durability_service.history.depth);
      DDS_LOG(cat, ":{%"PRId32":%"PRId32":%"PRId32"}", xqos->durability_service.resource_limits.max_samples, xqos->durability_service.resource_limits.max_instances, xqos->durability_service.resource_limits.max_samples_per_instance);
    });
  DO (DEADLINE, { LOGB1 ("deadline="FMT_DUR, PRINTARG_DUR (xqos->deadline.deadline)); });
  DO (LATENCY_BUDGET, { LOGB1 ("latency_budget="FMT_DUR, PRINTARG_DUR (xqos->latency_budget.duration)); });
  DO (LIVELINESS, { LOGB2 ("liveliness=%d:"FMT_DUR, xqos->liveliness.kind, PRINTARG_DUR (xqos->liveliness.lease_duration)); });
  DO (RELIABILITY, { LOGB2 ("reliability=%d:"FMT_DUR, xqos->reliability.kind, PRINTARG_DUR (xqos->reliability.max_blocking_time)); });
  DO (DESTINATION_ORDER, { LOGB1 ("destination_order=%d", xqos->destination_order.kind); });
  DO (HISTORY, { LOGB2 ("history=%d:%"PRId32, xqos->history.kind, xqos->history.depth); });
  DO (RESOURCE_LIMITS, { LOGB3 ("resource_limits=%"PRId32":%"PRId32":%"PRId32, xqos->resource_limits.max_samples, xqos->resource_limits.max_instances, xqos->resource_limits.max_samples_per_instance); });
  DO (TRANSPORT_PRIORITY, { LOGB1 ("transport_priority=%"PRId32, xqos->transport_priority.value); });
  DO (LIFESPAN, { LOGB1 ("lifespan="FMT_DUR, PRINTARG_DUR (xqos->lifespan.duration)); });
  DO (USER_DATA, {
    LOGB1 ("user_data=%"PRIu32"<", xqos->user_data.length);
    log_octetseq (cat, xqos->user_data.length, xqos->user_data.value);
    DDS_LOG(cat, ">");
  });
  DO (OWNERSHIP, { LOGB1 ("ownership=%d", xqos->ownership.kind); });
  DO (OWNERSHIP_STRENGTH, { LOGB1 ("ownership_strength=%"PRId32, xqos->ownership_strength.value); });
  DO (TIME_BASED_FILTER, { LOGB1 ("time_based_filter="FMT_DUR, PRINTARG_DUR (xqos->time_based_filter.minimum_separation)); });
  DO (PRISMTECH_READER_DATA_LIFECYCLE, { LOGB2 ("reader_data_lifecycle="FMT_DUR":"FMT_DUR, PRINTARG_DUR (xqos->reader_data_lifecycle.autopurge_nowriter_samples_delay), PRINTARG_DUR (xqos->reader_data_lifecycle.autopurge_disposed_samples_delay)); });
  DO (PRISMTECH_WRITER_DATA_LIFECYCLE, {
    LOGB1 ("writer_data_lifecycle={%u}", xqos->writer_data_lifecycle.autodispose_unregistered_instances); });
  DO (PRISMTECH_READER_LIFESPAN, { LOGB2 ("reader_lifespan={%u,"FMT_DUR"}", xqos->reader_lifespan.use_lifespan, PRINTARG_DUR (xqos->reader_lifespan.duration)); });
  DO (PRISMTECH_SUBSCRIPTION_KEYS, {
    LOGB1 ("subscription_keys={%u,{", xqos->subscription_keys.use_key_list);
    for (uint32_t i = 0; i < xqos->subscription_keys.key_list.n; i++) {
      DDS_LOG(cat, "%s%s", (i == 0) ? "" : ",", xqos->subscription_keys.key_list.strs[i]);
    }
    DDS_LOG(cat, "}}");
  });
  DO (PRISMTECH_ENTITY_FACTORY, { LOGB1 ("entity_factory=%u", xqos->entity_factory.autoenable_created_entities); });
  DO (RTI_TYPECODE, {
    LOGB1 ("rti_typecode=%"PRIu32"<", xqos->rti_typecode.length);
    log_octetseq (cat, xqos->rti_typecode.length, xqos->rti_typecode.value);
    DDS_LOG(cat, ">");
  });
  DO (CYCLONE_IGNORELOCAL, { LOGB1 ("ignorelocal=%u", xqos->ignorelocal.value); });

#undef PRINTARG_DUR
#undef FMT_DUR
#undef DO
#undef LOGB5
#undef LOGB4
#undef LOGB3
#undef LOGB2
#undef LOGB1
#undef LOGB0
}

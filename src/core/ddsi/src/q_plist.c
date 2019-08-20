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
#include "dds/ddsrt/string.h"
#include "dds/ddsrt/static_assert.h"

#include "dds/ddsi/q_log.h"

#include "dds/ddsi/q_bswap.h"
#include "dds/ddsi/q_unused.h"
#include "dds/ddsi/q_plist.h"
#include "dds/ddsi/q_time.h"
#include "dds/ddsi/q_xmsg.h"
#include "dds/ddsi/ddsi_vendor.h"
#include "dds/ddsi/ddsi_udp.h" /* nn_mc4gen_address_t */

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
  ddsi_tran_factory_t factory;
};

#define PDF_QOS        1 /* part of dds_qos_t */
#define PDF_FUNCTION   2 /* use special functions */
#define PDF_ALLOWMULTI 4 /* allow multiple copies -- do not use with Z or memory will leak */

struct flagset {
  uint64_t *present;
  uint64_t *aliased;
  uint64_t wanted;
};

/* Instructions for the generic serializer (&c) that handles most parameters.
   The "packed" attribute means single-byte instructions on GCC and Clang. */
enum pserop {
  XSTOP,
  XO, /* octet sequence */
  XS, /* string */
  XZ, /* string sequence */
  XE1, XE2, XE3, /* enum 0..1, 0..2, 0..3 */
  Xl, /* length, int32_t, -1 or >= 1 */
  Xi, Xix2, Xix3, Xix4, /* int32_t, 1 .. 4 in a row */
  Xu, Xux2, Xux3, Xux4, Xux5, /* uint32_t, 1 .. 5 in a row */
  XD, XDx2, /* duration, 1 .. 2 in a row */
  Xo, Xox2, /* octet, 1 .. 2 in a row */
  Xb, Xbx2, /* boolean, 1 .. 2 in a row */
  XbCOND, /* boolean: compare to ignore remainder if false (for use_... flags) */
  XG, /* GUID */
  XK /* keyhash */
} ddsrt_attribute_packed;

struct piddesc {
  nn_parameterid_t pid;  /* parameter id or PID_PAD if strictly local */
  uint16_t flags;        /* see PDF_xxx flags */
  uint64_t present_flag; /* flag in plist.present / plist.qos.present */
  const char *name;      /* name for reporting invalid input */
  size_t plist_offset;   /* offset from start of nn_plist_t */
  size_t size;           /* in-memory size for copying */
  union {
    /* descriptor for generic code: 4 is enough for the current set of
       parameters, compiler will warn if one ever tries to use more than
       will fit; on non-GCC/Clang and 32-bits machines */
    const enum pserop desc[4];
    struct {
      dds_return_t (*deser) (void * __restrict dst, size_t * __restrict dstoff, struct flagset *flagset, uint64_t flag, const struct dd * __restrict dd, size_t * __restrict srcoff);
      dds_return_t (*ser) (struct nn_xmsg *xmsg, nn_parameterid_t pid, const void *src, size_t srcoff);
      dds_return_t (*unalias) (void * __restrict dst, size_t * __restrict dstoff);
      dds_return_t (*fini) (void * __restrict dst, size_t * __restrict dstoff, struct flagset *flagset, uint64_t flag);
      dds_return_t (*valid) (const void *src, size_t srcoff);
      bool (*equal) (const void *srcx, const void *srcy, size_t srcoff);
    } f;
  } op;
  dds_return_t (*deser_validate_xform) (void * __restrict dst, const struct dd * __restrict dd);
};

static void log_octetseq (uint32_t cat, const struct ddsrt_log_cfg *logcfg, uint32_t n, const unsigned char *xs);
static dds_return_t validate_history_qospolicy (const dds_history_qospolicy_t *q);
static dds_return_t validate_resource_limits_qospolicy (const dds_resource_limits_qospolicy_t *q);
static dds_return_t validate_history_and_resource_limits (const dds_history_qospolicy_t *qh, const dds_resource_limits_qospolicy_t *qr);
static dds_return_t validate_external_duration (const ddsi_duration_t *d);
static dds_return_t validate_durability_service_qospolicy_acceptzero (const dds_durability_service_qospolicy_t *q, bool acceptzero);
static dds_return_t do_locator (nn_locators_t *ls, uint64_t *present, uint64_t wanted, uint64_t fl, const struct dd *dd, const struct ddsi_tran_factory *factory);
static dds_return_t final_validation_qos (const dds_qos_t *dest, nn_protocol_version_t protocol_version, nn_vendorid_t vendorid, bool *dursvc_accepted_allzero, bool strict);
static int partitions_equal (const dds_partition_qospolicy_t *a, const dds_partition_qospolicy_t *b);
static dds_return_t nn_xqos_valid_strictness (const struct ddsrt_log_cfg *logcfg, const dds_qos_t *xqos, bool strict);

static size_t align4size (size_t x)
{
  return (x + 3) & ~(size_t)3;
}

static void *deser_generic_dst (void * __restrict dst, size_t *dstoff, size_t align)
{
  *dstoff = (*dstoff + align - 1) & ~(align - 1);
  return (char *) dst + *dstoff;
}

static const void *deser_generic_src (const void * __restrict src, size_t *srcoff, size_t align)
{
  *srcoff = (*srcoff + align - 1) & ~(align - 1);
  return (const char *) src + *srcoff;
}

static void *ser_generic_align4 (char * __restrict p, size_t * __restrict off)
{
  const size_t off1 = align4size (*off);
  size_t pad = off1 - *off;
  char *dst = p + *off;
  *off = off1;
  while (pad--)
    *dst++ = 0;
  return dst;
}

static dds_return_t deser_uint32 (uint32_t *dst, const struct dd * __restrict dd, size_t * __restrict off)
{
  size_t off1 = (*off + 3) & ~(size_t)3;
  uint32_t tmp;
  if (off1 + 4 > dd->bufsz)
    return DDS_RETCODE_BAD_PARAMETER;
  tmp = *((uint32_t *) (dd->buf + off1));
  if (dd->bswap)
    tmp = bswap4u (tmp);
  *dst = tmp;
  *off = off1 + 4;
  return 0;
}

#define alignof(type_) offsetof (struct { char c; type_ d; }, d)

static dds_return_t deser_reliability (void * __restrict dst, size_t * __restrict dstoff, struct flagset *flagset, uint64_t flag, const struct dd * __restrict dd, size_t * __restrict srcoff)
{
  DDSRT_STATIC_ASSERT (DDS_EXTERNAL_RELIABILITY_BEST_EFFORT == 1 && DDS_EXTERNAL_RELIABILITY_RELIABLE == 2 &&
                       DDS_RELIABILITY_BEST_EFFORT == 0 && DDS_RELIABILITY_RELIABLE == 1);
  dds_reliability_qospolicy_t * const x = deser_generic_dst (dst, dstoff, alignof (dds_reliability_qospolicy_t));
  uint32_t kind, mbtsec, mbtfrac;
  ddsi_duration_t mbt;
  if (deser_uint32 (&kind, dd, srcoff) < 0 || deser_uint32 (&mbtsec, dd, srcoff) < 0 || deser_uint32 (&mbtfrac, dd, srcoff) < 0)
    return DDS_RETCODE_BAD_PARAMETER;
  if (kind < 1 || kind > 2)
    return DDS_RETCODE_BAD_PARAMETER;
  mbt.seconds = (int32_t) mbtsec;
  mbt.fraction = mbtfrac;
  if (validate_external_duration (&mbt) < 0)
    return DDS_RETCODE_BAD_PARAMETER;
  x->kind = (enum dds_reliability_kind) (kind - 1);
  x->max_blocking_time = nn_from_ddsi_duration (mbt);
  *dstoff += sizeof (*x);
  *flagset->present |= flag;
  return 0;
}

static dds_return_t ser_reliability (struct nn_xmsg *xmsg, nn_parameterid_t pid, const void *src, size_t srcoff)
{
  DDSRT_STATIC_ASSERT (DDS_EXTERNAL_RELIABILITY_BEST_EFFORT == 1 && DDS_EXTERNAL_RELIABILITY_RELIABLE == 2 &&
                       DDS_RELIABILITY_BEST_EFFORT == 0 && DDS_RELIABILITY_RELIABLE == 1);
  dds_reliability_qospolicy_t const * const x = deser_generic_src (src, &srcoff, alignof (dds_reliability_qospolicy_t));
  ddsi_duration_t mbt = nn_to_ddsi_duration (x->max_blocking_time);
  uint32_t * const p = nn_xmsg_addpar (xmsg, pid, 3 * sizeof (uint32_t));
  p[0] = 1 + (uint32_t) x->kind;
  p[1] = (uint32_t) mbt.seconds;
  p[2] = mbt.fraction;
  return 0;
}

static dds_return_t valid_reliability (const void *src, size_t srcoff)
{
  dds_reliability_qospolicy_t const * const x = deser_generic_src (src, &srcoff, alignof (dds_reliability_qospolicy_t));
  if ((x->kind == DDS_RELIABILITY_BEST_EFFORT || x->kind == DDS_RELIABILITY_RELIABLE) && x->max_blocking_time >= 0)
    return 0;
  else
    return DDS_RETCODE_BAD_PARAMETER;
}

static bool equal_reliability (const void *srcx, const void *srcy, size_t srcoff)
{
  dds_reliability_qospolicy_t const * const x = deser_generic_src (srcx, &srcoff, alignof (dds_reliability_qospolicy_t));
  dds_reliability_qospolicy_t const * const y = deser_generic_src (srcy, &srcoff, alignof (dds_reliability_qospolicy_t));
  return x->kind == y->kind && x->max_blocking_time == y->max_blocking_time;
}

static dds_return_t deser_statusinfo (void * __restrict dst, size_t * __restrict dstoff, struct flagset *flagset, uint64_t flag, const struct dd * __restrict dd, size_t * __restrict srcoff)
{
  uint32_t * const x = deser_generic_dst (dst, dstoff, alignof (dds_reliability_qospolicy_t));
  size_t srcoff1 = (*srcoff + 3) & ~(size_t)3;
  if (srcoff1 + 4 > dd->bufsz)
    return DDS_RETCODE_BAD_PARAMETER;
  /* status info is always in BE format (it is an array of 4 octets according to the spec) --
     fortunately we have 4 byte alignment anyway -- and can have bits set we don't grok
     (which we discard) */
  *x = fromBE4u (*((uint32_t *) (dd->buf + srcoff1))) & NN_STATUSINFO_STANDARDIZED;
  *dstoff += sizeof (*x);
  *srcoff = srcoff1 + 4;
  *flagset->present |= flag;
  return 0;
}

static dds_return_t ser_statusinfo (struct nn_xmsg *xmsg, nn_parameterid_t pid, const void *src, size_t srcoff)
{
  uint32_t const * const x = deser_generic_src (src, &srcoff, alignof (uint32_t));
  uint32_t * const p = nn_xmsg_addpar (xmsg, pid, sizeof (uint32_t));
  *p = toBE4u (*x);
  return 0;
}

static dds_return_t deser_locator (void * __restrict dst, size_t * __restrict dstoff, struct flagset *flagset, uint64_t flag, const struct dd * __restrict dd, size_t * __restrict srcoff)
{
  nn_locators_t * const x = deser_generic_dst (dst, dstoff, alignof (nn_locators_t));
  /* FIXME: don't want to modify do_locator just yet, and don't want to require that a
     locator is the only thing in the descriptor string (even though it actually always is),
     so do alignment explicitly, fake a temporary input buffer and advance the source buffer */
  *srcoff = (*srcoff + 3) & ~(size_t)3;
  if (*srcoff > dd->bufsz || dd->bufsz - *srcoff < 24)
    return DDS_RETCODE_BAD_PARAMETER;
  struct dd tmpdd = *dd;
  tmpdd.buf += *srcoff;
  tmpdd.bufsz -= *srcoff;
  if (do_locator (x, flagset->present, flagset->wanted, flag, &tmpdd, dd->factory) < 0)
    return DDS_RETCODE_BAD_PARAMETER;
  *srcoff += 24;
  *dstoff += sizeof (*x);
  *flagset->present |= flag;
  return 0;
}

static dds_return_t ser_locator (struct nn_xmsg *xmsg, nn_parameterid_t pid, const void *src, size_t srcoff)
{
  nn_locators_t const * const x = deser_generic_src (src, &srcoff, alignof (nn_locators_t));
  for (const struct nn_locators_one *l = x->first; l != NULL; l = l->next)
  {
    char * const p = nn_xmsg_addpar (xmsg, pid, sizeof (nn_locator_t));
    memcpy (p, &l->loc, sizeof (nn_locator_t));
  }
  return 0;
}

static dds_return_t unalias_locator (void * __restrict dst, size_t * __restrict dstoff)
{
  nn_locators_t * const x = deser_generic_dst (dst, dstoff, alignof (nn_locators_t));
  nn_locators_t newlocs = { .n = x->n, .first = NULL, .last = NULL };
  struct nn_locators_one **pnext = &newlocs.first;
  for (const struct nn_locators_one *lold = x->first; lold != NULL; lold = lold->next)
  {
    struct nn_locators_one *n = ddsrt_memdup (lold, sizeof (*n));
    *pnext = n;
    pnext = &n->next;
  }
  newlocs.last = *pnext;
  *pnext = NULL;
  *x = newlocs;
  *dstoff += sizeof (*x);
  return 0;
}

static dds_return_t fini_locator (void * __restrict dst, size_t * __restrict dstoff, struct flagset *flagset, uint64_t flag)
{
  nn_locators_t * const x = deser_generic_dst (dst, dstoff, alignof (nn_locators_t));
  if (!(*flagset->aliased &flag))
  {
    while (x->first)
    {
      struct nn_locators_one *l = x->first;
      x->first = l->next;
      ddsrt_free (l);
    }
  }
  return 0;
}

static void fini_generic_partial (void * __restrict dst, size_t * __restrict dstoff, const enum pserop *desc, const enum pserop * const desc_end, bool aliased)
{
#define COMPLEX(basecase_, type_, cleanup_unaliased_, cleanup_always_) do { \
    type_ *x = deser_generic_dst (dst, dstoff, alignof (type_));            \
    const uint32_t cnt = 1 + (uint32_t) (*desc - (basecase_));              \
    for (uint32_t xi = 0; xi < cnt; xi++, x++) {                            \
      if (!aliased) do { cleanup_unaliased_; } while (0);                   \
      do { cleanup_always_; } while (0);                                    \
    }                                                                       \
    *dstoff += cnt * sizeof (*x);                                           \
  } while (0)
#define SIMPLE(basecase_, type_) COMPLEX (basecase_, type_, (void) 0, (void) 0)
  while (desc != desc_end)
  {
    switch (*desc)
    {
      case XSTOP: return;
      case XO: COMPLEX (XO, ddsi_octetseq_t, ddsrt_free (x->value), (void) 0); break;
      case XS: COMPLEX (XS, char *, ddsrt_free (*x), (void) 0); break;
      case XZ: COMPLEX (XZ, ddsi_stringseq_t, { for (uint32_t i = 0; i < x->n; i++) ddsrt_free (x->strs[i]); }, ddsrt_free (x->strs)); break;
      case XE1: case XE2: case XE3: COMPLEX (*desc, unsigned, (void) 0, (void) 0); break;
      case Xi: case Xix2: case Xix3: case Xix4: SIMPLE (Xi, int32_t); break;
      case Xu: case Xux2: case Xux3: case Xux4: case Xux5: SIMPLE (Xu, uint32_t); break;
      case Xl: SIMPLE (Xl, int32_t); break;
      case XD: case XDx2: SIMPLE (XD, dds_duration_t); break;
      case Xo: case Xox2: SIMPLE (Xo, unsigned char); break;
      case Xb: case Xbx2: SIMPLE (Xb, unsigned char); break;
      case XbCOND: SIMPLE (XbCOND, unsigned char); break;
      case XG: SIMPLE (XG, nn_guid_t); break;
      case XK: SIMPLE (XK, nn_keyhash_t); break;
    }
    desc++;
  }
#undef SIMPLE
#undef COMPLEX
}

static dds_return_t deser_generic (void * __restrict dst, size_t * __restrict dstoff, struct flagset *flagset, uint64_t flag, const struct dd * __restrict dd, size_t * __restrict srcoff, const enum pserop * __restrict desc)
{
  enum pserop const * const desc_in = desc;
  size_t dstoff_in = *dstoff;
  /* very large buffers run a risk with alignment calculations; such buffers basically
     do not occur for discovery data, so checking makes sense */
  if (dd->bufsz >= SIZE_MAX - 8)
    return DDS_RETCODE_BAD_PARAMETER;
  while (true)
  {
    assert (*srcoff <= dd->bufsz);
    switch (*desc)
    {
      case XSTOP:
        *flagset->present |= flag;
        return 0;
      case XO: { /* octet sequence */
        ddsi_octetseq_t * const x = deser_generic_dst (dst, dstoff, alignof (ddsi_octetseq_t));
        if (deser_uint32 (&x->length, dd, srcoff) < 0 || dd->bufsz - *srcoff < x->length)
          goto fail;
        x->value = x->length ? (unsigned char *) (dd->buf + *srcoff) : NULL;
        *srcoff += x->length;
        *dstoff += sizeof (*x);
        *flagset->aliased |= flag;
        break;
      }
      case XS: { /* string: alias as-if octet sequence, do additional checks and store as string */
        char ** const x = deser_generic_dst (dst, dstoff, alignof (char *));
        ddsi_octetseq_t tmp;
        size_t tmpoff = 0;
        if (deser_generic (&tmp, &tmpoff, flagset, flag, dd, srcoff, (enum pserop []) { XO, XSTOP }) < 0)
          goto fail;
        if (tmp.length < 1 || tmp.value[tmp.length - 1] != 0)
          goto fail;
        *x = (char *) tmp.value;
        *dstoff += sizeof (*x);
        break;
      }
      case XZ: { /* string sequence: repeatedly read a string */
        ddsi_stringseq_t * const x = deser_generic_dst (dst, dstoff, alignof (ddsi_stringseq_t));
        /* sequence of string: length <data> length <data> ..., where each length is aligned
           to a multiple of 4 bytes and the lengths are all at least 1, therefore all but the
           last entry need 8 bytes and the final one at least 5; checking this protects us
           against allocating large amount of memory */
        if (deser_uint32 (&x->n, dd, srcoff) < 0 || x->n > (dd->bufsz - *srcoff + 7) / 8)
          goto fail;
        x->strs = x->n ? ddsrt_malloc (x->n * sizeof (*x->strs)) : NULL;
        size_t tmpoff = 0;
        for (uint32_t i = 0; i < x->n; i++)
          if (deser_generic (x->strs, &tmpoff, flagset, flag, dd, srcoff, (enum pserop []) { XS, XSTOP }) < 0)
            goto fail;
        *dstoff += sizeof (*x);
        break;
      }
      case XE1: case XE2: case XE3: { /* enum with max allowed value */
        unsigned * const x = deser_generic_dst (dst, dstoff, alignof (int));
        const uint32_t maxval = 1 + (uint32_t) (*desc - XE1);
        uint32_t tmp;
        if (deser_uint32 (&tmp, dd, srcoff) < 0 || tmp > maxval)
          goto fail;
        *x = (unsigned) tmp;
        *dstoff += sizeof (*x);
        break;
      }
      case Xi: case Xix2: case Xix3: case Xix4: { /* int32_t(s) */
        uint32_t * const x = deser_generic_dst (dst, dstoff, alignof (uint32_t));
        const uint32_t cnt = 1 + (uint32_t) (*desc - Xi);
        for (uint32_t i = 0; i < cnt; i++)
          if (deser_uint32 (&x[i], dd, srcoff) < 0)
            goto fail;
        *dstoff += cnt * sizeof (*x);
        break;
      }
      case Xu: case Xux2: case Xux3: case Xux4: case Xux5: { /* uint32_t(s): treated the same */
        uint32_t * const x = deser_generic_dst (dst, dstoff, alignof (uint32_t));
        const uint32_t cnt = 1 + (uint32_t) (*desc - Xu);
        for (uint32_t i = 0; i < cnt; i++)
          if (deser_uint32 (&x[i], dd, srcoff) < 0)
            goto fail;
        *dstoff += cnt * sizeof (*x);
        break;
      }
      case Xl: { /* length(s): int32_t, -1 or >= 1 */
        int32_t * const x = deser_generic_dst (dst, dstoff, alignof (uint32_t));
        const uint32_t cnt = 1 + (uint32_t) (*desc - Xl);
        for (uint32_t i = 0; i < cnt; i++)
          if (deser_uint32 ((uint32_t *) &x[i], dd, srcoff) < 0 || (x[i] < 1 && x[i] != DDS_LENGTH_UNLIMITED))
            goto fail;
        *dstoff += cnt * sizeof (*x);
        break;
      }
      case XD: case XDx2: { /* duration(s): int64_t <=> int32_t.uint32_t (seconds.fraction) */
        dds_duration_t * const x = deser_generic_dst (dst, dstoff, alignof (dds_duration_t));
        const uint32_t cnt = 1 + (uint32_t) (*desc - XD);
        for (uint32_t i = 0; i < cnt; i++)
        {
          ddsi_duration_t tmp;
          if (deser_uint32 ((uint32_t *) &tmp.seconds, dd, srcoff) < 0 || deser_uint32 (&tmp.fraction, dd, srcoff) < 0)
            goto fail;
          if (validate_external_duration (&tmp))
            goto fail;
          x[i] = nn_from_ddsi_duration (tmp);
        }
        *dstoff += cnt * sizeof (*x);
        break;
      }
      case Xo: case Xox2: { /* octet(s) */
        unsigned char * const x = deser_generic_dst (dst, dstoff, alignof (unsigned char));
        const uint32_t cnt = 1 + (uint32_t) (*desc - Xo);
        if (dd->bufsz - *srcoff < cnt)
          goto fail;
        memcpy (x, dd->buf + *srcoff, cnt);
        *srcoff += cnt;
        *dstoff += cnt * sizeof (*x);
        break;
      }
      case Xb: case Xbx2: case XbCOND: { /* boolean(s) */
        unsigned char * const x = deser_generic_dst (dst, dstoff, alignof (unsigned char));
        const uint32_t cnt = (*desc == Xbx2) ? 2 : 1; /* <<<< beware! */
        if (dd->bufsz - *srcoff < cnt)
          goto fail;
        memcpy (x, dd->buf + *srcoff, cnt);
        for (uint32_t i = 0; i < cnt; i++)
          if (x[i] > 1)
            goto fail;
        *srcoff += cnt;
        *dstoff += cnt * sizeof (*x);
        break;
      }
      case XG: { /* GUID */
        nn_guid_t * const x = deser_generic_dst (dst, dstoff, alignof (nn_guid_t));
        if (dd->bufsz - *srcoff < sizeof (*x))
          goto fail;
        memcpy (x, dd->buf + *srcoff, sizeof (*x));
        *x = nn_ntoh_guid (*x);
        *srcoff += sizeof (*x);
        *dstoff += sizeof (*x);
        break;
      }
      case XK: { /* keyhash */
        nn_keyhash_t * const x = deser_generic_dst (dst, dstoff, alignof (nn_keyhash_t));
        if (dd->bufsz - *srcoff < sizeof (*x))
          goto fail;
        memcpy (x, dd->buf + *srcoff, sizeof (*x));
        *srcoff += sizeof (*x);
        *dstoff += sizeof (*x);
        break;
      }
    }
    desc++;
  }

fail:
  fini_generic_partial (dst, &dstoff_in, desc_in, desc, *flagset->aliased & flag);
  *flagset->present &= ~flag;
  *flagset->aliased &= ~flag;
  return DDS_RETCODE_BAD_PARAMETER;
}

static size_t ser_generic_size (const void *src, size_t srcoff, const enum pserop * __restrict desc)
{
  size_t dstoff = 0;
#define COMPLEX(basecase_, type_, dstoff_update_) do {                  \
    type_ const *x = deser_generic_src (src, &srcoff, alignof (type_)); \
    const uint32_t cnt = 1 + (uint32_t) (*desc - (basecase_));          \
    for (uint32_t xi = 0; xi < cnt; xi++, x++) { dstoff_update_; }      \
    srcoff += cnt * sizeof (*x);                                        \
  } while (0)
#define SIMPLE1(basecase_, type_) COMPLEX (basecase_, type_, dstoff = dstoff + sizeof (*x))
#define SIMPLE4(basecase_, type_) COMPLEX (basecase_, type_, dstoff = align4size (dstoff) + sizeof (*x))
  while (true)
  {
    switch (*desc)
    {
      case XSTOP: return dstoff;
      case XO: COMPLEX (XO, ddsi_octetseq_t, dstoff = align4size (dstoff) + 4 + x->length); break;
      case XS: COMPLEX (XS, const char *, dstoff = align4size (dstoff) + 4 + strlen (*x) + 1); break;
      case XZ: COMPLEX (XZ, ddsi_stringseq_t, {
        dstoff = align4size (dstoff) + 4;
        for (uint32_t i = 0; i < x->n; i++)
          dstoff = align4size (dstoff) + 4 + strlen (x->strs[i]) + 1;
      }); break;
      case XE1: case XE2: case XE3: COMPLEX (*desc, unsigned, dstoff = align4size (dstoff) + 4); break;
      case Xi: case Xix2: case Xix3: case Xix4: SIMPLE4 (Xi, int32_t); break;
      case Xu: case Xux2: case Xux3: case Xux4: case Xux5: SIMPLE4 (Xu, uint32_t); break;
      case Xl: SIMPLE4 (Xl, int32_t); break;
      case XD: case XDx2: SIMPLE4 (XD, dds_duration_t); break;
      case Xo: case Xox2: SIMPLE1 (Xo, unsigned char); break;
      case Xb: case Xbx2: SIMPLE1 (Xb, unsigned char); break;
      case XbCOND: SIMPLE1 (XbCOND, unsigned char); break;
      case XG: SIMPLE1 (XG, nn_guid_t); break;
      case XK: SIMPLE1 (XK, nn_keyhash_t); break;
    }
    desc++;
  }
#undef SIMPLE
#undef COMPLEX
}

static dds_return_t ser_generic (struct nn_xmsg *xmsg, nn_parameterid_t pid, const void *src, size_t srcoff, const enum pserop * __restrict desc)
{
  char * const data = nn_xmsg_addpar (xmsg, pid, ser_generic_size (src, srcoff, desc));
  size_t dstoff = 0;
  while (true)
  {
    switch (*desc)
    {
      case XSTOP:
        return 0;
      case XO: { /* octet sequence */
        ddsi_octetseq_t const * const x = deser_generic_src (src, &srcoff, alignof (ddsi_octetseq_t));
        char * const p = ser_generic_align4 (data, &dstoff);
        *((uint32_t *) p) = x->length;
        if (x->length) memcpy (p + 4, x->value, x->length);
        dstoff += 4 + x->length;
        srcoff += sizeof (*x);
        break;
      }
      case XS: { /* string */
        char const * const * const x = deser_generic_src (src, &srcoff, alignof (char *));
        const uint32_t size = (uint32_t) (strlen (*x) + 1);
        char * const p = ser_generic_align4 (data, &dstoff);
        *((uint32_t *) p) = size;
        memcpy (p + 4, *x, size);
        dstoff += 4 + size;
        srcoff += sizeof (*x);
        break;
      }
      case XZ: { /* string sequence */
        ddsi_stringseq_t const * const x = deser_generic_src (src, &srcoff, alignof (ddsi_stringseq_t));
        char * const p = ser_generic_align4 (data, &dstoff);
        *((uint32_t *) p) = x->n;
        dstoff += 4;
        for (uint32_t i = 0; i < x->n; i++)
        {
          char * const q = ser_generic_align4 (data, &dstoff);
          const uint32_t size = (uint32_t) (strlen (x->strs[i]) + 1);
          *((uint32_t *) q) = size;
          memcpy (q + 4, x->strs[i], size);
          dstoff += 4 + size;
        }
        srcoff += sizeof (*x);
        break;
      }
      case XE1: case XE2: case XE3: { /* enum */
        unsigned const * const x = deser_generic_src (src, &srcoff, alignof (unsigned));
        uint32_t * const p = ser_generic_align4 (data, &dstoff);
        *p = (uint32_t) *x;
        dstoff += 4;
        srcoff += sizeof (*x);
        break;
      }
      case Xi: case Xix2: case Xix3: case Xix4: { /* int32_t(s) */
        int32_t const * const x = deser_generic_src (src, &srcoff, alignof (int32_t));
        const uint32_t cnt = 1 + (uint32_t) (*desc - Xi);
        int32_t * const p = ser_generic_align4 (data, &dstoff);
        for (uint32_t i = 0; i < cnt; i++)
          p[i] = x[i];
        dstoff += cnt * sizeof (*x);
        srcoff += cnt * sizeof (*x);
        break;
      }

      case Xu: case Xux2: case Xux3: case Xux4: case Xux5:  { /* uint32_t(s) */
        uint32_t const * const x = deser_generic_src (src, &srcoff, alignof (uint32_t));
        const uint32_t cnt = 1 + (uint32_t) (*desc - Xu);
        uint32_t * const p = ser_generic_align4 (data, &dstoff);
        for (uint32_t i = 0; i < cnt; i++)
          p[i] = x[i];
        dstoff += cnt * sizeof (*x);
        srcoff += cnt * sizeof (*x);
        break;
      }

      case Xl: { /* int32_t(s) */
        int32_t const * const x = deser_generic_src (src, &srcoff, alignof (uint32_t));
        const uint32_t cnt = 1 + (uint32_t) (*desc - Xu);
        int32_t * const p = ser_generic_align4 (data, &dstoff);
        for (uint32_t i = 0; i < cnt; i++)
          p[i] = x[i];
        dstoff += cnt * sizeof (*x);
        srcoff += cnt * sizeof (*x);
        break;
      }
      case XD: case XDx2: { /* duration(s): int64_t <=> int32_t.uint32_t (seconds.fraction) */
        dds_duration_t const * const x = deser_generic_src (src, &srcoff, alignof (dds_duration_t));
        const uint32_t cnt = 1 + (uint32_t) (*desc - XD);
        uint32_t * const p = ser_generic_align4 (data, &dstoff);
        for (uint32_t i = 0; i < cnt; i++)
        {
          ddsi_duration_t tmp = nn_to_ddsi_duration (x[i]);
          p[2 * i + 0] = (uint32_t) tmp.seconds;
          p[2 * i + 1] = tmp.fraction;
        }
        dstoff += 2 * cnt * sizeof (uint32_t);
        srcoff += cnt * sizeof (*x);
        break;
      }
      case Xo: case Xox2: { /* octet(s) */
        unsigned char const * const x = deser_generic_src (src, &srcoff, alignof (unsigned char));
        const uint32_t cnt = 1 + (uint32_t) (*desc - Xo);
        char * const p = data + dstoff;
        memcpy (p, x, cnt);
        dstoff += cnt;
        srcoff += cnt * sizeof (*x);
        break;
      }
      case Xb: case Xbx2: case XbCOND: { /* boolean(s) */
        unsigned char const * const x = deser_generic_src (src, &srcoff, alignof (unsigned char));
        const uint32_t cnt = (*desc == Xbx2) ? 2 : 1; /* <<<< beware! */
        char * const p = data + dstoff;
        memcpy (p, x, cnt);
        dstoff += cnt;
        srcoff += cnt * sizeof (*x);
        break;
      }
      case XG: { /* GUID */
        nn_guid_t const * const x = deser_generic_src (src, &srcoff, alignof (nn_guid_t));
        const nn_guid_t xn = nn_hton_guid (*x);
        char * const p = data + dstoff;
        memcpy (p, &xn, sizeof (xn));
        dstoff += sizeof (xn);
        srcoff += sizeof (*x);
        break;
      }
      case XK: { /* keyhash */
        nn_keyhash_t const * const x = deser_generic_src (src, &srcoff, alignof (nn_keyhash_t));
        char * const p = data + dstoff;
        memcpy (p, x, sizeof (*x));
        dstoff += sizeof (*x);
        srcoff += sizeof (*x);
        break;
      }
    }
    desc++;
  }
}

static dds_return_t unalias_generic (void * __restrict dst, size_t * __restrict dstoff, const enum pserop * __restrict desc)
{
#define COMPLEX(basecase_, type_, ...) do {                      \
    type_ *x = deser_generic_dst (dst, dstoff, alignof (type_)); \
    const uint32_t cnt = 1 + (uint32_t) (*desc - basecase_);     \
    for (uint32_t xi = 0; xi < cnt; xi++, x++) { __VA_ARGS__; }  \
    *dstoff += cnt * sizeof (*x);                                \
  } while (0)
#define SIMPLE(basecase_, type_) COMPLEX (basecase_, type_, (void) 0)
  while (true)
  {
    switch (*desc)
    {
      case XSTOP:
        return 0;
      case XO: COMPLEX (XO, ddsi_octetseq_t, if (x->value) { x->value = ddsrt_memdup (x->value, x->length); }); break;
      case XS: COMPLEX (XS, char *, if (*x) { *x = ddsrt_strdup (*x); }); break;
      case XZ: COMPLEX (XZ, ddsi_stringseq_t, if (x->n) {
        x->strs = ddsrt_memdup (x->strs, x->n * sizeof (*x->strs));
        for (uint32_t i = 0; i < x->n; i++)
          x->strs[i] = ddsrt_strdup (x->strs[i]);
      }); break;
      case XE1: case XE2: case XE3: COMPLEX (*desc, unsigned, (void) 0); break;
      case Xi: case Xix2: case Xix3: case Xix4: SIMPLE (Xi, int32_t); break;
      case Xu: case Xux2: case Xux3: case Xux4: case Xux5: SIMPLE (Xu, uint32_t); break;
      case Xl: SIMPLE (Xl, int32_t); break;
      case XD: case XDx2: SIMPLE (XD, dds_duration_t); break;
      case Xo: case Xox2: SIMPLE (Xo, unsigned char); break;
      case Xb: case Xbx2: SIMPLE (Xb, unsigned char); break;
      case XbCOND: SIMPLE (XbCOND, unsigned char); break;
      case XG: SIMPLE (XG, nn_guid_t); break;
      case XK: SIMPLE (XK, nn_keyhash_t); break;
    }
    desc++;
  }
#undef SIMPLE
#undef COMPLEX
}

static bool unalias_generic_required (const enum pserop * __restrict desc)
{
  while (*desc != XSTOP)
  {
    switch (*desc++)
    {
      case XO: case XS: case XZ:
        return true;
      default:
        break;
    }
  }
  return false;
}

static bool fini_generic_required (const enum pserop * __restrict desc)
{
  /* the two happen to be the same */
  return unalias_generic_required (desc);
}

static dds_return_t fini_generic (void * __restrict dst, size_t * __restrict dstoff, struct flagset *flagset, uint64_t flag, const enum pserop * __restrict desc)
{
  fini_generic_partial (dst, dstoff, desc, NULL, *flagset->aliased & flag);
  return 0;
}

static dds_return_t valid_generic (const void *src, size_t srcoff, const enum pserop * __restrict desc)
{
#define COMPLEX(basecase_, type_, cond_stmts_) do {                     \
    type_ const *x = deser_generic_src (src, &srcoff, alignof (type_)); \
    const uint32_t cnt = 1 + (uint32_t) (*desc - (basecase_));          \
    for (uint32_t xi = 0; xi < cnt; xi++, x++) { cond_stmts_; }         \
    srcoff += cnt * sizeof (*x);                                        \
  } while (0)
#define SIMPLE(basecase_, type_, cond_) COMPLEX (basecase_, type_, if (!(cond_)) return DDS_RETCODE_BAD_PARAMETER)
#define TRIVIAL(basecase_, type_) COMPLEX (basecase_, type_, (void) 0)
  while (true)
  {
    switch (*desc)
    {
      case XSTOP: return 0;
      case XO: SIMPLE (XO, ddsi_octetseq_t, (x->length == 0) == (x->value == NULL)); break;
      case XS: SIMPLE (XS, const char *, *x != NULL); break;
      case XZ: COMPLEX (XZ, ddsi_stringseq_t, {
        if ((x->n == 0) != (x->strs == NULL))
          return DDS_RETCODE_BAD_PARAMETER;
        for (uint32_t i = 0; i < x->n; i++)
          if (x->strs[i] == NULL)
            return DDS_RETCODE_BAD_PARAMETER;
      }); break;
      case XE1: case XE2: case XE3: SIMPLE (*desc, unsigned, *x <= 1 + (unsigned) *desc - XE1); break;
      case Xi: case Xix2: case Xix3: case Xix4: TRIVIAL (Xi, int32_t); break;
      case Xu: case Xux2: case Xux3: case Xux4: case Xux5: TRIVIAL (Xu, uint32_t); break;
      case Xl: SIMPLE (Xl, int32_t, *x == DDS_LENGTH_UNLIMITED || *x > 1); break;
      case XD: case XDx2: SIMPLE (XD, dds_duration_t, *x >= 0); break;
      case Xo: case Xox2: TRIVIAL (Xo, unsigned char); break;
      case Xb: case Xbx2: SIMPLE (Xb, unsigned char, *x == 0 || *x == 1); break;
      case XbCOND: SIMPLE (XbCOND, unsigned char, *x == 0 || *x == 1); break;
      case XG: TRIVIAL (XG, nn_guid_t); break;
      case XK: TRIVIAL (XK, nn_keyhash_t); break;
    }
    desc++;
  }
#undef TRIVIAL
#undef SIMPLE
#undef COMPLEX
}

static bool equal_generic (const void *srcx, const void *srcy, size_t srcoff, const enum pserop * __restrict desc)
{
#define COMPLEX(basecase_, type_, cond_stmts_) do {                      \
    type_ const *x = deser_generic_src (srcx, &srcoff, alignof (type_)); \
    type_ const *y = deser_generic_src (srcy, &srcoff, alignof (type_)); \
    const uint32_t cnt = 1 + (uint32_t) (*desc - (basecase_));           \
    for (uint32_t xi = 0; xi < cnt; xi++, x++, y++) { cond_stmts_; }     \
    srcoff += cnt * sizeof (*x);                                         \
  } while (0)
#define SIMPLE(basecase_, type_, cond_) COMPLEX (basecase_, type_, if (!(cond_)) return false)
#define TRIVIAL(basecase_, type_) SIMPLE (basecase_, type_, *x == *y)
  while (true)
  {
    switch (*desc)
    {
      case XSTOP:
        return true;
      case XO:
        SIMPLE (XO, ddsi_octetseq_t,
                (x->length == y->length) &&
                (x->length == 0 || memcmp (x->value, y->value, x->length) == 0));
        break;
      case XS:
        SIMPLE (XS, const char *, strcmp (*x, *y) == 0);
        break;
      case XZ:
        COMPLEX (XZ, ddsi_stringseq_t, {
          if (x->n != y->n)
            return false;
          for (uint32_t i = 0; i < x->n; i++)
            if (strcmp (x->strs[i], y->strs[i]) != 0)
              return false;
        });
        break;
      case XE1: case XE2: case XE3: TRIVIAL (*desc, unsigned); break;
      case Xi: case Xix2: case Xix3: case Xix4: TRIVIAL (Xi, int32_t); break;
      case Xu: case Xux2: case Xux3: case Xux4: case Xux5: TRIVIAL (Xu, uint32_t); break;
      case Xl: TRIVIAL (Xl, int32_t); break;
      case XD: case XDx2: TRIVIAL (XD, dds_duration_t); break;
      case Xo: case Xox2: TRIVIAL (Xo, unsigned char); break;
      case Xb: case Xbx2: TRIVIAL (Xb, unsigned char); break;
      case XbCOND:
        COMPLEX (XbCOND, unsigned char, {
          if (*x != *y)
            return false;
          if (*x == false)
            return true;
        });
        break;
      case XG: SIMPLE (XG, nn_guid_t, memcmp (x, y, sizeof (*x))); break;
      case XK: SIMPLE (XK, nn_keyhash_t, memcmp (x, y, sizeof (*x))); break;
    }
    desc++;
  }
#undef TRIVIAL
#undef SIMPLE
#undef COMPLEX
}

#define membersize(type, member) sizeof (((type *) 0)->member)
#define ENTRY(PFX_, NAME_, member_, flag_, validate_, ...)                               \
  { PID_##NAME_, flag_, PFX_##_##NAME_, #NAME_, offsetof (struct nn_plist, member_),     \
    membersize (struct nn_plist, member_), { .desc = { __VA_ARGS__, XSTOP } }, validate_ \
  }
#define QPV(NAME_, name_, ...) ENTRY(QP, NAME_, qos.name_, PDF_QOS, dvx_##name_, __VA_ARGS__)
#define PPV(NAME_, name_, ...) ENTRY(PP, NAME_, name_, 0, dvx_##name_, __VA_ARGS__)
#define QP(NAME_, name_, ...) ENTRY(QP, NAME_, qos.name_, PDF_QOS, 0, __VA_ARGS__)
#define PP(NAME_, name_, ...) ENTRY(PP, NAME_, name_, 0, 0, __VA_ARGS__)
#define PPM(NAME_, name_, ...) ENTRY(PP, NAME_, name_, PDF_ALLOWMULTI, 0, __VA_ARGS__)

static int protocol_version_is_newer (nn_protocol_version_t pv)
{
  return (pv.major < RTPS_MAJOR) ? 0 : (pv.major > RTPS_MAJOR) ? 1 : (pv.minor > RTPS_MINOR);
}

static dds_return_t dvx_durability_service (void * __restrict dst, const struct dd * __restrict dd)
{
  /* Accept all zero durability because of CoreDX, final_validation is more strict */
  (void) dd;
  return validate_durability_service_qospolicy_acceptzero (dst, true);
}

static dds_return_t dvx_history (void * __restrict dst, const struct dd * __restrict dd)
{
  (void) dd;
  return validate_history_qospolicy (dst);
}

static dds_return_t dvx_resource_limits (void * __restrict dst, const struct dd * __restrict dd)
{
  (void) dd;
  return validate_resource_limits_qospolicy (dst);
}

static dds_return_t dvx_participant_guid (void * __restrict dst, const struct dd * __restrict dd)
{
  const nn_guid_t *g = dst;
  (void) dd;
  if (g->prefix.u[0] == 0 && g->prefix.u[1] == 0 && g->prefix.u[2] == 0)
    return (g->entityid.u == 0) ? 0 : DDS_RETCODE_BAD_PARAMETER;
  else
    return (g->entityid.u == NN_ENTITYID_PARTICIPANT) ? 0 : DDS_RETCODE_BAD_PARAMETER;
}

static dds_return_t dvx_group_guid (void * __restrict dst, const struct dd * __restrict dd)
{
  const nn_guid_t *g = dst;
  (void) dd;
  if (g->prefix.u[0] == 0 && g->prefix.u[1] == 0 && g->prefix.u[2] == 0)
    return (g->entityid.u == 0) ? 0 : DDS_RETCODE_BAD_PARAMETER;
  else
    return (g->entityid.u != 0) ? 0 : DDS_RETCODE_BAD_PARAMETER;
}

static dds_return_t dvx_endpoint_guid (void * __restrict dst, const struct dd * __restrict dd)
{
  nn_guid_t *g = dst;
  if (g->prefix.u[0] == 0 && g->prefix.u[1] == 0 && g->prefix.u[2] == 0)
    return (g->entityid.u == 0) ? 0 : DDS_RETCODE_BAD_PARAMETER;
  switch (g->entityid.u & NN_ENTITYID_KIND_MASK)
  {
    case NN_ENTITYID_KIND_WRITER_WITH_KEY:
    case NN_ENTITYID_KIND_WRITER_NO_KEY:
    case NN_ENTITYID_KIND_READER_NO_KEY:
    case NN_ENTITYID_KIND_READER_WITH_KEY:
      return 0;
    default:
      return (protocol_version_is_newer (dd->protocol_version) ? 0 : DDS_RETCODE_BAD_PARAMETER);
  }
}

#ifdef DDSI_INCLUDE_SSM
static dds_return_t dvx_reader_favours_ssm (void * __restrict dst, const struct dd * __restrict dd)
{
  uint32_t * const favours_ssm = dst;
  (void) dd;
  /* any unrecognized state: avoid SSM */
  if (*favours_ssm != 0 && *favours_ssm != 1)
    *favours_ssm = 0;
  return 0;
}
#endif

/* Standardized parameters -- QoS _MUST_ come first (nn_plist_init_tables verifies this) because
   it allows early-out when processing a dds_qos_t instead of an nn_plist_t */
static const struct piddesc piddesc_omg[] = {
  QP  (USER_DATA,                           user_data, XO),
  QP  (TOPIC_NAME,                          topic_name, XS),
  QP  (TYPE_NAME,                           type_name, XS),
  QP  (TOPIC_DATA,                          topic_data, XO),
  QP  (GROUP_DATA,                          group_data, XO),
  QP  (DURABILITY,                          durability, XE3),
  /* CoreDX's use of all-zero durability service QoS means we can't use l; interdependencies between QoS
     values means we must validate the combination anyway */
  QPV (DURABILITY_SERVICE,                  durability_service, XD, XE1, Xix4),
  QP  (DEADLINE,                            deadline, XD),
  QP  (LATENCY_BUDGET,                      latency_budget, XD),
  QP  (LIVELINESS,                          liveliness, XE2, XD),
  /* Reliability encoding does not follow the rules (best-effort/reliable map to 1/2 instead of 0/1 */
  { PID_RELIABILITY, PDF_QOS | PDF_FUNCTION, QP_RELIABILITY, "RELIABILITY",
    offsetof (struct nn_plist, qos.reliability), membersize (struct nn_plist, qos.reliability),
    { .f = { .deser = deser_reliability, .ser = ser_reliability, .valid = valid_reliability, .equal = equal_reliability } }, 0 },
  QP  (LIFESPAN,                            lifespan, XD),
  QP  (DESTINATION_ORDER,                   destination_order, XE1),
  /* History depth is ignored when kind = KEEP_ALL, and must be >= 1 when KEEP_LAST, so can't use "l" */
  QPV (HISTORY,                             history, XE1, Xi),
  QPV (RESOURCE_LIMITS,                     resource_limits, Xix3),
  QP  (OWNERSHIP,                           ownership, XE1),
  QP  (OWNERSHIP_STRENGTH,                  ownership_strength, Xi),
  QP  (PRESENTATION,                        presentation, XE2, Xbx2),
  QP  (PARTITION,                           partition, XZ),
  QP  (TIME_BASED_FILTER,                   time_based_filter, XD),
  QP  (TRANSPORT_PRIORITY,                  transport_priority, Xi),
  PP  (PROTOCOL_VERSION,                    protocol_version, Xox2),
  PP  (VENDORID,                            vendorid, Xox2),
  PP  (EXPECTS_INLINE_QOS,                  expects_inline_qos, Xb),
  PP  (PARTICIPANT_MANUAL_LIVELINESS_COUNT, participant_manual_liveliness_count, Xi),
  PP  (PARTICIPANT_BUILTIN_ENDPOINTS,       participant_builtin_endpoints, Xu),
  PP  (PARTICIPANT_LEASE_DURATION,          participant_lease_duration, XD),
  PPV (PARTICIPANT_GUID,                    participant_guid, XG),
  PPV (GROUP_GUID,                          group_guid, XG),
  PP  (BUILTIN_ENDPOINT_SET,                builtin_endpoint_set, Xu),
  PP  (ENTITY_NAME,                         entity_name, XS),
  PP  (KEYHASH,                             keyhash, XK),
  PPV (ENDPOINT_GUID,                       endpoint_guid, XG),
#ifdef DDSI_INCLUDE_SSM
  PPV (READER_FAVOURS_SSM,                  reader_favours_ssm, Xu),
#endif
  { PID_STATUSINFO, PDF_FUNCTION, PP_STATUSINFO, "STATUSINFO",
    offsetof (struct nn_plist, statusinfo), membersize (struct nn_plist, statusinfo),
    { .f = { .deser = deser_statusinfo, .ser = ser_statusinfo } }, 0 },
  /* Locators are difficult to deal with because they can occur multi times to represent a set;
     that is manageable for deser, unalias and fini, but it breaks ser because that one only
     generates a single parameter header */
  { PID_UNICAST_LOCATOR, PDF_FUNCTION | PDF_ALLOWMULTI,
    PP_UNICAST_LOCATOR, "UNICAST_LOCATOR",
    offsetof (struct nn_plist, unicast_locators), membersize (struct nn_plist, unicast_locators),
    { .f = { .deser = deser_locator, .ser = ser_locator, .unalias = unalias_locator, .fini = fini_locator } }, 0 },
  { PID_MULTICAST_LOCATOR, PDF_FUNCTION | PDF_ALLOWMULTI,
    PP_MULTICAST_LOCATOR, "MULTICAST_LOCATOR",
    offsetof (struct nn_plist, multicast_locators), membersize (struct nn_plist, multicast_locators),
    { .f = { .deser = deser_locator, .ser = ser_locator, .unalias = unalias_locator, .fini = fini_locator } }, 0 },
  { PID_DEFAULT_UNICAST_LOCATOR, PDF_FUNCTION | PDF_ALLOWMULTI,
    PP_DEFAULT_UNICAST_LOCATOR, "DEFAULT_UNICAST_LOCATOR",
    offsetof (struct nn_plist, default_unicast_locators), membersize (struct nn_plist, default_unicast_locators),
    { .f = { .deser = deser_locator, .ser = ser_locator, .unalias = unalias_locator, .fini = fini_locator } }, 0 },
  { PID_DEFAULT_MULTICAST_LOCATOR, PDF_FUNCTION | PDF_ALLOWMULTI,
    PP_DEFAULT_MULTICAST_LOCATOR, "DEFAULT_MULTICAST_LOCATOR",
    offsetof (struct nn_plist, default_multicast_locators), membersize (struct nn_plist, default_multicast_locators),
    { .f = { .deser = deser_locator, .ser = ser_locator, .unalias = unalias_locator, .fini = fini_locator } }, 0 },
  { PID_METATRAFFIC_UNICAST_LOCATOR, PDF_FUNCTION | PDF_ALLOWMULTI,
    PP_METATRAFFIC_UNICAST_LOCATOR, "METATRAFFIC_UNICAST_LOCATOR",
    offsetof (struct nn_plist, metatraffic_unicast_locators), membersize (struct nn_plist, metatraffic_unicast_locators),
    { .f = { .deser = deser_locator, .ser = ser_locator, .unalias = unalias_locator, .fini = fini_locator } }, 0 },
  { PID_METATRAFFIC_MULTICAST_LOCATOR, PDF_FUNCTION | PDF_ALLOWMULTI,
    PP_METATRAFFIC_MULTICAST_LOCATOR, "METATRAFFIC_MULTICAST_LOCATOR",
    offsetof (struct nn_plist, metatraffic_multicast_locators), membersize (struct nn_plist, metatraffic_multicast_locators),
    { .f = { .deser = deser_locator, .ser = ser_locator, .unalias = unalias_locator, .fini = fini_locator } }, 0 },
  /* PID_..._{IPADDRESS,PORT} is impossible to deal with and are never generated, only accepted.
     The problem is that there one needs additional state (and even then there is no clear
     interpretation) ... So they'll have to be special-cased */
  { PID_SENTINEL, 0, 0, NULL, 0, 0, { .desc = { XSTOP } }, 0 }
};

/* Understood parameters for Eclipse Foundation (Cyclone DDS) vendor code */
static const struct piddesc piddesc_eclipse[] = {
  QP  (PRISMTECH_ENTITY_FACTORY,            entity_factory, Xb),
  QP  (PRISMTECH_READER_LIFESPAN,           reader_lifespan, Xb, XD),
  QP  (PRISMTECH_WRITER_DATA_LIFECYCLE,     writer_data_lifecycle, Xb),
  QP  (PRISMTECH_READER_DATA_LIFECYCLE,     reader_data_lifecycle, XDx2),
  QP  (PRISMTECH_SUBSCRIPTION_KEYS,         subscription_keys, XbCOND, XZ),
  { PID_PAD, PDF_QOS, QP_CYCLONE_IGNORELOCAL, "CYCLONE_IGNORELOCAL",
    offsetof (struct nn_plist, qos.ignorelocal), membersize (struct nn_plist, qos.ignorelocal),
    { .desc = { XE2, XSTOP } }, 0 },
  PP  (PRISMTECH_BUILTIN_ENDPOINT_SET,      prismtech_builtin_endpoint_set, Xu),
  PP  (PRISMTECH_PARTICIPANT_VERSION_INFO,  prismtech_participant_version_info, Xux5, XS),
  PP  (PRISMTECH_EXEC_NAME,                 exec_name, XS),
  PP  (PRISMTECH_PROCESS_ID,                process_id, Xu),
  PP  (PRISMTECH_NODE_NAME,                 node_name, XS),
  PP  (PRISMTECH_TYPE_DESCRIPTION,          type_description, XS),
  { PID_SENTINEL, 0, 0, NULL, 0, 0, { .desc = { XSTOP } }, 0 }
};

/* Understood parameters for PrismTech vendor code */
static const struct piddesc piddesc_prismtech[] = {
  QP  (PRISMTECH_ENTITY_FACTORY,            entity_factory, Xb),
  QP  (PRISMTECH_READER_LIFESPAN,           reader_lifespan, Xb, XD),
  QP  (PRISMTECH_WRITER_DATA_LIFECYCLE,     writer_data_lifecycle, Xb),
  QP  (PRISMTECH_READER_DATA_LIFECYCLE,     reader_data_lifecycle, XDx2),
  QP  (PRISMTECH_SUBSCRIPTION_KEYS,         subscription_keys, XbCOND, XZ),
  PP  (PRISMTECH_BUILTIN_ENDPOINT_SET,      prismtech_builtin_endpoint_set, Xu),
  PP  (PRISMTECH_PARTICIPANT_VERSION_INFO,  prismtech_participant_version_info, Xux5, XS),
  PP  (PRISMTECH_EXEC_NAME,                 exec_name, XS),
  PP  (PRISMTECH_PROCESS_ID,                process_id, Xu),
  PP  (PRISMTECH_NODE_NAME,                 node_name, XS),
  PP  (PRISMTECH_TYPE_DESCRIPTION,          type_description, XS),
  { PID_SENTINEL, 0, 0, NULL, 0, 0, { .desc = { XSTOP } }, 0 }
};

#undef PPM
#undef PP
#undef QP
#undef PPV
#undef QPV
#undef ENTRY
#undef membersize

/* Parameters to be included in messages we generate */
static const struct piddesc *piddesc_tables_output[] = {
  piddesc_omg,
  piddesc_eclipse
};

/* All known parameters -- this can potentially include
   parameters from other vendors that we never generate
   but that we do recognize on input and store for some
   purpose other than the internal workings of Cyclone,
   and that require fini/unalias processing */
static const struct piddesc *piddesc_tables_all[] = {
  piddesc_omg,
  piddesc_eclipse
};

struct piddesc_index {
  size_t index_max;
  const struct piddesc **index;
  /* include source table for generating the index --
     it's easier to generate the index at startup then
     to maintain in the source */
  const struct piddesc *table;
};

/* Vendor code to vendor-specific table mapping, with index
   vendor codes are currently of the form 1.x with x a small
   number > 0 (and that's not likely to change) so we have
   a table for major = 1 and use index 0 for the standard
   ones.

   Sizes are such that the highest PID (without flags) in
   table are the last entry in the array.  Checked by
   nn_plist_init_tables.

   FIXME: should compute them at build-time */
#ifdef DDSI_INCLUDE_SSM
static const struct piddesc *piddesc_omg_index[115];
#else /* status info is the highest */
static const struct piddesc *piddesc_omg_index[114];
#endif
static const struct piddesc *piddesc_eclipse_index[19];
static const struct piddesc *piddesc_prismtech_index[19];

#define INDEX_ANY(vendorid_, tab_) [vendorid_] = { \
    .index_max = sizeof (piddesc_##tab_##_index) / sizeof (piddesc_##tab_##_index[0]) - 1, \
    .index = (const struct piddesc **) piddesc_##tab_##_index, \
    .table = piddesc_##tab_ }
#define INDEX(VENDOR_, tab_) INDEX_ANY (NN_VENDORID_MINOR_##VENDOR_, tab_)

static const struct piddesc_index piddesc_vendor_index[] = {
  INDEX_ANY (0, omg),
  INDEX (ECLIPSE, eclipse),
  INDEX (PRISMTECH_OSPL, prismtech),
  INDEX (PRISMTECH_JAVA, prismtech),
  INDEX (PRISMTECH_LITE, prismtech),
  INDEX (PRISMTECH_GATEWAY, prismtech),
  INDEX (PRISMTECH_CLOUD, prismtech)
};

#undef INDEX
#undef INDEX_ANY

/* List of entries that require unalias, fini processing;
   initialized by nn_plist_init_tables; will assert when
   table too small or too large */
static const struct piddesc *piddesc_unalias[18];
static const struct piddesc *piddesc_fini[18];
static ddsrt_once_t table_init_control = DDSRT_ONCE_INIT;

static nn_parameterid_t pid_without_flags (nn_parameterid_t pid)
{
  return (nn_parameterid_t) (pid & ~(PID_VENDORSPECIFIC_FLAG | PID_UNRECOGNIZED_INCOMPATIBLE_FLAG));
}

static int piddesc_cmp_qos_addr (const void *va, const void *vb)
{
  struct piddesc const * const *a = (struct piddesc const * const *) va;
  struct piddesc const * const *b = (struct piddesc const * const *) vb;
  /* QoS go first, then address */
  if (((*a)->flags & PDF_QOS) != ((*b)->flags & PDF_QOS))
    return ((*a)->flags & PDF_QOS) ? -1 : 1;
  else
    return ((uintptr_t) *a == (uintptr_t) *b) ? 0 : ((uintptr_t) *a < (uintptr_t) *b) ? -1 : 1;
}

static void nn_plist_init_tables_real (void)
{
  /* make index of pid -> entry */
  for (size_t i = 0; i < sizeof (piddesc_vendor_index) / sizeof (piddesc_vendor_index[0]); i++)
  {
    const struct piddesc *table = piddesc_vendor_index[i].table;
    if (table == NULL)
      continue;
    struct piddesc const **index = piddesc_vendor_index[i].index;
#ifndef NDEBUG
    nn_parameterid_t maxpid = 0;
    bool only_qos_seen = true;
#endif
    for (size_t j = 0; table[j].pid != PID_SENTINEL; j++)
    {
      nn_parameterid_t pid = pid_without_flags (table[j].pid);
#ifndef NDEBUG
      /* Table must first list QoS, then other parameters */
      assert (only_qos_seen || !(table[j].flags & PDF_QOS));
      if (!(table[j].flags & PDF_QOS))
        only_qos_seen = false;
      /* Track max PID so we can verify the table is no larger
         than necessary */
      if (pid > maxpid)
        maxpid = pid;
#endif
      /* PAD is used for entries that are never visible on the wire
         and the decoder assumes the PAD entries will be skipped
         because they don't map to an entry */
      if (pid == PID_PAD)
        continue;
      assert (pid <= piddesc_vendor_index[i].index_max);
      assert (index[pid] == NULL || index[pid] == &table[j]);
      index[pid] = &table[j];
    }
    assert (maxpid == piddesc_vendor_index[i].index_max);
  }

  /* PIDs requiring unalias; there is overlap between the tables
     (because of different vendor codes mapping to the same entry
     in qos/plist).  Use the "present" flags to filter out
     duplicates. */
  uint64_t pf = 0, qf = 0;
  size_t unalias_index = 0;
  size_t fini_index = 0;
  for (size_t i = 0; i < sizeof (piddesc_vendor_index) / sizeof (piddesc_vendor_index[0]); i++)
  {
    const struct piddesc *table = piddesc_vendor_index[i].table;
    if (table == NULL)
      continue;
    for (size_t j = 0; table[j].pid != PID_SENTINEL; j++)
    {
      uint64_t * const f = (table[j].flags & PDF_QOS) ? &qf : &pf;
      if (*f & table[j].present_flag)
        continue;
      *f |= table[j].present_flag;
      if (((table[j].flags & PDF_FUNCTION) && table[j].op.f.unalias) ||
          (!(table[j].flags & PDF_FUNCTION) && unalias_generic_required (table[j].op.desc)))
      {
        assert (unalias_index < sizeof (piddesc_unalias) / sizeof (piddesc_unalias[0]));
        piddesc_unalias[unalias_index++] = &table[j];
      }
      if (((table[j].flags & PDF_FUNCTION) && table[j].op.f.fini) ||
          (!(table[j].flags & PDF_FUNCTION) && fini_generic_required (table[j].op.desc)))
      {
        assert (fini_index < sizeof (piddesc_fini) / sizeof (piddesc_fini[0]));
        piddesc_fini[fini_index++] = &table[j];
      }
    }
  }
  assert (unalias_index == sizeof (piddesc_unalias) / sizeof (piddesc_unalias[0]) &&
          fini_index == sizeof (piddesc_fini) / sizeof (piddesc_fini[0]));
  qsort ((void *) piddesc_unalias, unalias_index, sizeof (piddesc_unalias[0]), piddesc_cmp_qos_addr);
  qsort ((void *) piddesc_fini, fini_index, sizeof (piddesc_fini[0]), piddesc_cmp_qos_addr);
#ifndef NDEBUG
  {
    size_t i;
    for (i = 0; i < unalias_index; i++)
      if (!(piddesc_unalias[i]->flags & PDF_QOS))
        break;
    for (; i < unalias_index; i++)
      assert (!(piddesc_unalias[i]->flags & PDF_QOS));
    for (i = 0; i < fini_index; i++)
      if (!(piddesc_fini[i]->flags & PDF_QOS))
        break;
    for (; i < fini_index; i++)
      assert (!(piddesc_fini[i]->flags & PDF_QOS));
  }
#endif
}

void nn_plist_init_tables (void)
{
  ddsrt_once (&table_init_control, nn_plist_init_tables_real);
}

static void plist_or_xqos_fini (void * __restrict dst, size_t shift, uint64_t pmask, uint64_t qmask)
{
  /* shift == 0: plist, shift > 0: just qos */
  struct flagset pfs, qfs;
  /* DDS manipulation can be done without creating a participant, so we may
     have to initialize tables just-in-time */
  if (piddesc_fini[0] == NULL)
    nn_plist_init_tables ();
  if (shift > 0)
  {
    dds_qos_t *qos = dst;
    pfs = (struct flagset) { 0 };
    qfs = (struct flagset) { .present = &qos->present, .aliased = &qos->aliased };
  }
  else
  {
    nn_plist_t *plist = dst;
    pfs = (struct flagset) { .present = &plist->present, .aliased = &plist->aliased };
    qfs = (struct flagset) { .present = &plist->qos.present, .aliased = &plist->qos.aliased };
  }
  for (size_t i = 0; i < sizeof (piddesc_fini) / sizeof (piddesc_fini[0]); i++)
  {
    struct piddesc const * const entry = piddesc_fini[i];
    if (shift > 0 && !(entry->flags & PDF_QOS))
      break;
    assert (entry->plist_offset >= shift);
    assert (shift == 0 || entry->plist_offset - shift < sizeof (dds_qos_t));
    size_t dstoff = entry->plist_offset - shift;
    struct flagset * const fs = (entry->flags & PDF_QOS) ? &qfs : &pfs;
    uint64_t mask = (entry->flags & PDF_QOS) ? qmask : pmask;
    if (*fs->present & entry->present_flag & mask)
    {
      if (!(entry->flags & PDF_FUNCTION))
        fini_generic (dst, &dstoff, fs, entry->present_flag, entry->op.desc);
      else if (entry->op.f.fini)
        entry->op.f.fini (dst, &dstoff, fs, entry->present_flag);
    }
  }
  if (pfs.present) { *pfs.present &= ~pmask; *pfs.aliased &= ~pmask; }
  *qfs.present &= ~qmask; *qfs.aliased &= ~qmask;
}

static void plist_or_xqos_unalias (void * __restrict dst, size_t shift)
{
  /* shift == 0: plist, shift > 0: just qos */
  struct flagset pfs, qfs;
  /* DDS manipulation can be done without creating a participant, so we may
     have to initialize tables just-in-time */
  if (piddesc_unalias[0] == NULL)
    nn_plist_init_tables ();
  if (shift > 0)
  {
    dds_qos_t *qos = dst;
    pfs = (struct flagset) { 0 };
    qfs = (struct flagset) { .present = &qos->present, .aliased = &qos->aliased };
  }
  else
  {
    nn_plist_t *plist = dst;
    pfs = (struct flagset) { .present = &plist->present, .aliased = &plist->aliased };
    qfs = (struct flagset) { .present = &plist->qos.present, .aliased = &plist->qos.aliased };
  }
  for (size_t i = 0; i < sizeof (piddesc_unalias) / sizeof (piddesc_unalias[0]); i++)
  {
    struct piddesc const * const entry = piddesc_unalias[i];
    if (shift > 0 && !(entry->flags & PDF_QOS))
      break;
    assert (entry->plist_offset >= shift);
    assert (shift == 0 || entry->plist_offset - shift < sizeof (dds_qos_t));
    size_t dstoff = entry->plist_offset - shift;
    struct flagset * const fs = (entry->flags & PDF_QOS) ? &qfs : &pfs;
    if ((*fs->present & entry->present_flag) && (*fs->aliased & entry->present_flag))
    {
      if (!(entry->flags & PDF_FUNCTION))
        unalias_generic (dst, &dstoff, entry->op.desc);
      else if (entry->op.f.unalias)
        entry->op.f.unalias (dst, &dstoff);
      *fs->aliased &= ~entry->present_flag;
    }
  }
  assert (pfs.aliased == NULL || *pfs.aliased == 0);
  assert (*qfs.aliased == 0);
}

static void plist_or_xqos_mergein_missing (void * __restrict dst, const void * __restrict src, size_t shift, uint64_t pmask, uint64_t qmask)
{
  /* shift == 0: plist, shift > 0: just qos */
  struct flagset pfs_src, qfs_src;
  struct flagset pfs_dst, qfs_dst;
#ifndef NDEBUG
  const uint64_t aliased_dst_inp = (shift == 0) ? ((nn_plist_t *) dst)->aliased : 0;
  const uint64_t aliased_dst_inq = (shift == 0) ? ((nn_plist_t *) dst)->qos.aliased : ((dds_qos_t *) dst)->aliased;
#endif
  if (shift > 0)
  {
    dds_qos_t *qos_dst = dst;
    const dds_qos_t *qos_src = src;
    pfs_dst = (struct flagset) { 0 };
    qfs_dst = (struct flagset) { .present = &qos_dst->present, .aliased = &qos_dst->aliased };
    pfs_src = (struct flagset) { 0 };
    qfs_src = (struct flagset) { .present = (uint64_t *) &qos_src->present, .aliased = (uint64_t *) &qos_src->aliased };
  }
  else
  {
    nn_plist_t *plist_dst = dst;
    const nn_plist_t *plist_src = src;
    pfs_dst = (struct flagset) { .present = &plist_dst->present, .aliased = &plist_dst->aliased };
    qfs_dst = (struct flagset) { .present = &plist_dst->qos.present, .aliased = &plist_dst->qos.aliased };
    pfs_src = (struct flagset) { .present = (uint64_t *) &plist_src->present, .aliased = (uint64_t *) &plist_src->aliased };
    qfs_src = (struct flagset) { .present = (uint64_t *) &plist_src->qos.present, .aliased = (uint64_t *) &plist_src->qos.aliased };
  }
  /* aliased may never have any bits set that are clear in present */
  assert (pfs_dst.present == NULL || (aliased_dst_inp & ~ *pfs_dst.present) == 0);
  assert ((aliased_dst_inq & ~ *qfs_dst.present) == 0);
  for (size_t k = 0; k < sizeof (piddesc_tables_all) / sizeof (piddesc_tables_all[0]); k++)
  {
    struct piddesc const * const table = piddesc_tables_all[k];
    for (uint32_t i = 0; table[i].pid != PID_SENTINEL; i++)
    {
      struct piddesc const * const entry = &table[i];
      if (shift > 0 && !(entry->flags & PDF_QOS))
        break;
      assert (entry->plist_offset >= shift);
      assert (shift == 0 || entry->plist_offset - shift < sizeof (dds_qos_t));
      size_t dstoff = entry->plist_offset - shift;
      struct flagset * const fs_dst = (entry->flags & PDF_QOS) ? &qfs_dst : &pfs_dst;
      struct flagset * const fs_src = (entry->flags & PDF_QOS) ? &qfs_src : &pfs_src;
      uint64_t const mask = (entry->flags & PDF_QOS) ? qmask : pmask;
      /* skip if already present in dst or absent in src */
      if (!(*fs_dst->present & entry->present_flag) && (*fs_src->present & mask & entry->present_flag))
      {
        /* bitwise copy, mark as aliased & unalias; have to unalias fields one-by-one rather than
         do this for all fields and call "unalias" on the entire object because fields that are
         already present may be aliased, and it would be somewhat impolite to change that.

         Note: dst & src have the same type, so offset in src is the same;
         Note: unalias may have to look at */
        memcpy ((char *) dst + dstoff, (const char *) src + dstoff, entry->size);
        *fs_dst->present |= entry->present_flag;
        if (!(entry->flags & PDF_FUNCTION))
          unalias_generic (dst, &dstoff, entry->op.desc);
        else if (entry->op.f.unalias)
          entry->op.f.unalias (dst, &dstoff);
      }
    }
  }
  /* all entries in src should be present in dst (but there may be more) */
  assert (pfs_dst.present == NULL || (*pfs_src.present & pmask & ~ *pfs_dst.present) == 0);
  assert ((*qfs_src.present & qmask & ~ *qfs_dst.present) == 0);
  /* the only aliased entries in dst may be ones that were aliased on input */
  assert (pfs_dst.aliased == NULL || (*pfs_dst.aliased & ~ aliased_dst_inp) == 0);
  assert ((*qfs_dst.aliased & ~ aliased_dst_inq) == 0);
}

static void plist_or_xqos_addtomsg (struct nn_xmsg *xmsg, const void * __restrict src, size_t shift, uint64_t pwanted, uint64_t qwanted)
{
  /* shift == 0: plist, shift > 0: just qos */
  uint64_t pw, qw;
  if (shift > 0)
  {
    const dds_qos_t *qos = src;
    pw = 0;
    qw = qos->present & qwanted;
  }
  else
  {
    const nn_plist_t *plist = src;
    pw = plist->present & pwanted;
    qw = plist->qos.present & qwanted;
  }
  for (size_t k = 0; k < sizeof (piddesc_tables_output) / sizeof (piddesc_tables_output[0]); k++)
  {
    struct piddesc const * const table = piddesc_tables_output[k];
    for (uint32_t i = 0; table[i].pid != PID_SENTINEL; i++)
    {
      struct piddesc const * const entry = &table[i];
      if (entry->pid == PID_PAD)
        continue;
      if (((entry->flags & PDF_QOS) ? qw : pw) & entry->present_flag)
      {
        assert (entry->plist_offset >= shift);
        assert (shift == 0 || entry->plist_offset - shift < sizeof (dds_qos_t));
        size_t srcoff = entry->plist_offset - shift;
        if (!(entry->flags & PDF_FUNCTION))
          ser_generic (xmsg, entry->pid, src, srcoff, entry->op.desc);
        else
          entry->op.f.ser (xmsg, entry->pid, src, srcoff);
      }
    }
  }
}

void nn_plist_fini (nn_plist_t *plist)
{
  plist_or_xqos_fini (plist, 0, ~(uint64_t)0, ~(uint64_t)0);
}

void nn_plist_unalias (nn_plist_t *plist)
{
  plist_or_xqos_unalias (plist, 0);
}

static dds_return_t nn_xqos_valid_strictness (const struct ddsrt_log_cfg *logcfg, const dds_qos_t *xqos, bool strict)
{
  dds_return_t ret;
  if (piddesc_unalias[0] == NULL)
    nn_plist_init_tables ();
  for (size_t k = 0; k < sizeof (piddesc_tables_all) / sizeof (piddesc_tables_all[0]); k++)
  {
    struct piddesc const * const table = piddesc_tables_all[k];
    for (uint32_t i = 0; table[i].pid != PID_SENTINEL; i++)
    {
      struct piddesc const * const entry = &table[i];
      if (!(entry->flags & PDF_QOS))
        break;
      if (xqos->present & entry->present_flag)
      {
        const size_t srcoff = entry->plist_offset - offsetof (nn_plist_t, qos);
        if (!(entry->flags & PDF_FUNCTION))
          ret = valid_generic (xqos, srcoff, entry->op.desc);
        else
          ret = entry->op.f.valid (xqos, srcoff);
        if (ret < 0)
        {
          DDS_CLOG (DDS_LC_PLIST, logcfg, "nn_xqos_valid: %s invalid\n", entry->name);
          return ret;
        }
      }
    }
  }
  if ((ret = final_validation_qos (xqos, (nn_protocol_version_t) { RTPS_MAJOR, RTPS_MINOR }, NN_VENDORID_ECLIPSE, NULL, strict)) < 0)
  {
    DDS_CLOG (DDS_LC_PLIST, logcfg, "nn_xqos_valid: final validation failed\n");
  }
  return ret;
}

dds_return_t nn_xqos_valid (const struct ddsrt_log_cfg *logcfg, const dds_qos_t *xqos)
{
  return nn_xqos_valid_strictness (logcfg, xqos, true);
}

uint64_t nn_xqos_delta (const dds_qos_t *x, const dds_qos_t *y, uint64_t mask)
{
  if (piddesc_unalias[0] == NULL)
    nn_plist_init_tables ();
  /* Returns QP_... set for settings where x differs from y; if
     present in x but not in y (or in y but not in x) it counts as a
     difference. */
  uint64_t delta = (x->present ^ y->present) & mask;
  const uint64_t check = (x->present & y->present) & mask;
  for (size_t k = 0; k < sizeof (piddesc_tables_all) / sizeof (piddesc_tables_all[0]); k++)
  {
    struct piddesc const * const table = piddesc_tables_all[k];
    for (uint32_t i = 0; table[i].pid != PID_SENTINEL; i++)
    {
      struct piddesc const * const entry = &table[i];
      if (!(entry->flags & PDF_QOS))
        break;
      if (check & entry->present_flag)
      {
        const size_t srcoff = entry->plist_offset - offsetof (nn_plist_t, qos);
        bool equal;
        /* Partition is special-cased because it is a set (with a special rules
           for empty sets and empty strings to boot), and normal string sequence
           comparison requires the ordering to be the same */
        if (entry->pid == PID_PARTITION)
          equal = partitions_equal (&x->partition, &y->partition);
        else if (!(entry->flags & PDF_FUNCTION))
          equal = equal_generic (x, y, srcoff, entry->op.desc);
        else
          equal = entry->op.f.equal (x, y, srcoff);
        if (!equal)
          delta |= entry->present_flag;
      }
    }
  }
  return delta;
}

static dds_return_t validate_external_duration (const ddsi_duration_t *d)
{
  /* Accepted are zero, positive, infinite or invalid as defined in
     the DDS 2.1 spec, table 9.4. */
  if (d->seconds >= 0)
    return 0;
  else if (d->seconds == -1 && d->fraction == UINT32_MAX)
    return 0;
  else
    return DDS_RETCODE_BAD_PARAMETER;
}

static int history_qospolicy_allzero (const dds_history_qospolicy_t *q)
{
  return q->kind == DDS_HISTORY_KEEP_LAST && q->depth == 0;
}

static dds_return_t validate_history_qospolicy (const dds_history_qospolicy_t *q)
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
      return DDS_RETCODE_BAD_PARAMETER;
  }
  /* Accept all values for depth if kind = ALL */
  if (q->kind == DDS_HISTORY_KEEP_LAST && q->depth < 1)
    return DDS_RETCODE_BAD_PARAMETER;
  return 0;
}

static int resource_limits_qospolicy_allzero (const dds_resource_limits_qospolicy_t *q)
{
  return q->max_samples == 0 && q->max_instances == 0 && q->max_samples_per_instance == 0;
}

static dds_return_t validate_resource_limits_qospolicy (const dds_resource_limits_qospolicy_t *q)
{
  /* Note: dependent on history setting as well (see
     validate_history_qospolicy). Verifying only the internal
     consistency of the resource limits. */
  if (q->max_samples < 1 && q->max_samples != DDS_LENGTH_UNLIMITED)
    return DDS_RETCODE_BAD_PARAMETER;
  if (q->max_instances < 1 && q->max_instances != DDS_LENGTH_UNLIMITED)
    return DDS_RETCODE_BAD_PARAMETER;
  if (q->max_samples_per_instance < 1 && q->max_samples_per_instance != DDS_LENGTH_UNLIMITED)
    return DDS_RETCODE_BAD_PARAMETER;
  if (q->max_samples != DDS_LENGTH_UNLIMITED && q->max_samples_per_instance != DDS_LENGTH_UNLIMITED)
  {
    /* Interpreting 7.1.3.19 as if "unlimited" is meant to mean "don't
       care" and any conditions related to it must be ignored. */
    if (q->max_samples < q->max_samples_per_instance)
      return DDS_RETCODE_INCONSISTENT_POLICY;
  }
  return 0;
}

static dds_return_t validate_history_and_resource_limits (const dds_history_qospolicy_t *qh, const dds_resource_limits_qospolicy_t *qr)
{
  dds_return_t res;
  if ((res = validate_history_qospolicy (qh)) < 0)
    return res;
  if ((res = validate_resource_limits_qospolicy (qr)) < 0)
    return res;
  switch (qh->kind)
  {
    case DDS_HISTORY_KEEP_ALL:
#if 0 /* See comment in validate_resource_limits, ref'ing 7.1.3.19 */
      if (qr->max_samples_per_instance != DDS_LENGTH_UNLIMITED)
        return DDS_RETCODE_BAD_PARAMETER;
#endif
      break;
    case DDS_HISTORY_KEEP_LAST:
      if (qr->max_samples_per_instance != DDS_LENGTH_UNLIMITED && qh->depth > qr->max_samples_per_instance)
        return DDS_RETCODE_INCONSISTENT_POLICY;
      break;
  }
  return 0;
}

static int durability_service_qospolicy_allzero (const dds_durability_service_qospolicy_t *q)
{
  return (history_qospolicy_allzero (&q->history) &&
          resource_limits_qospolicy_allzero (&q->resource_limits) &&
          q->service_cleanup_delay == 0);
}

static dds_return_t validate_durability_service_qospolicy_acceptzero (const dds_durability_service_qospolicy_t *q, bool acceptzero)
{
  dds_return_t res;
  if (acceptzero && durability_service_qospolicy_allzero (q))
    return 0;
  if (q->service_cleanup_delay < 0)
    return DDS_RETCODE_BAD_PARAMETER;
  if ((res = validate_history_and_resource_limits (&q->history, &q->resource_limits)) < 0)
    return res;
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

static dds_return_t do_locator (nn_locators_t *ls, uint64_t *present, uint64_t wanted, uint64_t fl, const struct dd *dd, const struct ddsi_tran_factory *factory)
{
  nn_locator_t loc;

  if (dd->bufsz < sizeof (loc))
    return DDS_RETCODE_BAD_PARAMETER;

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
        return DDS_RETCODE_BAD_PARAMETER;
      if (!locator_address_prefix12_zero (&loc))
        return DDS_RETCODE_BAD_PARAMETER;
      break;
    case NN_LOCATOR_KIND_UDPv6:
    case NN_LOCATOR_KIND_TCPv6:
      if (loc.port <= 0 || loc.port > 65535)
        return DDS_RETCODE_BAD_PARAMETER;
      break;
    case NN_LOCATOR_KIND_UDPv4MCGEN: {
      const nn_udpv4mcgen_address_t *x = (const nn_udpv4mcgen_address_t *) loc.address;
      if (!ddsi_factory_supports (factory, NN_LOCATOR_KIND_UDPv4))
        return 0;
      if (loc.port <= 0 || loc.port > 65536)
        return DDS_RETCODE_BAD_PARAMETER;
      if ((uint32_t) x->base + x->count >= 28 || x->count == 0 || x->idx >= x->count)
        return DDS_RETCODE_BAD_PARAMETER;
      break;
    }
    case NN_LOCATOR_KIND_INVALID:
      if (!locator_address_zero (&loc))
        return DDS_RETCODE_BAD_PARAMETER;
      if (loc.port != 0)
        return DDS_RETCODE_BAD_PARAMETER;
      /* silently dropped correctly formatted "invalid" locators. */
      return 0;
    case NN_LOCATOR_KIND_RESERVED:
      /* silently dropped "reserved" locators. */
      return 0;
    default:
      return 0;
  }
  return add_locator (ls, present, wanted, fl, &loc);
}

static void locator_from_ipv4address_port (nn_locator_t *loc, const nn_ipv4address_t *a, const nn_port_t *p, ddsi_tran_factory_t factory)
{
  loc->kind = factory->m_connless ? NN_LOCATOR_KIND_UDPv4 : NN_LOCATOR_KIND_TCPv4;
  loc->port = *p;
  memset (loc->address, 0, 12);
  memcpy (loc->address + 12, a, 4);
}

static dds_return_t do_ipv4address (nn_plist_t *dest, nn_ipaddress_params_tmp_t *dest_tmp, uint64_t wanted, uint32_t fl_tmp, const struct dd *dd, ddsi_tran_factory_t factory)
{
  nn_ipv4address_t *a;
  nn_port_t *p;
  nn_locators_t *ls;
  uint32_t fl1_tmp;
  uint64_t fldest;
  if (dd->bufsz < sizeof (*a))
    return DDS_RETCODE_BAD_PARAMETER;
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
    locator_from_ipv4address_port (&loc, a, p, factory);
    dest_tmp->present &= ~(fl_tmp | fl1_tmp);
    return add_locator (ls, &dest->present, wanted, fldest, &loc);
  }
  else
  {
    return 0;
  }
}

static dds_return_t do_port (nn_plist_t *dest, nn_ipaddress_params_tmp_t *dest_tmp, uint64_t wanted, uint32_t fl_tmp, const struct dd *dd, ddsi_tran_factory_t factory)
{
  nn_ipv4address_t *a;
  nn_port_t *p;
  nn_locators_t *ls;
  uint64_t fldest;
  uint32_t fl1_tmp;
  if (dd->bufsz < sizeof (*p))
    return DDS_RETCODE_BAD_PARAMETER;
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
    return DDS_RETCODE_BAD_PARAMETER;
  dest_tmp->present |= fl_tmp;
  if ((dest_tmp->present & (fl_tmp | fl1_tmp)) == (fl_tmp | fl1_tmp))
  {
    /* If port already known, add corresponding locator and discard
       both address & port from the set of present plist: this
       allows adding another pair. */
    nn_locator_t loc;
    locator_from_ipv4address_port (&loc, a, p, factory);
    dest_tmp->present &= ~(fl_tmp | fl1_tmp);
    return add_locator (ls, &dest->present, wanted, fldest, &loc);
  }
  else
  {
    return 0;
  }
}

static dds_return_t return_unrecognized_pid (nn_plist_t *plist, nn_parameterid_t pid)
{
  if (!(pid & PID_UNRECOGNIZED_INCOMPATIBLE_FLAG))
    return 0;
  else
  {
    plist->present |= PP_INCOMPATIBLE;
    return DDS_RETCODE_UNSUPPORTED;
  }
}

static dds_return_t init_one_parameter (nn_plist_t *plist, nn_ipaddress_params_tmp_t *dest_tmp, uint64_t pwanted, uint64_t qwanted, uint16_t pid, const struct dd *dd, ddsi_tran_factory_t factory, const ddsrt_log_cfg_t *logcfg)
{
  /* special-cased ipv4address and port, because they have state beyond that what gets
     passed into the generic code */
  switch (pid)
  {
#define XA(NAME_) case PID_##NAME_##_IPADDRESS: return do_ipv4address (plist, dest_tmp, pwanted, PPTMP_##NAME_##_IPADDRESS, dd, factory)
#define XP(NAME_) case PID_##NAME_##_PORT: return do_port (plist, dest_tmp, pwanted, PPTMP_##NAME_##_PORT, dd, factory)
    XA (MULTICAST);
    XA (DEFAULT_UNICAST);
    XP (DEFAULT_UNICAST);
    XA (METATRAFFIC_UNICAST);
    XP (METATRAFFIC_UNICAST);
    XA (METATRAFFIC_MULTICAST);
    XP (METATRAFFIC_MULTICAST);
#undef XP
#undef XA
  }

  const struct piddesc_index *index;
  if (!(pid & PID_VENDORSPECIFIC_FLAG))
    index = &piddesc_vendor_index[0];
  else if (dd->vendorid.id[0] != 1 || dd->vendorid.id[1] < 1)
    return return_unrecognized_pid (plist, pid);
  else if (dd->vendorid.id[1] >= sizeof (piddesc_vendor_index) / sizeof (piddesc_vendor_index[0]))
    return return_unrecognized_pid (plist, pid);
  else if (piddesc_vendor_index[dd->vendorid.id[1]].index == NULL)
  return return_unrecognized_pid (plist, pid);
  else
    index = &piddesc_vendor_index[dd->vendorid.id[1]];

  const struct piddesc *entry;
  if (pid_without_flags (pid) > index->index_max || (entry = index->index[pid_without_flags (pid)]) == NULL)
    return return_unrecognized_pid (plist, pid);
  assert (pid_without_flags (pid) == pid_without_flags (entry->pid));
  if (pid != entry->pid)
  {
    DDS_CERROR (logcfg, "error processing parameter list (vendor %u.%u, version %u.%u): pid %"PRIx16" mapped to pid %"PRIx16"\n",
                dd->vendorid.id[0], dd->vendorid.id[1],
                dd->protocol_version.major, dd->protocol_version.minor,
                pid, entry->pid);
    return return_unrecognized_pid (plist, pid);
  }
  assert (pid != PID_PAD);

  struct flagset flagset;
  if (entry->flags & PDF_QOS)
  {
    flagset.present = &plist->qos.present;
    flagset.aliased = &plist->qos.aliased;
    flagset.wanted = qwanted;
  }
  else
  {
    flagset.present = &plist->present;
    flagset.aliased = &plist->aliased;
    flagset.wanted = pwanted;
  }

  /* Disallow multiple copies of the same parameter unless explicit allowed
     (which is needed for handling locators).  String sequences will leak
     memory if deserialized repeatedly */
  if ((*flagset.present & entry->present_flag) && !(entry->flags & PDF_ALLOWMULTI))
  {
    DDS_CWARNING (logcfg, "invalid parameter list (vendor %u.%u, version %u.%u): pid %"PRIx16" (%s) multiply defined\n",
                  dd->vendorid.id[0], dd->vendorid.id[1],
                  dd->protocol_version.major, dd->protocol_version.minor,
                  pid, entry->name);
    return DDS_RETCODE_BAD_PARAMETER;
  }
  if (!(flagset.wanted & entry->present_flag))
  {
    /* skip don't cares -- the point of skipping them is performance and
       avoiding unnecessary allocations, so validating them would be silly */
    return 0;
  }

  /* String sequences are not allowed in parameters that may occur multiple
     times because they will leak the arrays of pointers.  Fixing this is
     not worth the bother as long as such parameters don't exist. */
  dds_return_t ret;
  void * const dst = (char *) plist + entry->plist_offset;
  size_t dstoff = 0;
  size_t srcoff = 0;
  if (entry->flags & PDF_FUNCTION)
    ret = entry->op.f.deser (dst, &dstoff, &flagset, entry->present_flag, dd, &srcoff);
  else
    ret = deser_generic (dst, &dstoff, &flagset, entry->present_flag, dd, &srcoff, entry->op.desc);
  if (ret == 0 && entry->deser_validate_xform)
    ret = entry->deser_validate_xform (dst, dd);
  if (ret < 0)
  {
    DDS_CWARNING (logcfg, "invalid parameter list (vendor %u.%u, version %u.%u): pid %"PRIx16" (%s) invalid, input = ",
                  dd->vendorid.id[0], dd->vendorid.id[1],
                  dd->protocol_version.major, dd->protocol_version.minor,
                  pid, entry->name);
    log_octetseq (DDS_LC_WARNING, logcfg, (uint32_t) dd->bufsz, dd->buf);
    DDS_CWARNING (logcfg, "\n");
  }
  return ret;
}

void nn_plist_mergein_missing (nn_plist_t *a, const nn_plist_t *b, uint64_t pmask, uint64_t qmask)
{
  plist_or_xqos_mergein_missing (a, b, 0, pmask, qmask);
}

void nn_xqos_mergein_missing (dds_qos_t *a, const dds_qos_t *b, uint64_t mask)
{
  plist_or_xqos_mergein_missing (a, b, offsetof (nn_plist_t, qos), 0, mask);
}

void nn_plist_copy (nn_plist_t *dst, const nn_plist_t *src)
{
  nn_plist_init_empty (dst);
  nn_plist_mergein_missing (dst, src, ~(uint64_t)0, ~(uint64_t)0);
}

nn_plist_t *nn_plist_dup (const nn_plist_t *src)
{
  nn_plist_t *dst;
  dst = ddsrt_malloc (sizeof (*dst));
  nn_plist_copy (dst, src);
  assert (dst->aliased == 0);
  return dst;
}

void nn_plist_init_empty (nn_plist_t *dest)
{
#ifndef NDEBUG
  memset (dest, 0, sizeof (*dest));
#endif
  dest->present = dest->aliased = 0;
  nn_xqos_init_empty (&dest->qos);
}

static dds_return_t final_validation_qos (const dds_qos_t *dest, nn_protocol_version_t protocol_version, nn_vendorid_t vendorid, bool *dursvc_accepted_allzero, bool strict)
{
  /* input is const, but we need to validate the combination of
     history & resource limits: so use a copy of those two policies */
  dds_history_qospolicy_t tmphist = {
    .kind = DDS_HISTORY_KEEP_LAST,
    .depth = 1
  };
  dds_resource_limits_qospolicy_t tmpreslim = {
    .max_samples = DDS_LENGTH_UNLIMITED,
    .max_instances = DDS_LENGTH_UNLIMITED,
    .max_samples_per_instance = DDS_LENGTH_UNLIMITED
  };
  dds_return_t res;

  /* Resource limits & history are related, so if only one is given,
     set the other to the default, claim it has been provided &
     validate the combination. They can't be changed afterward, so
     this is a reasonable interpretation. */
  if (dest->present & QP_HISTORY)
    tmphist = dest->history;
  if (dest->present & QP_RESOURCE_LIMITS)
    tmpreslim = dest->resource_limits;
  if ((res = validate_history_and_resource_limits (&tmphist, &tmpreslim)) < 0)
    return res;

  if ((dest->present & QP_DEADLINE) && (dest->present & QP_TIME_BASED_FILTER))
  {
    if (dest->deadline.deadline < dest->time_based_filter.minimum_separation)
      return DDS_RETCODE_INCONSISTENT_POLICY;
  }

  /* Durability service is sort-of accepted if all zeros, but only
     for some protocol versions and vendors.  We don't handle want
     to deal with that case internally. Now that all QoS have been
     parsed we know the setting of the durability QoS (the default
     is always VOLATILE), and hence we can verify that the setting
     is valid or delete it if irrelevant. */
  if (dursvc_accepted_allzero)
    *dursvc_accepted_allzero = false;
  if (dest->present & QP_DURABILITY_SERVICE)
  {
    const dds_durability_kind_t durkind = (dest->present & QP_DURABILITY) ? dest->durability.kind : DDS_DURABILITY_VOLATILE;
    bool acceptzero;
    bool check_dursvc = true;
    /* Use a somewhat convoluted rule to decide whether or not to
       "accept" an all-zero durability service setting, to find a
       reasonable mix of strictness and compatibility */
    if (dursvc_accepted_allzero == NULL)
      acceptzero = false;
    else if (protocol_version_is_newer (protocol_version))
      acceptzero = true;
    else if (strict)
      acceptzero = vendor_is_twinoaks (vendorid);
    else
      acceptzero = !vendor_is_eclipse (vendorid);
    switch (durkind)
    {
      case DDS_DURABILITY_VOLATILE:
      case DDS_DURABILITY_TRANSIENT_LOCAL:
        /* let caller now if we accepted all-zero: our input is const and we can't patch it out */
        if (acceptzero && durability_service_qospolicy_allzero (&dest->durability_service) && dursvc_accepted_allzero)
        {
          *dursvc_accepted_allzero = true;
          check_dursvc = false;
        }
        break;
      case DDS_DURABILITY_TRANSIENT:
      case DDS_DURABILITY_PERSISTENT:
        break;
    }
    if (check_dursvc && (res = validate_durability_service_qospolicy_acceptzero (&dest->durability_service, false)) < 0)
      return res;
  }
  return 0;
}

static dds_return_t final_validation (nn_plist_t *dest, nn_protocol_version_t protocol_version, nn_vendorid_t vendorid, bool *dursvc_accepted_allzero, bool strict)
{
  return final_validation_qos (&dest->qos, protocol_version, vendorid, dursvc_accepted_allzero, strict);
}

dds_return_t nn_plist_init_frommsg (nn_plist_t *dest, char **nextafterplist, uint64_t pwanted, uint64_t qwanted, const nn_plist_src_t *src)
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
  dd.factory = src->factory;
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
      DDS_CWARNING (src->logconfig, "plist(vendor %u.%u): unknown encoding (%d)\n",
                    src->vendorid.id[0], src->vendorid.id[1], src->encoding);
      return DDS_RETCODE_BAD_PARAMETER;
  }
  nn_plist_init_empty (dest);
  dest_tmp.present = 0;

  DDS_CLOG (DDS_LC_PLIST, src->logconfig, "NN_PLIST_INIT (bswap %d)\n", dd.bswap);

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
      bool dursvc_accepted_allzero;
      DDS_CLOG (DDS_LC_PLIST, src->logconfig, "%4"PRIx32" PID %"PRIx16"\n", (uint32_t) (pl - src->buf), pid);
      if ((res = final_validation (dest, src->protocol_version, src->vendorid, &dursvc_accepted_allzero, src->strict)) < 0)
      {
        nn_plist_fini (dest);
        return res;
      }
      else
      {
        /* If we accepted an all-zero durability service, that's awfully friendly of ours,
           but we'll pretend we never saw it */
        if (dursvc_accepted_allzero)
          dest->qos.present &= ~QP_DURABILITY_SERVICE;
        pl += sizeof (*par);
        if (nextafterplist)
          *nextafterplist = (char *) pl;
        return 0;
      }
    }
    if (length > src->bufsz - sizeof (*par) - (uint32_t) (pl - src->buf))
    {
      DDS_CWARNING (src->logconfig, "plist(vendor %u.%u): parameter length %"PRIu16" out of bounds\n",
                    src->vendorid.id[0], src->vendorid.id[1], length);
      nn_plist_fini (dest);
      return DDS_RETCODE_BAD_PARAMETER;
    }
    if ((length % 4) != 0) /* DDSI 9.4.2.11 */
    {
      DDS_CWARNING (src->logconfig, "plist(vendor %u.%u): parameter length %"PRIu16" mod 4 != 0\n",
                    src->vendorid.id[0], src->vendorid.id[1], length);
      nn_plist_fini (dest);
      return DDS_RETCODE_BAD_PARAMETER;
    }

    if (src->logconfig->c.mask & DDS_LC_PLIST)
    {
      DDS_CLOG (DDS_LC_PLIST, src->logconfig, "%4"PRIx32" PID %"PRIx16" len %"PRIu16" ", (uint32_t) (pl - src->buf), pid, length);
      log_octetseq (DDS_LC_PLIST, src->logconfig, length, (const unsigned char *) (par + 1));
      DDS_CLOG (DDS_LC_PLIST, src->logconfig, "\n");
    }

    dd.buf = (const unsigned char *) (par + 1);
    dd.bufsz = length;
    if ((res = init_one_parameter (dest, &dest_tmp, pwanted, qwanted, pid, &dd, src->factory, src->logconfig)) < 0)
    {
      /* make sure we print a trace message on error */
      DDS_CTRACE (src->logconfig, "plist(vendor %u.%u): failed at pid=%"PRIx16"\n", src->vendorid.id[0], src->vendorid.id[1], pid);
      nn_plist_fini (dest);
      return res;
    }
    pl += sizeof (*par) + length;
  }
  /* If we get here, that means we reached the end of the message
     without encountering a sentinel. That is an error */
  DDS_CWARNING (src->logconfig, "plist(vendor %u.%u): invalid parameter list: sentinel missing\n",
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
      DDS_CWARNING (src->logconfig, "plist(vendor %u.%u): quickscan: unknown encoding (%d)\n",
                    src->vendorid.id[0], src->vendorid.id[1], src->encoding);
      return NULL;
  }
  DDS_CLOG (DDS_LC_PLIST, src->logconfig, "NN_PLIST_QUICKSCAN (bswap %d)\n", dest->bswap);
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
      DDS_CWARNING (src->logconfig, "plist(vendor %u.%u): quickscan: parameter length %"PRIu16" out of bounds\n",
                    src->vendorid.id[0], src->vendorid.id[1], length);
      return NULL;
    }
    if ((length % 4) != 0) /* DDSI 9.4.2.11 */
    {
      DDS_CWARNING (src->logconfig, "plist(vendor %u.%u): quickscan: parameter length %"PRIu16" mod 4 != 0\n",
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
          DDS_CTRACE (src->logconfig, "plist(vendor %u.%u): quickscan(PID_STATUSINFO): buffer too small\n",
                      src->vendorid.id[0], src->vendorid.id[1]);
          return NULL;
        }
        else
        {
          /* can only represent 2 LSBs of statusinfo in "dest", so if others are set,
             mark it as a "complex_qos" and accept the hit of parsing the data completely. */
          uint32_t stinfo = fromBE4u (*((uint32_t *) pl));
          dest->statusinfo = stinfo & 3u;
          if ((stinfo & ~3u))
            dest->complex_qos = 1;
        }
        break;
      case PID_KEYHASH:
        break;
      default:
        DDS_CLOG (DDS_LC_PLIST, src->logconfig, "(pid=%"PRIx16" complex_qos=1)", pid);
        dest->complex_qos = 1;
        break;
    }
    pl += length;
  }
  /* If we get here, that means we reached the end of the message
     without encountering a sentinel. That is an error */
  DDS_CWARNING (src->logconfig, "plist(vendor %u.%u): quickscan: invalid parameter list: sentinel missing\n",
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

  plist->qos.present |= QP_USER_DATA;
  plist->qos.user_data.length = 0;
  plist->qos.user_data.value = NULL;
}

static void xqos_init_default_common (dds_qos_t *xqos)
{
  nn_xqos_init_empty (xqos);

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

static void nn_xqos_init_default_endpoint (dds_qos_t *xqos)
{
  xqos_init_default_common (xqos);

  xqos->present |= QP_TOPIC_DATA;
  xqos->topic_data.length = 0;
  xqos->topic_data.value = NULL;

  xqos->present |= QP_GROUP_DATA;
  xqos->group_data.length = 0;
  xqos->group_data.value = NULL;

  xqos->present |= QP_USER_DATA;
  xqos->user_data.length = 0;
  xqos->user_data.value = NULL;

  xqos->present |= QP_PARTITION;
  xqos->partition.n = 0;
  xqos->partition.strs = NULL;
}

void nn_xqos_init_default_reader (dds_qos_t *xqos)
{
  nn_xqos_init_default_endpoint (xqos);

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
  nn_xqos_init_default_endpoint (xqos);

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

static void nn_xqos_init_default_publisher_subscriber (dds_qos_t *xqos)
{
  nn_xqos_init_empty (xqos);

  xqos->present |= QP_GROUP_DATA;
  xqos->group_data.length = 0;
  xqos->group_data.value = NULL;

  xqos->present |= QP_PRISMTECH_ENTITY_FACTORY;
  xqos->entity_factory.autoenable_created_entities = 1;

  xqos->present |= QP_PARTITION;
  xqos->partition.n = 0;
  xqos->partition.strs = NULL;
}

void nn_xqos_init_default_subscriber (dds_qos_t *xqos)
{
  nn_xqos_init_default_publisher_subscriber (xqos);
}

void nn_xqos_init_default_publisher (dds_qos_t *xqos)
{
  nn_xqos_init_default_publisher_subscriber (xqos);
}

void nn_xqos_copy (dds_qos_t *dst, const dds_qos_t *src)
{
  nn_xqos_init_empty (dst);
  nn_xqos_mergein_missing (dst, src, ~(uint64_t)0);
}

void nn_xqos_fini (dds_qos_t *xqos)
{
  plist_or_xqos_fini (xqos, offsetof (nn_plist_t, qos), ~(uint64_t)0, ~(uint64_t)0);
}

void nn_xqos_fini_mask (dds_qos_t *xqos, uint64_t mask)
{
  plist_or_xqos_fini (xqos, offsetof (nn_plist_t, qos), ~(uint64_t)0, mask);
}

void nn_xqos_unalias (dds_qos_t *xqos)
{
  plist_or_xqos_unalias (xqos, offsetof (nn_plist_t, qos));
}

dds_qos_t * nn_xqos_dup (const dds_qos_t *src)
{
  dds_qos_t *dst = ddsrt_malloc (sizeof (*dst));
  nn_xqos_copy (dst, src);
  assert (dst->aliased == 0);
  return dst;
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

static int strcmp_wrapper (const void *va, const void *vb)
{
  char const * const *a = va;
  char const * const *b = vb;
  return strcmp (*a, *b);
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
    if (bsearch (&b->strs[i], tab, a->n, sizeof (*tab), strcmp_wrapper) == NULL)
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

/*************************/

void nn_xqos_addtomsg (struct nn_xmsg *m, const dds_qos_t *xqos, uint64_t wanted)
{
  plist_or_xqos_addtomsg (m, xqos, offsetof (struct nn_plist, qos), 0, wanted);
}

void nn_plist_addtomsg (struct nn_xmsg *m, const nn_plist_t *ps, uint64_t pwanted, uint64_t qwanted)
{
  plist_or_xqos_addtomsg (m, ps, 0, pwanted, qwanted);
}

/*************************/

static uint32_t isprint_runlen (uint32_t n, const unsigned char *xs)
{
  uint32_t m;
  for (m = 0; m < n && xs[m] != '"' && isprint (xs[m]) && xs[m] < 127; m++)
    ;
  return m;
}


static void log_octetseq (uint32_t cat, const struct ddsrt_log_cfg *logcfg, uint32_t n, const unsigned char *xs)
{
  uint32_t i = 0;
  while (i < n)
  {
    uint32_t m = isprint_runlen (n - i, xs);
    if (m >= 4 || (i == 0 && m == n))
    {
      DDS_CLOG (cat, logcfg, "%s\"%*.*s\"", i == 0 ? "" : ",", m, m, xs);
      xs += m;
      i += m;
    }
    else
    {
      if (m == 0)
        m = 1;
      while (m--)
      {
        DDS_CLOG (cat, logcfg, "%s%u", i == 0 ? "" : ",", *xs++);
        i++;
      }
    }
  }
}

void nn_log_xqos (uint32_t cat, const struct ddsrt_log_cfg *logcfg, const dds_qos_t *xqos)
{
  uint64_t p = xqos->present;
  const char *prefix = "";
#define LOGB0(fmt_) DDS_CLOG (cat, logcfg, "%s" fmt_, prefix)
#define LOGB1(fmt_, ...) DDS_CLOG (cat, logcfg, "%s" fmt_, prefix, __VA_ARGS__)
#define DO(name_, body_) do { if (p & QP_##name_) { { body_ } prefix = ","; } } while (0)

#define FMT_DUR "%"PRId64".%09"PRId32
#define PRINTARG_DUR(d) ((int64_t) ((d) / 1000000000)), ((int32_t) ((d) % 1000000000))

  DO (TOPIC_NAME, { LOGB1 ("topic=%s", xqos->topic_name); });
  DO (TYPE_NAME, { LOGB1 ("type=%s", xqos->type_name); });
  DO (PRESENTATION, { LOGB1 ("presentation=%d:%u:%u", xqos->presentation.access_scope, xqos->presentation.coherent_access, xqos->presentation.ordered_access); });
  DO (PARTITION, {
      LOGB0 ("partition={");
      for (uint32_t i = 0; i < xqos->partition.n; i++) {
        DDS_CLOG (cat, logcfg, "%s%s", (i == 0) ? "" : ",", xqos->partition.strs[i]);
      }
      DDS_CLOG (cat, logcfg, "}");
    });
  DO (GROUP_DATA, {
    LOGB1 ("group_data=%"PRIu32"<", xqos->group_data.length);
    log_octetseq (cat, logcfg, xqos->group_data.length, xqos->group_data.value);
    DDS_CLOG (cat, logcfg, ">");
  });
  DO (TOPIC_DATA, {
    LOGB1 ("topic_data=%"PRIu32"<", xqos->topic_data.length);
    log_octetseq (cat, logcfg, xqos->topic_data.length, xqos->topic_data.value);
    DDS_CLOG(cat, logcfg, ">");
  });
  DO (DURABILITY, { LOGB1 ("durability=%d", xqos->durability.kind); });
  DO (DURABILITY_SERVICE, {
      LOGB0 ("durability_service=");
      DDS_CLOG(cat, logcfg, FMT_DUR, PRINTARG_DUR (xqos->durability_service.service_cleanup_delay));
      DDS_CLOG(cat, logcfg, ":{%u:%"PRId32"}", xqos->durability_service.history.kind, xqos->durability_service.history.depth);
      DDS_CLOG(cat, logcfg, ":{%"PRId32":%"PRId32":%"PRId32"}", xqos->durability_service.resource_limits.max_samples, xqos->durability_service.resource_limits.max_instances, xqos->durability_service.resource_limits.max_samples_per_instance);
    });
  DO (DEADLINE, { LOGB1 ("deadline="FMT_DUR, PRINTARG_DUR (xqos->deadline.deadline)); });
  DO (LATENCY_BUDGET, { LOGB1 ("latency_budget="FMT_DUR, PRINTARG_DUR (xqos->latency_budget.duration)); });
  DO (LIVELINESS, { LOGB1 ("liveliness=%d:"FMT_DUR, xqos->liveliness.kind, PRINTARG_DUR (xqos->liveliness.lease_duration)); });
  DO (RELIABILITY, { LOGB1 ("reliability=%d:"FMT_DUR, xqos->reliability.kind, PRINTARG_DUR (xqos->reliability.max_blocking_time)); });
  DO (DESTINATION_ORDER, { LOGB1 ("destination_order=%d", xqos->destination_order.kind); });
  DO (HISTORY, { LOGB1 ("history=%d:%"PRId32, xqos->history.kind, xqos->history.depth); });
  DO (RESOURCE_LIMITS, { LOGB1 ("resource_limits=%"PRId32":%"PRId32":%"PRId32, xqos->resource_limits.max_samples, xqos->resource_limits.max_instances, xqos->resource_limits.max_samples_per_instance); });
  DO (TRANSPORT_PRIORITY, { LOGB1 ("transport_priority=%"PRId32, xqos->transport_priority.value); });
  DO (LIFESPAN, { LOGB1 ("lifespan="FMT_DUR, PRINTARG_DUR (xqos->lifespan.duration)); });
  DO (USER_DATA, {
    LOGB1 ("user_data=%"PRIu32"<", xqos->user_data.length);
    log_octetseq (cat, logcfg, xqos->user_data.length, xqos->user_data.value);
    DDS_CLOG (cat, logcfg, ">");
  });
  DO (OWNERSHIP, { LOGB1 ("ownership=%d", xqos->ownership.kind); });
  DO (OWNERSHIP_STRENGTH, { LOGB1 ("ownership_strength=%"PRId32, xqos->ownership_strength.value); });
  DO (TIME_BASED_FILTER, { LOGB1 ("time_based_filter="FMT_DUR, PRINTARG_DUR (xqos->time_based_filter.minimum_separation)); });
  DO (PRISMTECH_READER_DATA_LIFECYCLE, { LOGB1 ("reader_data_lifecycle="FMT_DUR":"FMT_DUR, PRINTARG_DUR (xqos->reader_data_lifecycle.autopurge_nowriter_samples_delay), PRINTARG_DUR (xqos->reader_data_lifecycle.autopurge_disposed_samples_delay)); });
  DO (PRISMTECH_WRITER_DATA_LIFECYCLE, {
    LOGB1 ("writer_data_lifecycle={%u}", xqos->writer_data_lifecycle.autodispose_unregistered_instances); });
  DO (PRISMTECH_READER_LIFESPAN, { LOGB1 ("reader_lifespan={%u,"FMT_DUR"}", xqos->reader_lifespan.use_lifespan, PRINTARG_DUR (xqos->reader_lifespan.duration)); });
  DO (PRISMTECH_SUBSCRIPTION_KEYS, {
    LOGB1 ("subscription_keys={%u,{", xqos->subscription_keys.use_key_list);
    for (uint32_t i = 0; i < xqos->subscription_keys.key_list.n; i++) {
      DDS_CLOG (cat, logcfg, "%s%s", (i == 0) ? "" : ",", xqos->subscription_keys.key_list.strs[i]);
    }
    DDS_CLOG (cat, logcfg, "}}");
  });
  DO (PRISMTECH_ENTITY_FACTORY, { LOGB1 ("entity_factory=%u", xqos->entity_factory.autoenable_created_entities); });
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

// Copyright(c) 2006 to 2022 ZettaScale Technology and others
//
// This program and the accompanying materials are made available under the
// terms of the Eclipse Public License v. 2.0 which is available at
// http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
// v. 1.0 which is available at
// http://www.eclipse.org/org/documents/edl-v10.php.
//
// SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <ctype.h>

#include "dds/features.h"
#include "dds/ddsrt/align.h"
#include "dds/ddsrt/log.h"
#include "dds/ddsrt/heap.h"
#include "dds/ddsrt/string.h"
#include "dds/ddsrt/static_assert.h"
#include "dds/ddsrt/avl.h"
#include "dds/ddsi/ddsi_log.h"
#include "dds/ddsi/ddsi_unused.h"
#include "dds/ddsi/ddsi_domaingv.h"
#include "dds/ddsi/ddsi_typelib.h"
#include "ddsi__plist.h"
#include "ddsi__time.h"
#include "ddsi__xmsg.h"
#include "ddsi__vendor.h"
#include "ddsi__udp.h" /* ddsi_mc4gen_address_t */
#include "ddsi__protocol.h"
#include "ddsi__radmin.h" /* for ddsi_plist_quickscan */
#include "ddsi__plist_generic.h"
#include "ddsi__security_omg.h"
#include "ddsi__tran.h"
#include "ddsi__xqos.h"
#include "dds/cdr/dds_cdrstream.h"

/* I am tempted to change LENGTH_UNLIMITED to 0 in the API (with -1
   supported for backwards compatibility) ... on the wire however
   it must be -1 */
DDSRT_STATIC_ASSERT(DDS_LENGTH_UNLIMITED == -1);

/* It turns out there are some platforms where the alignment of a
   field m in the following:

     struct {
       ...;
       T m;
     }

   may be different from _Alignof(T) ...  So far I have only seen
   this on some embedded platform, and I furthermore suppose one
   should expect this to happen if one were to use "#pragma pack"
   or any of the analogous things, but that is not something we
   have in our code.

   Still, it means some of the parameter list code fails in nasty
   ways.  From the current set of observations, the problem
   disappears if we use this alternative form. */
#define plist_alignof(ty_) (offsetof(struct {char c_; ty_ t;}, t))

/* These are internal to the parameter list processing. We never
   generate them, and we never want to do see them anywhere outside
   the actual parsing of an incoming parameter list. (There are
   entries in ddsi_plist, but they are never to be inspected and
   the bits corresponding to them must be 0 except while processing an
   incoming parameter list.) */
#define PPTMP_MULTICAST_IPADDRESS               (1 << 0)
#define PPTMP_DEFAULT_UNICAST_IPADDRESS         (1 << 1)
#define PPTMP_DEFAULT_UNICAST_PORT              (1 << 2)
#define PPTMP_METATRAFFIC_UNICAST_IPADDRESS     (1 << 3)
#define PPTMP_METATRAFFIC_UNICAST_PORT          (1 << 4)
#define PPTMP_METATRAFFIC_MULTICAST_IPADDRESS   (1 << 5)
#define PPTMP_METATRAFFIC_MULTICAST_PORT        (1 << 6)

typedef struct ddsi_ipaddress_params_tmp {
  uint32_t present;

  ddsi_ipv4address_t multicast_ipaddress;
  ddsi_ipv4address_t default_unicast_ipaddress;
  ddsi_port_t default_unicast_port;
  ddsi_ipv4address_t metatraffic_unicast_ipaddress;
  ddsi_port_t metatraffic_unicast_port;
  ddsi_ipv4address_t metatraffic_multicast_ipaddress;
  ddsi_port_t metatraffic_multicast_port;
} ddsi_ipaddress_params_tmp_t;

struct dd {
  const unsigned char *buf;
  size_t bufsz;
  unsigned bswap: 1;
  ddsi_protocol_version_t protocol_version;
  ddsi_vendorid_t vendorid;
  enum ddsi_plist_context_kind context_kind;
};

#define PDF_QOS        1 /* part of dds_qos_t */
#define PDF_FUNCTION   2 /* use special functions */
#define PDF_ALLOWMULTI 4 /* allow multiple copies -- do not use with Z or memory will leak */

struct flagset {
  uint64_t *present;
  uint64_t *aliased;
  uint64_t wanted;
};

struct piddesc {
  ddsi_parameterid_t pid;  /* parameter id or DDSI_PID_PAD if strictly local */
  uint16_t flags;        /* see PDF_xxx flags */
  uint64_t present_flag; /* flag in plist.present / plist.qos.present */
  const char *name;      /* name for reporting invalid input */
  size_t plist_offset;   /* offset from start of ddsi_plist_t */
  size_t size;           /* in-memory size for copying */
  union {
    /* descriptor for generic code: 12 is enough for the current set of
       parameters, compiler will warn if one ever tries to use more than
       will fit */
    const enum ddsi_pserop desc[12];
    struct {
      dds_return_t (*deser) (void * __restrict dst, struct flagset *flagset, uint64_t flag, const struct dd * __restrict dd, struct ddsi_domaingv const * const gv);
      dds_return_t (*ser) (struct ddsi_xmsg *xmsg, ddsi_parameterid_t pid, const void *src, size_t srcoff, enum ddsrt_byte_order_selector bo, enum ddsi_plist_context_kind context_kind);
      dds_return_t (*unalias) (void * __restrict dst, size_t * __restrict dstoff, bool gen_seq_aliased);
      dds_return_t (*fini) (void * __restrict dst, size_t * __restrict dstoff, struct flagset *flagset, uint64_t flag);
      dds_return_t (*valid) (const void *src, size_t srcoff);
      bool (*equal) (const void *srcx, const void *srcy, size_t srcoff);
      bool (*print) (char * __restrict *buf, size_t * __restrict bufsize, const void *src, size_t srcoff);
    } f;
  } op;
  dds_return_t (*deser_validate_xform) (void * __restrict dst, const struct dd * __restrict dd);
};

enum do_locator_result {
  DOLOC_ACCEPTED,
  DOLOC_IGNORED,
  DOLOC_INVALID
};

static dds_return_t validate_history_qospolicy (const dds_history_qospolicy_t *q);
static dds_return_t validate_resource_limits_qospolicy (const dds_resource_limits_qospolicy_t *q);
static dds_return_t validate_history_and_resource_limits (const dds_history_qospolicy_t *qh, const dds_resource_limits_qospolicy_t *qr);
static dds_return_t validate_external_duration (const ddsi_duration_t *d);
static dds_return_t validate_durability_service_qospolicy_acceptzero (const dds_durability_service_qospolicy_t *q, bool acceptzero);
static enum do_locator_result do_locator (ddsi_locators_t *ls, uint64_t present, uint64_t wanted, uint64_t fl, const struct dd *dd, struct ddsi_domaingv const * const gv);
static dds_return_t final_validation_qos (const dds_qos_t *dest, ddsi_protocol_version_t protocol_version, ddsi_vendorid_t vendorid, bool *dursvc_accepted_allzero, bool strict);
static int partitions_equal (const void *srca, const void *srcb, size_t off);
static dds_return_t ddsi_xqos_valid_strictness (const struct ddsrt_log_cfg *logcfg, const dds_qos_t *xqos, bool strict);
static dds_return_t unalias_generic (void * __restrict dst, size_t * __restrict dstoff, bool gen_seq_aliased, const enum ddsi_pserop * __restrict desc);
static dds_return_t deser_generic (void * __restrict dst, struct flagset *flagset, uint64_t flag, const struct dd * __restrict dd, const enum ddsi_pserop * __restrict desc);
static dds_return_t ser_generic (struct ddsi_xmsg *xmsg, ddsi_parameterid_t pid, const void *src, size_t srcoff, const enum ddsi_pserop * __restrict desc, enum ddsrt_byte_order_selector bo);
static bool equal_generic (const void *srcx, const void *srcy, size_t srcoff, const enum ddsi_pserop * __restrict desc);
static dds_return_t fini_generic (void * __restrict dst, size_t * __restrict dstoff, struct flagset *flagset, uint64_t flag, const enum ddsi_pserop * __restrict desc);
static dds_return_t valid_generic (const void *src, size_t srcoff, const enum ddsi_pserop * __restrict desc);
static bool print_generic (char * __restrict *buf, size_t * __restrict bufsize, const void *src, size_t srcoff, const enum ddsi_pserop * __restrict desc);

static size_t pserop_seralign(enum ddsi_pserop op)
{
  switch(op)
  {
    case XSTOP:
    case XbPROP:
    case Xopt:
      /* NB: XbPROP is never serialized, so its alignment is irrelevant.  If ever there
         is a need to allow calling this function when op = XbPROP, it needs to be changed
         to taking the address of the pserop, and in that case inspect the following
         operator */
      assert(false);
      return 1;
    case Xo: case Xox2:
    case Xb: case Xbx2: case Xbx3: case Xbx4: case Xbx5:
    case XbCOND:
    case XG:
    case XK:
      return 1;
    case Xs:
      return 2;
    case XO:
    case XS:
    case XE1: case XE2: case XE3:
    case Xi: case Xix2: case Xix3: case Xix4:
    case Xu: case Xux2: case Xux3: case Xux4: case Xux5:
    case XD: case XDx2:
    case XQ:
      return 4;
    case Xl:
      return 8;
  }
  assert(false);
  return 1;
}

static size_t alignN(const size_t off, const size_t align)
{
  return (off + align - 1) & ~(align - 1);
}

static size_t align2(const size_t off)
{
  return alignN(off, 2);
}

static size_t align4(const size_t off)
{
  return alignN(off, 4);
}

static size_t align8(const size_t off)
{
  return alignN(off, 8);
}

static void *deser_generic_dst (void * __restrict dst, size_t *dstoff, const size_t align)
{
  *dstoff = alignN(*dstoff, align);
  return (char *) dst + *dstoff;
}

static const void *deser_generic_src (const void * __restrict src, size_t *srcoff, const size_t align)
{
  *srcoff = alignN(*srcoff, align);
  return (const char *) src + *srcoff;
}

static void *ser_generic_aligned (char * __restrict p, size_t * __restrict off, const size_t align)
{
  const size_t off1 = alignN(*off, align);
  size_t pad = off1 - *off;
  char *dst = p + *off;
  *off = off1;
  while (pad--)
    *dst++ = 0;
  return dst;
}

static void *ser_generic_align2(char * __restrict p, size_t * __restrict off)
{
  return ser_generic_aligned(p, off, 2);
}

static void *ser_generic_align4(char * __restrict p, size_t * __restrict off)
{
  return ser_generic_aligned(p, off, 4);
}

static void *ser_generic_align8(char * __restrict p, size_t * __restrict off)
{
  return ser_generic_aligned(p, off, 8);
}

static dds_return_t deser_uint16 (uint16_t *dst, const struct dd * __restrict dd, size_t * __restrict off)
{
  size_t off1 = (*off + 1) & ~(size_t)1;
  uint16_t tmp;
  if (off1 + 2 > dd->bufsz)
    return DDS_RETCODE_BAD_PARAMETER;
  tmp = *((uint16_t *) (dd->buf + off1));
  if (dd->bswap)
    tmp = ddsrt_bswap2u (tmp);
  *dst = tmp;
  *off = off1 + 2;
  return 0;
}

static dds_return_t deser_uint32 (uint32_t *dst, const struct dd * __restrict dd, size_t * __restrict off)
{
  size_t off1 = (*off + 3) & ~(size_t)3;
  uint32_t tmp;
  if (off1 + 4 > dd->bufsz)
    return DDS_RETCODE_BAD_PARAMETER;
  tmp = *((uint32_t *) (dd->buf + off1));
  if (dd->bswap)
    tmp = ddsrt_bswap4u (tmp);
  *dst = tmp;
  *off = off1 + 4;
  return 0;
}

static dds_return_t deser_int16 (int16_t *dst, const struct dd * __restrict dd, size_t * __restrict off)
{
  size_t off1 = (*off + 1) & ~(size_t)1;
  int16_t tmp;
  if (off1 + 2 > dd->bufsz)
    return DDS_RETCODE_BAD_PARAMETER;
  tmp = *((int16_t *) (dd->buf + off1));
  if (dd->bswap)
    tmp = ddsrt_bswap2 (tmp);
  *dst = tmp;
  *off = off1 + 2;
  return 0;
}

static dds_return_t deser_int64 (int64_t *dst, const struct dd * __restrict dd, size_t * __restrict off)
{
  size_t off1 = (*off + 7) & ~(size_t)7;
  int64_t tmp;
  if (off1 + 8 > dd->bufsz)
    return DDS_RETCODE_BAD_PARAMETER;
  tmp = *((int64_t *) (dd->buf + off1));
  if (dd->bswap)
    tmp = ddsrt_bswap8 (tmp);
  *dst = tmp;
  *off = off1 + 8;
  return 0;
}

/* Returns true if buffer not yet exhausted, false otherwise */
static bool prtf (char * __restrict *buf, size_t * __restrict bufsize, const char *fmt, ...)
  ddsrt_attribute_format_printf(3, 4);

static bool prtf (char * __restrict *buf, size_t * __restrict bufsize, const char *fmt, ...)
{
  va_list ap;
  if (*bufsize == 0)
    return false;
  va_start (ap, fmt);
  int n = vsnprintf (*buf, *bufsize, fmt, ap);
  va_end (ap);
  if (n < 0)
  {
    **buf = 0;
    return false;
  }
  else if ((size_t) n <= *bufsize)
  {
    *buf += (size_t) n;
    *bufsize -= (size_t) n;
    return (*bufsize > 0);
  }
  else
  {
    *buf += *bufsize;
    *bufsize = 0;
    return false;
  }
}

static dds_return_t deser_reliability (void * __restrict dst, struct flagset *flagset, uint64_t flag, const struct dd * __restrict dd, struct ddsi_domaingv const * const gv)
{
  DDSRT_STATIC_ASSERT (DDS_EXTERNAL_RELIABILITY_BEST_EFFORT == 1 && DDS_EXTERNAL_RELIABILITY_RELIABLE == 2 &&
                       DDS_RELIABILITY_BEST_EFFORT == 0 && DDS_RELIABILITY_RELIABLE == 1);
  size_t srcoff = 0, dstoff = 0;
  dds_reliability_qospolicy_t * const x = deser_generic_dst (dst, &dstoff, plist_alignof (dds_reliability_qospolicy_t));
  uint32_t kind, mbtsec, mbtfrac;
  ddsi_duration_t mbt;
  (void) gv;
  if (deser_uint32 (&kind, dd, &srcoff) < 0 || deser_uint32 (&mbtsec, dd, &srcoff) < 0 || deser_uint32 (&mbtfrac, dd, &srcoff) < 0)
    return DDS_RETCODE_BAD_PARAMETER;
  if (kind < 1 || kind > 2)
    return DDS_RETCODE_BAD_PARAMETER;
  mbt.seconds = (int32_t) mbtsec;
  mbt.fraction = mbtfrac;
  if (validate_external_duration (&mbt) < 0)
    return DDS_RETCODE_BAD_PARAMETER;
  x->kind = (enum dds_reliability_kind) (kind - 1);
  x->max_blocking_time = ddsi_duration_to_dds (mbt);
  *flagset->present |= flag;
  return 0;
}

static dds_return_t ser_reliability (struct ddsi_xmsg *xmsg, ddsi_parameterid_t pid, const void *src, size_t srcoff, enum ddsrt_byte_order_selector bo, enum ddsi_plist_context_kind context_kind)
{
  (void) context_kind;
  DDSRT_STATIC_ASSERT (DDS_EXTERNAL_RELIABILITY_BEST_EFFORT == 1 && DDS_EXTERNAL_RELIABILITY_RELIABLE == 2 &&
                       DDS_RELIABILITY_BEST_EFFORT == 0 && DDS_RELIABILITY_RELIABLE == 1);
  dds_reliability_qospolicy_t const * const x = deser_generic_src (src, &srcoff, plist_alignof (dds_reliability_qospolicy_t));
  ddsi_duration_t mbt = ddsi_duration_from_dds (x->max_blocking_time);
  uint32_t * const p = ddsi_xmsg_addpar_bo (xmsg, pid, 3 * sizeof (uint32_t), bo);
  p[0] = ddsrt_toBO4u(bo, 1 + (uint32_t) x->kind);
  p[1] = ddsrt_toBO4u(bo, (uint32_t) mbt.seconds);
  p[2] = ddsrt_toBO4u(bo, mbt.fraction);
  return 0;
}

static dds_return_t valid_reliability (const void *src, size_t srcoff)
{
  dds_reliability_qospolicy_t const * const x = deser_generic_src (src, &srcoff, plist_alignof (dds_reliability_qospolicy_t));
  if ((x->kind == DDS_RELIABILITY_BEST_EFFORT || x->kind == DDS_RELIABILITY_RELIABLE) && x->max_blocking_time >= 0)
    return 0;
  else
    return DDS_RETCODE_BAD_PARAMETER;
}

static bool equal_reliability (const void *srcx, const void *srcy, size_t srcoff)
{
  dds_reliability_qospolicy_t const * const x = deser_generic_src (srcx, &srcoff, plist_alignof (dds_reliability_qospolicy_t));
  dds_reliability_qospolicy_t const * const y = deser_generic_src (srcy, &srcoff, plist_alignof (dds_reliability_qospolicy_t));
  return x->kind == y->kind && x->max_blocking_time == y->max_blocking_time;
}

static bool print_reliability (char * __restrict *buf, size_t * __restrict bufsize, const void *src, size_t srcoff)
{
  dds_reliability_qospolicy_t const * const x = deser_generic_src (src, &srcoff, plist_alignof (dds_reliability_qospolicy_t));
  return prtf (buf, bufsize, "%d:%"PRId64, (int) x->kind, x->max_blocking_time);
}

static dds_return_t deser_statusinfo (void * __restrict dst, struct flagset *flagset, uint64_t flag, const struct dd * __restrict dd, struct ddsi_domaingv const * const gv)
{
  size_t srcoff = 0, dstoff = 0;
  uint32_t * const x = deser_generic_dst (dst, &dstoff, plist_alignof (dds_reliability_qospolicy_t));
  size_t srcoff1 = (srcoff + 3) & ~(size_t)3;
  (void) gv;
  if (srcoff1 + 4 > dd->bufsz)
    return DDS_RETCODE_BAD_PARAMETER;
  /* status info is always in BE format (it is an array of 4 octets according to the spec) --
     fortunately we have 4 byte alignment anyway -- and can have bits set we don't grok
     (which we discard) */
  *x = ddsrt_fromBE4u (*((uint32_t *) (dd->buf + srcoff1))) & DDSI_STATUSINFO_STANDARDIZED;
  *flagset->present |= flag;
  return 0;
}

static dds_return_t ser_statusinfo (struct ddsi_xmsg *xmsg, ddsi_parameterid_t pid, const void *src, size_t srcoff, enum ddsrt_byte_order_selector bo, enum ddsi_plist_context_kind context_kind)
{
  (void) context_kind;
  uint32_t const * const x = deser_generic_src (src, &srcoff, plist_alignof (uint32_t));
  uint32_t * const p = ddsi_xmsg_addpar_bo (xmsg, pid, sizeof (uint32_t), bo);
  *p = ddsrt_toBE4u (*x);
  return 0;
}

static bool print_statusinfo (char * __restrict *buf, size_t * __restrict bufsize, const void *src, size_t srcoff)
{
  uint32_t const * const x = deser_generic_src (src, &srcoff, plist_alignof (uint32_t));
  return prtf (buf, bufsize, "%"PRIx32, *x);
}

static dds_return_t deser_locator (void * __restrict dst, struct flagset *flagset, uint64_t flag, const struct dd * __restrict dd, struct ddsi_domaingv const * const gv)
{
  size_t srcoff = 0, dstoff = 0;
  ddsi_locators_t * const x = deser_generic_dst (dst, &dstoff, plist_alignof (ddsi_locators_t));
  /* FIXME: don't want to modify do_locator just yet, and don't want to require that a
     locator is the only thing in the descriptor string (even though it actually always is),
     so do alignment explicitly, fake a temporary input buffer and advance the source buffer */
  srcoff = (srcoff + 3) & ~(size_t)3;
  if (srcoff > dd->bufsz || dd->bufsz - srcoff < 24)
    return DDS_RETCODE_BAD_PARAMETER;
  struct dd tmpdd = *dd;
  tmpdd.buf += srcoff;
  tmpdd.bufsz -= srcoff;
  switch (do_locator (x, *flagset->present, flagset->wanted, flag, &tmpdd, gv))
  {
    case DOLOC_INVALID:
      return DDS_RETCODE_BAD_PARAMETER;
    case DOLOC_IGNORED:
      break;
    case DOLOC_ACCEPTED:
      *flagset->present |= flag;
      break;
  }
  return 0;
}

static dds_return_t ser_locator (struct ddsi_xmsg *xmsg, ddsi_parameterid_t pid, const void *src, size_t srcoff, enum ddsrt_byte_order_selector bo, enum ddsi_plist_context_kind context_kind)
{
  (void) context_kind;
  ddsi_locators_t const * const x = deser_generic_src (src, &srcoff, plist_alignof (ddsi_locators_t));
  for (const struct ddsi_locators_one *l = x->first; l != NULL; l = l->next)
  {
    char * const p = ddsi_xmsg_addpar_bo (xmsg, pid, 24, bo);
    const int32_t kind = ddsrt_toBO4 (bo, l->loc.kind);
    const uint32_t port = ddsrt_toBO4u (bo, l->loc.port);
    memcpy (p, &kind, 4);
    memcpy (p + 4, &port, 4);
    memcpy (p + 8, l->loc.address, 16);
  }
  return 0;
}

static dds_return_t unalias_locator (void * __restrict dst, size_t * __restrict dstoff, bool gen_seq_aliased)
{
  ddsi_locators_t * const x = deser_generic_dst (dst, dstoff, plist_alignof (ddsi_locators_t));
  ddsi_locators_t newlocs = { .n = x->n, .first = NULL, .last = NULL };
  struct ddsi_locators_one **pnext = &newlocs.first;
  (void) gen_seq_aliased;
  for (const struct ddsi_locators_one *lold = x->first; lold != NULL; lold = lold->next)
  {
    struct ddsi_locators_one *n = ddsrt_memdup (lold, sizeof (*n));
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
  ddsi_locators_t * const x = deser_generic_dst (dst, dstoff, plist_alignof (ddsi_locators_t));
  if (!(*flagset->aliased & flag))
  {
    while (x->first)
    {
      struct ddsi_locators_one *l = x->first;
      x->first = l->next;
      ddsrt_free (l);
    }
  }
  return 0;
}

static const enum ddsi_pserop* pserop_advance (const enum ddsi_pserop * __restrict desc)
{
  /* Should not start on an end. */
  assert(*desc != XSTOP);

  /* If not a sequence, return next. */
  if (*desc != XQ) return (desc + 1);

  /* Jump over this sequence (ignoring nested ones). */
  int scope = 1;
  do
  {
    desc++;
    if (*desc ==    XQ) scope++;
    if (*desc == XSTOP) scope--;
  }
  while (scope != 0);

  /* We're on the stop, return next. */
  return (desc + 1);
}

static bool print_locator (char * __restrict *buf, size_t * __restrict bufsize, const void *src, size_t srcoff)
{
  ddsi_locators_t const * const x = deser_generic_src (src, &srcoff, plist_alignof (ddsi_locators_t));
  const char *sep = "";
  prtf (buf, bufsize, "{");
  for (const struct ddsi_locators_one *l = x->first; l != NULL; l = l->next)
  {
    char tmp[DDSI_LOCSTRLEN];
    ddsi_locator_to_string (tmp, sizeof (tmp), &l->loc);
    prtf (buf, bufsize, "%s%s", sep, tmp);
    sep = ",";
  }
  return prtf (buf, bufsize, "}");
}

static dds_return_t deser_type_consistency (void * __restrict dst, struct flagset *flagset, uint64_t flag, const struct dd * __restrict dd, struct ddsi_domaingv const * const gv)
{
  DDSRT_STATIC_ASSERT (DDS_TYPE_CONSISTENCY_DISALLOW_TYPE_COERCION == 0 && DDS_TYPE_CONSISTENCY_ALLOW_TYPE_COERCION == 1);
  size_t srcoff = 0, dstoff = 0;
  dds_type_consistency_enforcement_qospolicy_t * const x = deser_generic_dst (dst, &dstoff, plist_alignof (dds_type_consistency_enforcement_qospolicy_t));
  const uint32_t option_count = 5;
  uint16_t kind;
  (void) gv;
  if (deser_uint16 (&kind, dd, &srcoff) < 0)
    return DDS_RETCODE_BAD_PARAMETER;
  if (kind > DDS_TYPE_CONSISTENCY_ALLOW_TYPE_COERCION)
    return DDS_RETCODE_BAD_PARAMETER;
  x->kind = kind;
  if (dd->bufsz - srcoff < option_count)
  {
    /* set defaults as described in clause 7.6.3.4.1 of the xtypes spec */
    x->ignore_sequence_bounds = x->kind != DDS_TYPE_CONSISTENCY_DISALLOW_TYPE_COERCION;
    x->ignore_string_bounds = x->kind != DDS_TYPE_CONSISTENCY_DISALLOW_TYPE_COERCION;
    x->ignore_member_names = false;
    x->prevent_type_widening = x->kind == DDS_TYPE_CONSISTENCY_DISALLOW_TYPE_COERCION;
    x->force_type_validation = false;
  }
  else
  {
    for (uint32_t i = 0; i < option_count; i++)
      if (dd->buf[srcoff + i] > 1)
        return DDS_RETCODE_BAD_PARAMETER;
    x->force_type_validation = (bool) dd->buf[srcoff + 4];
    if (x->kind == DDS_TYPE_CONSISTENCY_DISALLOW_TYPE_COERCION)
    {
      /* set values for options that do not apply (xtypes spec 7.6.3.4.1) */
      x->ignore_sequence_bounds = false;
      x->ignore_string_bounds = false;
      x->ignore_member_names = false;
      x->prevent_type_widening = true;
    }
    else
    {
      x->ignore_sequence_bounds = (bool) dd->buf[srcoff + 0];
      x->ignore_string_bounds = (bool) dd->buf[srcoff + 1];
      x->ignore_member_names = (bool) dd->buf[srcoff + 2];
      x->prevent_type_widening = (bool) dd->buf[srcoff + 3];
    }
  }
  *flagset->present |= flag;
  return 0;
}

static dds_return_t ser_type_consistency (struct ddsi_xmsg *xmsg, ddsi_parameterid_t pid, const void *src, size_t srcoff, enum ddsrt_byte_order_selector bo, enum ddsi_plist_context_kind context_kind)
{
  (void) context_kind;
  dds_type_consistency_enforcement_qospolicy_t const * const x = deser_generic_src (src, &srcoff, plist_alignof (dds_type_consistency_enforcement_qospolicy_t));
  char * const p = ddsi_xmsg_addpar_bo (xmsg, pid, 8, bo);
  const uint16_t kind = ddsrt_toBO2u (bo, (uint16_t) x->kind);
  memcpy (p, &kind, 2);
  size_t offs = sizeof (kind);
  p[offs + 0] = x->ignore_sequence_bounds;
  p[offs + 1] = x->ignore_string_bounds;
  p[offs + 2] = x->ignore_member_names;
  p[offs + 3] = x->prevent_type_widening;
  p[offs + 4] = x->force_type_validation;
  return 0;
}

static dds_return_t valid_type_consistency (const void *src, size_t srcoff)
{
  DDSRT_STATIC_ASSERT (DDS_TYPE_CONSISTENCY_DISALLOW_TYPE_COERCION == 0 && DDS_TYPE_CONSISTENCY_ALLOW_TYPE_COERCION == 1);
  dds_type_consistency_enforcement_qospolicy_t const * const x = deser_generic_src (src, &srcoff, plist_alignof (dds_type_consistency_enforcement_qospolicy_t));
  if (x->kind > DDS_TYPE_CONSISTENCY_ALLOW_TYPE_COERCION)
    return DDS_RETCODE_BAD_PARAMETER;
  return 0;
}

static bool equal_type_consistency (const void *srcx, const void *srcy, size_t srcoff)
{
  dds_type_consistency_enforcement_qospolicy_t const * const x = deser_generic_src (srcx, &srcoff, plist_alignof (dds_type_consistency_enforcement_qospolicy_t));
  dds_type_consistency_enforcement_qospolicy_t const * const y = deser_generic_src (srcy, &srcoff, plist_alignof (dds_type_consistency_enforcement_qospolicy_t));
  return x->kind == y->kind
    && x->ignore_sequence_bounds == y->ignore_sequence_bounds
    && x->ignore_string_bounds== y->ignore_string_bounds
    && x->ignore_member_names == y->ignore_member_names
    && x->prevent_type_widening == y->prevent_type_widening
    && x->force_type_validation == y->force_type_validation;
}

static bool print_type_consistency (char * __restrict *buf, size_t * __restrict bufsize, const void *src, size_t srcoff)
{
  dds_type_consistency_enforcement_qospolicy_t const * const x = deser_generic_src (src, &srcoff, plist_alignof (dds_type_consistency_enforcement_qospolicy_t));
  return prtf (buf, bufsize, "%d:%d%d%d%d%d", (int) x->kind, x->ignore_sequence_bounds, x->ignore_string_bounds, x->ignore_member_names, x->prevent_type_widening, x->force_type_validation);
}

static const enum ddsi_pserop desc_liveliness[] = { XE2, XD, XSTOP };

static dds_return_t validate_liveliness_kind (uint32_t kind)
{
  switch (kind)
  {
    case DDS_LIVELINESS_AUTOMATIC:
    case DDS_LIVELINESS_MANUAL_BY_PARTICIPANT:
    case DDS_LIVELINESS_MANUAL_BY_TOPIC:
      return 0;
  }
  return DDS_RETCODE_BAD_PARAMETER;
}

static dds_return_t deser_liveliness (void * __restrict dst, struct flagset *flagset, uint64_t flag, const struct dd * __restrict dd, struct ddsi_domaingv const * const gv)
{
  (void) flag; (void) gv;
  size_t srcoff = 0, dstoff = 0;
  dds_liveliness_qospolicy_t * const x = deser_generic_dst (dst, &dstoff, plist_alignof (dds_liveliness_qospolicy_t));
  switch (dd->context_kind)
  {
    case DDSI_PLIST_CONTEXT_PARTICIPANT: {
      x->kind = DDS_LIVELINESS_AUTOMATIC;
      break;
    }
    case DDSI_PLIST_CONTEXT_ENDPOINT:
    case DDSI_PLIST_CONTEXT_TOPIC:
    case DDSI_PLIST_CONTEXT_INLINE_QOS: {
      uint32_t kind;
      if (deser_uint32 (&kind, dd, &srcoff) < 0 || validate_liveliness_kind (kind) != 0)
        return DDS_RETCODE_BAD_PARAMETER;
      x->kind = (dds_liveliness_kind_t) kind;
      break;
    }
    case DDSI_PLIST_CONTEXT_QOS_DISALLOWED: {
      return DDS_RETCODE_BAD_PARAMETER;
    }
  }
  ddsi_duration_t tmp;
  if (deser_uint32 ((uint32_t *) &tmp.seconds, dd, &srcoff) < 0 ||
      deser_uint32 (&tmp.fraction, dd, &srcoff) < 0 ||
      validate_external_duration (&tmp) != 0)
    return DDS_RETCODE_BAD_PARAMETER;
  x->lease_duration = ddsi_duration_to_dds (tmp);
  *flagset->present |= DDSI_QP_LIVELINESS;
  return 0;
}

static dds_return_t ser_liveliness (struct ddsi_xmsg *xmsg, ddsi_parameterid_t pid, const void *src, size_t srcoff, enum ddsrt_byte_order_selector bo, enum ddsi_plist_context_kind context_kind)
{
  (void) pid;
  dds_liveliness_qospolicy_t const * const x = deser_generic_src (src, &srcoff, plist_alignof (dds_liveliness_qospolicy_t));
  switch (context_kind)
  {
    case DDSI_PLIST_CONTEXT_PARTICIPANT: {
      uint32_t * const p = ddsi_xmsg_addpar_bo (xmsg, DDSI_PID_PARTICIPANT_LEASE_DURATION, sizeof (ddsi_duration_t), bo);
      const ddsi_duration_t tmp = ddsi_duration_from_dds (x->lease_duration);
      assert (x->kind == DDS_LIVELINESS_AUTOMATIC);
      p[0] = ddsrt_toBO4u(bo, (uint32_t) tmp.seconds);
      p[1] = ddsrt_toBO4u(bo, tmp.fraction);
      break;
    }
    case DDSI_PLIST_CONTEXT_ENDPOINT:
    case DDSI_PLIST_CONTEXT_TOPIC:
    case DDSI_PLIST_CONTEXT_INLINE_QOS: {
      uint32_t * const p = ddsi_xmsg_addpar_bo (xmsg, DDSI_PID_LIVELINESS, sizeof (uint32_t) + sizeof (ddsi_duration_t), bo);
      const ddsi_duration_t tmp = ddsi_duration_from_dds (x->lease_duration);
      p[0] = ddsrt_toBO4u(bo, (uint32_t) x->kind);
      p[1] = ddsrt_toBO4u(bo, (uint32_t) tmp.seconds);
      p[2] = ddsrt_toBO4u(bo, tmp.fraction);
      break;
    }
    case DDSI_PLIST_CONTEXT_QOS_DISALLOWED: {
      return DDS_RETCODE_BAD_PARAMETER;
    }
  }
  return 0;
}

static dds_return_t valid_liveliness (const void *src, size_t srcoff)
{
  return valid_generic (src, srcoff, desc_liveliness);
}

static bool equal_liveliness (const void *srcx, const void *srcy, size_t srcoff)
{
  return equal_generic (srcx, srcy, srcoff, desc_liveliness);
}

static bool print_liveliness (char * __restrict *buf, size_t * __restrict bufsize, const void *src, size_t srcoff)
{
  return print_generic (buf, bufsize, src, srcoff, desc_liveliness);
}

static const enum ddsi_pserop desc_data_representation[] =  { XQ, Xs, XSTOP, XSTOP };

static dds_return_t deser_data_representation (void * __restrict dst, struct flagset *flagset, uint64_t flag, const struct dd * __restrict dd, struct ddsi_domaingv const * const gv)
{
  size_t srcoff = 0, dstoff = 0;
  dds_data_representation_qospolicy_t * const x = deser_generic_dst (dst, &dstoff, plist_alignof (dds_data_representation_qospolicy_t));
  uint32_t cnt;
  int16_t v;
  (void) gv;
  if (deser_uint32 (&cnt, dd, &srcoff) < 0 || cnt > (dd->bufsz - srcoff) / sizeof (int16_t))
    return DDS_RETCODE_BAD_PARAMETER;
  /* Consider an empty list as if the parameter is not set, the resulting qos will get default
     data representation when the default qos for the entity is merged in. */
  if (cnt == 0)
    return 0;
  x->value.n = cnt;
  x->value.ids = ddsrt_malloc (cnt * sizeof (*x->value.ids));
  /* coverity[tainted_data: FALSE] */
  for (uint32_t n = 0; n < cnt; n++)
  {
    if (deser_int16 (&v, dd, &srcoff) < 0)
      return DDS_RETCODE_BAD_PARAMETER;
    for (uint32_t n2 = 0; n2 < n; n2++)
      if (x->value.ids[n2] == v)
        continue;
    x->value.ids[n] = v;
  }
  *flagset->present |= flag;
  return 0;
}

static dds_return_t ser_data_representation (struct ddsi_xmsg *xmsg, ddsi_parameterid_t pid, const void *src, size_t srcoff, enum ddsrt_byte_order_selector bo, enum ddsi_plist_context_kind context_kind)
{
  (void) context_kind;
  return ser_generic (xmsg, pid, src, srcoff, desc_data_representation, bo);
}

static dds_return_t valid_data_representation (const void *src, size_t srcoff)
{
  return valid_generic (src, srcoff, desc_data_representation);
}

static bool equal_data_representation (const void *srcx, const void *srcy, size_t srcoff)
{
  return equal_generic (srcx, srcy, srcoff, desc_data_representation);
}

static dds_return_t unalias_data_representation (void * __restrict dst, size_t * __restrict dstoff, bool gen_seq_aliased)
{
  return unalias_generic (dst, dstoff, gen_seq_aliased, desc_data_representation);
}

static dds_return_t fini_data_representation (void * __restrict dst, size_t * __restrict dstoff, struct flagset *flagset, uint64_t flag)
{
  return fini_generic (dst, dstoff, flagset, flag, desc_data_representation);
}

static bool print_data_representation (char * __restrict *buf, size_t * __restrict bufsize, const void *src, size_t srcoff)
{
  dds_data_representation_qospolicy_t const * const x = deser_generic_src (src, &srcoff, plist_alignof (dds_data_representation_qospolicy_t));
  prtf (buf, bufsize, "%"PRIu32"(", x->value.n);
  const char *sep = "";
  for (uint32_t n = 0; n < x->value.n; n++)
  {
    prtf (buf, bufsize, "%s%i", sep, x->value.ids[n]);
    sep = ",";
  }
  return prtf (buf, bufsize, ")");
}

#ifdef DDS_HAS_TYPE_DISCOVERY

static dds_return_t deser_type_information (void * __restrict dst, struct flagset *flagset, uint64_t flag, const struct dd * __restrict dd, struct ddsi_domaingv const * const gv)
{
  (void) gv;
  size_t dstoff = 0;
  uint32_t srcoff = 0;
  unsigned char *buf;
  dds_return_t ret = 0;

  if (dd->bswap)
    buf = ddsrt_memdup (dd->buf, dd->bufsz);
  else
    buf = (unsigned char *) dd->buf;
  if (!dds_stream_normalize_data ((char *) buf, &srcoff, (uint32_t) dd->bufsz, dd->bswap, DDSI_RTPS_CDR_ENC_VERSION_2, DDS_XTypes_TypeInformation_desc.m_ops))
  {
    ret = DDS_RETCODE_BAD_PARAMETER;
    goto err_normalize;
  }

  dds_istream_t is = { .m_buffer = buf, .m_index = 0, .m_size = (uint32_t) dd->bufsz, .m_xcdr_version = DDSI_RTPS_CDR_ENC_VERSION_2 };
  ddsi_typeinfo_t const ** x = deser_generic_dst (dst, &dstoff, plist_alignof (ddsi_typeinfo_t *));
  *x = ddsrt_calloc (1, DDS_XTypes_TypeInformation_desc.m_size);
  dds_stream_read (&is, (void *) *x, &dds_cdrstream_default_allocator, DDS_XTypes_TypeInformation_desc.m_ops);
  *flagset->present |= flag;
err_normalize:
  if (dd->bswap)
    ddsrt_free (buf);
  return ret;
}

static dds_return_t ser_type_information (struct ddsi_xmsg *xmsg, ddsi_parameterid_t pid, const void *src, size_t srcoff, enum ddsrt_byte_order_selector bo, enum ddsi_plist_context_kind context_kind)
{
  (void) context_kind;
  ddsi_typeinfo_t const * const * x = deser_generic_src (src, &srcoff, plist_alignof (ddsi_typeinfo_t *));

  dds_ostream_t os = { .m_buffer = NULL, .m_index = 0, .m_size = 0, .m_xcdr_version = DDSI_RTPS_CDR_ENC_VERSION_2 };
  (void) dds_stream_write_with_byte_order (&os, &dds_cdrstream_default_allocator, (const void *) *x, DDS_XTypes_TypeInformation_desc.m_ops, bo);
  char * const p = ddsi_xmsg_addpar_bo (xmsg, pid, os.m_index, bo);
  memcpy (p, os.m_buffer, os.m_index);
  dds_ostream_fini (&os, &dds_cdrstream_default_allocator);
  return 0;
}

static dds_return_t valid_type_information (const void *src, size_t srcoff)
{
  ddsi_typeinfo_t const * const * x = deser_generic_src (src, &srcoff, plist_alignof (ddsi_typeinfo_t *));
  return (*x != NULL && ddsi_typeinfo_valid (*x)) ? DDS_RETCODE_OK : DDS_RETCODE_BAD_PARAMETER;
}

static bool equal_type_information (const void *srcx, const void *srcy, size_t srcoff)
{
  ddsi_typeinfo_t const * const * x = deser_generic_src (srcx, &srcoff, plist_alignof (ddsi_typeinfo_t *));
  ddsi_typeinfo_t const * const * y = deser_generic_src (srcy, &srcoff, plist_alignof (ddsi_typeinfo_t *));
  return ddsi_typeinfo_equal (*x, *y, DDSI_TYPE_INCLUDE_DEPS);
}

static dds_return_t unalias_type_information (void * __restrict dst, size_t * __restrict dstoff, bool gen_seq_aliased)
{
  ddsi_typeinfo_t const * * x = deser_generic_dst (dst, dstoff, plist_alignof (ddsi_typeinfo_t *));
  ddsi_typeinfo_t * new_type_info = ddsi_typeinfo_dup (*x);
  (void) gen_seq_aliased;
  *x = new_type_info;
  *dstoff += sizeof (*x);
  return 0;
}

static dds_return_t fini_type_information (void * __restrict dst, size_t * __restrict dstoff, struct flagset *flagset, uint64_t flag)
{
  ddsi_typeinfo_t const * const * x = deser_generic_src (dst, dstoff, plist_alignof (ddsi_typeinfo_t *));
  if ((*flagset->present & flag) && !(*flagset->aliased & flag))
  {
    ddsi_typeinfo_fini ((ddsi_typeinfo_t *) *x);
    ddsrt_free ((ddsi_typeinfo_t *) *x);
  }
  return 0;
}

static bool print_type_information (char * __restrict *buf, size_t * __restrict bufsize, const void *src, size_t srcoff)
{
  ddsi_typeinfo_t const * const * x = deser_generic_src (src, &srcoff, plist_alignof (ddsi_typeinfo_t *));
  struct ddsi_typeid_str tpstrm, tpstrc;
  return prtf (buf, bufsize, "%s/%s",
    ddsi_make_typeid_str (&tpstrm, ddsi_typeinfo_minimal_typeid (*x)),
    ddsi_make_typeid_str (&tpstrc, ddsi_typeinfo_complete_typeid (*x)));
}

#endif /* DDS_HAS_TYPE_DISCOVERY */

// The XE1 .. XE3 codes are the only ones dealing with an enumerated type and are
// assumed to map to the same type, which would traditionally be an "int", but in
// gcc with the -fshort-enums option enabled, it is a char.  That happens quite a
// bit on embedded platforms, judging by a few tickets.
//
// This enum definition is used only in the serialisation/deserialisation code to
// stand in for the actual type.
enum xe3_prototype { XE3_PROTO_0, XE3_PROTO_1, XE3_PROTO_2, XE3_PROTO_3 };

static size_t ser_generic_srcsize (const enum ddsi_pserop * __restrict desc)
{
  size_t srcoff = 0, srcalign = 0;
#define SIMPLE(basecase_, type_) do {                          \
    const uint32_t cnt = 1 + (uint32_t) (*desc - (basecase_)); \
    const size_t align = plist_alignof (type_);                      \
    srcalign = (align > srcalign) ? align : srcalign;          \
    srcoff = (srcoff + align - 1) & ~(align - 1);              \
    srcoff += cnt * sizeof (type_);                            \
  } while (0)
  while (true)
  {
    switch (*desc)
    {
      case XSTOP: return (srcoff + srcalign - 1) & ~(srcalign - 1);
      case XO: SIMPLE (XO, ddsi_octetseq_t); break;
      case XS: SIMPLE (XS, const char *); break;
      case XE1: case XE2: case XE3: SIMPLE (*desc, enum xe3_prototype); break;
      case Xs: SIMPLE (Xs, int16_t); break;
      case Xi: case Xix2: case Xix3: case Xix4: SIMPLE (Xi, int32_t); break;
      case Xu: case Xux2: case Xux3: case Xux4: case Xux5: SIMPLE (Xu, uint32_t); break;
      case Xl: SIMPLE (Xl, int64_t); break;
      case XD: case XDx2: SIMPLE (XD, dds_duration_t); break;
      case Xo: case Xox2: SIMPLE (Xo, unsigned char); break;
      case Xb: case Xbx2: case Xbx3: case Xbx4: case Xbx5: SIMPLE (Xb, unsigned char); break;
      case XbCOND: SIMPLE (XbCOND, unsigned char); break;
      case XG: SIMPLE (XG, ddsi_guid_t); break;
      case XK: SIMPLE (XK, ddsi_keyhash_t); break;
      case XbPROP: SIMPLE (XbPROP, unsigned char); break;
      case XQ: SIMPLE (XQ, ddsi_octetseq_t); break;
      case Xopt: break;
    }
    desc = pserop_advance(desc);
  }
#undef SIMPLE
}

size_t ddsi_plist_memsize_generic (const enum ddsi_pserop * __restrict desc)
{
  return ser_generic_srcsize (desc);
}

static bool fini_generic_embeddable (void * __restrict dst, size_t * __restrict dstoff, const enum ddsi_pserop *desc, const enum ddsi_pserop * const desc_end, bool aliased)
{
  bool freed = false;
#define COMPLEX(basecase_, type_, cleanup_unaliased_, cleanup_always_) do { \
    type_ *x = deser_generic_dst (dst, dstoff, plist_alignof (type_));      \
    const uint32_t cnt = 1 + (uint32_t) (*desc - (basecase_));              \
    for (uint32_t xi = 0; xi < cnt; xi++, x++) {                            \
      if (!aliased) do { cleanup_unaliased_; } while (0);                   \
      do { cleanup_always_; } while (0);                                    \
    }                                                                       \
    *dstoff += cnt * sizeof (*x);                                           \
  } while (0)
#define SIMPLE(basecase_, type_) COMPLEX (basecase_, type_, (void) 0, (void) 0)
  while ((desc_end == NULL) || (desc < desc_end))
  {
    switch (*desc)
    {
      case XSTOP: return freed;
      case XO: COMPLEX (XO, ddsi_octetseq_t, { ddsrt_free (x->value); freed = true; }, (void) 0); break;
      case XS: COMPLEX (XS, char *, { ddsrt_free (*x); freed = true; }, (void) 0); break;
      case XE1: case XE2: case XE3: COMPLEX (*desc, enum xe3_prototype, (void) 0, (void) 0); break;
      case Xs: SIMPLE (Xs, int16_t); break;
      case Xi: case Xix2: case Xix3: case Xix4: SIMPLE (Xi, int32_t); break;
      case Xu: case Xux2: case Xux3: case Xux4: case Xux5: SIMPLE (Xu, uint32_t); break;
      case Xl: SIMPLE (Xl, int64_t); break;
      case XD: case XDx2: SIMPLE (XD, dds_duration_t); break;
      case Xo: case Xox2: SIMPLE (Xo, unsigned char); break;
      case Xb: case Xbx2: case Xbx3: case Xbx4: case Xbx5: SIMPLE (Xb, unsigned char); break;
      case XbCOND: SIMPLE (XbCOND, unsigned char); break;
      case XG: SIMPLE (XG, ddsi_guid_t); break;
      case XK: SIMPLE (XK, ddsi_keyhash_t); break;
      case XbPROP: SIMPLE (XbPROP, unsigned char); break;
      case XQ:
      {
        ddsi_octetseq_t *x = deser_generic_dst (dst, dstoff, plist_alignof (ddsi_octetseq_t));
        const size_t elem_size = ser_generic_srcsize (desc + 1);
        bool elem_freed = true;
        for (uint32_t i = 0; (i < x->length) && elem_freed; i++)
        {
          size_t elem_off = i * elem_size;
          elem_freed = fini_generic_embeddable (x->value, &elem_off, desc + 1, desc_end, aliased);
        }
        ddsrt_free (x->value);
        freed = true;
        *dstoff += sizeof (ddsi_octetseq_t);
      }
      break;
      case Xopt: break;
    }
    desc = pserop_advance(desc);
  }
#undef SIMPLE
#undef COMPLEX
  return freed;
}

static size_t pserop_memalign (enum ddsi_pserop op)
{
  switch (op)
  {
    case XO: case XQ: return plist_alignof (ddsi_octetseq_t);
    case XS: return plist_alignof (char *);
    case XG: return plist_alignof (ddsi_guid_t);
    case XK: return plist_alignof (ddsi_keyhash_t);
    case Xb: case Xbx2: case Xbx3: case Xbx4: case Xbx5: return 1;
    case Xo: case Xox2: return 1;
    case XbCOND: case XbPROP: return 1;
    case XE1: case XE2: case XE3: return plist_alignof (enum xe3_prototype);
    case Xs: return plist_alignof (int16_t);
    case Xi: case Xix2: case Xix3: case Xix4: return plist_alignof (int32_t);
    case Xu: case Xux2: case Xux3: case Xux4: case Xux5: return plist_alignof (uint32_t);
    case Xl: return plist_alignof (int64_t);
    case XD: case XDx2: return plist_alignof (dds_duration_t);
    case XSTOP: case Xopt: assert (0);
  }
  return 0;
}

static dds_return_t deser_generic_r (void * __restrict dst, size_t * __restrict dstoff, struct flagset *flagset, uint64_t flag, const struct dd * __restrict dd, size_t * __restrict srcoff, const enum ddsi_pserop * __restrict desc)
{
  enum ddsi_pserop const * const desc_in = desc;
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
        goto success;
      case XO: { /* octet sequence */
        ddsi_octetseq_t * const x = deser_generic_dst (dst, dstoff, plist_alignof (ddsi_octetseq_t));
        if (deser_uint32 (&x->length, dd, srcoff) < 0 || dd->bufsz - *srcoff < x->length)
          goto fail;
        x->value = x->length ? (unsigned char *) (dd->buf + *srcoff) : NULL;
        *srcoff += x->length;
        *dstoff += sizeof (*x);
        *flagset->aliased |= flag;
        break;
      }
      case XS: { /* string: alias as-if octet sequence, do additional checks and store as string */
        char ** const x = deser_generic_dst (dst, dstoff, plist_alignof (char *));
        ddsi_octetseq_t tmp;
        size_t tmpoff = 0;
        if (deser_generic_r (&tmp, &tmpoff, flagset, flag, dd, srcoff, (enum ddsi_pserop []) { XO, XSTOP }) < 0)
          goto fail;
        if (tmp.length < 1 || tmp.value[tmp.length - 1] != 0)
          goto fail;
        *x = (char *) tmp.value;
        *dstoff += sizeof (*x);
        break;
      }
      case XE1: case XE2: case XE3: { /* enum with max allowed value */
        enum xe3_prototype * const x = deser_generic_dst (dst, dstoff, plist_alignof (enum xe3_prototype));
        const uint32_t maxval = 1 + (uint32_t) (*desc - XE1);
        uint32_t tmp;
        if (deser_uint32 (&tmp, dd, srcoff) < 0 || tmp > maxval)
          goto fail;
        *x = (enum xe3_prototype) tmp;
        *dstoff += sizeof (*x);
        break;
      }
      case Xs: { /* int16_t */
        uint16_t * const x = deser_generic_dst (dst, dstoff, plist_alignof (uint16_t));
        if (deser_uint16(x, dd, srcoff) < 0)
          goto fail;
        *dstoff += sizeof (*x);
        break;
      }
      case Xi: case Xix2: case Xix3: case Xix4: { /* int32_t(s) */
        uint32_t * const x = deser_generic_dst (dst, dstoff, plist_alignof (uint32_t));
        const uint32_t cnt = 1 + (uint32_t) (*desc - Xi);
        for (uint32_t i = 0; i < cnt; i++)
          if (deser_uint32 (&x[i], dd, srcoff) < 0)
            goto fail;
        *dstoff += cnt * sizeof (*x);
        break;
      }
      case Xu: case Xux2: case Xux3: case Xux4: case Xux5: { /* uint32_t(s): treated the same */
        uint32_t * const x = deser_generic_dst (dst, dstoff, plist_alignof (uint32_t));
        const uint32_t cnt = 1 + (uint32_t) (*desc - Xu);
        for (uint32_t i = 0; i < cnt; i++)
          if (deser_uint32 (&x[i], dd, srcoff) < 0)
            goto fail;
        *dstoff += cnt * sizeof (*x);
        break;
      }
      case Xl: { /* int64_t */
        int64_t * const x = deser_generic_dst (dst, dstoff, plist_alignof (int64_t));
        if (deser_int64(x, dd, srcoff) < 0)
          goto fail;
        *dstoff += sizeof (*x);
        break;
      }
      case XD: case XDx2: { /* duration(s): int64_t <=> int32_t.uint32_t (seconds.fraction) */
        dds_duration_t * const x = deser_generic_dst (dst, dstoff, plist_alignof (dds_duration_t));
        const uint32_t cnt = 1 + (uint32_t) (*desc - XD);
        for (uint32_t i = 0; i < cnt; i++)
        {
          ddsi_duration_t tmp;
          if (deser_uint32 ((uint32_t *) &tmp.seconds, dd, srcoff) < 0 || deser_uint32 (&tmp.fraction, dd, srcoff) < 0)
            goto fail;
          if (validate_external_duration (&tmp))
            goto fail;
          x[i] = ddsi_duration_to_dds (tmp);
        }
        *dstoff += cnt * sizeof (*x);
        break;
      }
      case Xo: case Xox2: { /* octet(s) */
        unsigned char * const x = deser_generic_dst (dst, dstoff, plist_alignof (unsigned char));
        const uint32_t cnt = 1 + (uint32_t) (*desc - Xo);
        if (dd->bufsz - *srcoff < cnt)
          goto fail;
        memcpy (x, dd->buf + *srcoff, cnt);
        *srcoff += cnt;
        *dstoff += cnt * sizeof (*x);
        break;
      }
      case Xb: case Xbx2: case Xbx3: case Xbx4: case Xbx5: case XbCOND: { /* boolean(s) */
        unsigned char * const x = deser_generic_dst (dst, dstoff, plist_alignof (unsigned char));
        const uint32_t cnt = 1 + ((*desc == XbCOND) ? 0 : (uint32_t) (*desc - Xb));
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
      case XbPROP: { /* "propagate" flag, boolean, implied in serialized representation */
        unsigned char * const x = deser_generic_dst (dst, dstoff, plist_alignof (unsigned char));
        *x = 1;
        *dstoff += sizeof (*x);
        break;
      }
      case XG: { /* GUID */
        ddsi_guid_t * const x = deser_generic_dst (dst, dstoff, plist_alignof (ddsi_guid_t));
        if (dd->bufsz - *srcoff < sizeof (*x))
          goto fail;
        memcpy (x, dd->buf + *srcoff, sizeof (*x));
        *x = ddsi_ntoh_guid (*x);
        *srcoff += sizeof (*x);
        *dstoff += sizeof (*x);
        break;
      }
      case XK: { /* keyhash */
        ddsi_keyhash_t * const x = deser_generic_dst (dst, dstoff, plist_alignof (ddsi_keyhash_t));
        if (dd->bufsz - *srcoff < sizeof (*x))
          goto fail;
        memcpy (x, dd->buf + *srcoff, sizeof (*x));
        *srcoff += sizeof (*x);
        *dstoff += sizeof (*x);
        break;
      }
      case XQ: { /* arbitrary sequence */
        ddsi_octetseq_t * const x = deser_generic_dst (dst, dstoff, plist_alignof (ddsi_octetseq_t));
        if (deser_uint32 (&x->length, dd, srcoff) < 0 || x->length > dd->bufsz - *srcoff)
          goto fail;
        const size_t elem_size = ser_generic_srcsize (desc + 1);
        x->value = x->length ? ddsrt_malloc (x->length * elem_size) : NULL;
        for (uint32_t i = 0; i < x->length; i++)
        {
          size_t elem_off = i * elem_size;
          if (deser_generic_r (x->value, &elem_off, flagset, flag, dd, srcoff, desc + 1) < 0)
          {
            bool elem_freed = true;
            for (uint32_t f = 0; (f < i) && (elem_freed); f++)
            {
              size_t free_off = f * elem_size;
              elem_freed = fini_generic_embeddable (x->value, &free_off, desc + 1, NULL, *flagset->aliased & flag);
            }
            ddsrt_free (x->value);
            goto fail;
          }
        }
        *dstoff += sizeof (*x);
        break;
      }
      case Xopt: { /* remainder is optional; alignment is very nearly always 4 */
        bool end_of_input;
        size_t align = pserop_seralign(desc[1]);
        *srcoff = alignN(*srcoff, align);
        end_of_input = (*srcoff + align > dd->bufsz);
        if (end_of_input)
        {
          void * const x = deser_generic_dst (dst, dstoff, pserop_memalign (desc[1]));
          size_t rem_size = ser_generic_srcsize (desc + 1);
          memset (x, 0, rem_size);
          goto success;
        }
      }
    }
    desc = pserop_advance(desc);
  }
success:
  *flagset->present |= flag;
  return 0;

fail:
  (void)fini_generic_embeddable (dst, &dstoff_in, desc_in, desc, *flagset->aliased & flag);
  return DDS_RETCODE_BAD_PARAMETER;
}

static dds_return_t deser_generic (void * __restrict dst, struct flagset *flagset, uint64_t flag, const struct dd * __restrict dd, const enum ddsi_pserop * __restrict desc)
{
  size_t srcoff = 0, dstoff = 0;
  dds_return_t ret;
  ret = deser_generic_r (dst, &dstoff, flagset, flag, dd, &srcoff, desc);
  if (ret != 0)
  {
    *flagset->present &= ~flag;
    *flagset->aliased &= ~flag;
  }
  return ret;
}

dds_return_t ddsi_plist_deser_generic_srcoff (void * __restrict dst, const void * __restrict src, size_t srcsize, size_t *srcoff, bool bswap, const enum ddsi_pserop * __restrict desc)
{
  struct dd dd = {
    .buf = src,
    .bufsz = srcsize,
    .bswap = bswap,
    .protocol_version = {0,0},
    .vendorid = DDSI_VENDORID_ECLIPSE,
    .context_kind = DDSI_PLIST_CONTEXT_QOS_DISALLOWED
  };
  uint64_t present = 0, aliased = 0;
  struct flagset fs = { .present = &present, .aliased = &aliased, .wanted = 1 };
  size_t dstoff = 0;
  return deser_generic_r (dst, &dstoff, &fs, 1, &dd, srcoff, desc);
}

dds_return_t ddsi_plist_deser_generic (void * __restrict dst, const void * __restrict src, size_t srcsize, bool bswap, const enum ddsi_pserop * __restrict desc)
{
  size_t srcoff = 0;
  return ddsi_plist_deser_generic_srcoff (dst, src, srcsize, &srcoff, bswap, desc);
}

void ddsi_plist_ser_generic_size_embeddable (size_t *dstoff, const void *src, size_t srcoff, const enum ddsi_pserop * __restrict desc)
{
#define COMPLEX(basecase_, type_, dstoff_update_) do {                        \
    type_ const *x = deser_generic_src (src, &srcoff, plist_alignof (type_)); \
    const uint32_t cnt = 1 + (uint32_t) (*desc - (basecase_));                \
    for (uint32_t xi = 0; xi < cnt; xi++, x++) { dstoff_update_; }            \
    srcoff += cnt * sizeof (*x);                                              \
  } while (0)
#define SIMPLE1(basecase_, type_) COMPLEX (basecase_, type_, *dstoff = *dstoff + sizeof (*x))
#define SIMPLE2(basecase_, type_) COMPLEX (basecase_, type_, *dstoff = align2 (*dstoff) + sizeof (*x))
#define SIMPLE4(basecase_, type_) COMPLEX (basecase_, type_, *dstoff = align4 (*dstoff) + sizeof (*x))
#define SIMPLE8(basecase_, type_) COMPLEX (basecase_, type_, *dstoff = align8 (*dstoff) + sizeof (*x))
  while (true)
  {
    switch (*desc)
    {
      case XSTOP: return;
      case XO: COMPLEX (XO, ddsi_octetseq_t, *dstoff = align4 (*dstoff) + 4 + x->length); break;
      case XS: COMPLEX (XS, const char *, *dstoff = align4 (*dstoff) + 4 + strlen (*x) + 1); break;
      case XE1: case XE2: case XE3: COMPLEX (*desc, enum xe3_prototype, *dstoff = align4 (*dstoff) + 4); break;
      case Xs: SIMPLE2 (Xs, int16_t); break;
      case Xi: case Xix2: case Xix3: case Xix4: SIMPLE4 (Xi, int32_t); break;
      case Xu: case Xux2: case Xux3: case Xux4: case Xux5: SIMPLE4 (Xu, uint32_t); break;
      case Xl: SIMPLE8 (Xl, int64_t); break;
      case XD: case XDx2: SIMPLE4 (XD, dds_duration_t); break;
      case Xo: case Xox2: SIMPLE1 (Xo, unsigned char); break;
      case Xb: case Xbx2: case Xbx3: case Xbx4: case Xbx5: SIMPLE1 (Xb, unsigned char); break;
      case XbCOND: SIMPLE1 (XbCOND, unsigned char); break;
      case XG: SIMPLE1 (XG, ddsi_guid_t); break;
      case XK: SIMPLE1 (XK, ddsi_keyhash_t); break;
      case XbPROP: /* "propagate" boolean: when 'false'; no serialisation; no size; force early out */
               COMPLEX (XbPROP, unsigned char, if (! *x) return); break;
      case XQ: COMPLEX (XQ, ddsi_octetseq_t, {
        const size_t elem_size = ser_generic_srcsize (desc + 1);
        *dstoff = align4 (*dstoff) + 4;
        for (uint32_t i = 0; i < x->length; i++)
          ddsi_plist_ser_generic_size_embeddable (dstoff, x->value, i * elem_size, desc + 1);
      }); break;
      case Xopt: break;
    }
    desc = pserop_advance(desc);
  }
#undef SIMPLE8
#undef SIMPLE4
#undef SIMPLE1
#undef COMPLEX
}

static size_t ser_generic_size (const void *src, size_t srcoff, const enum ddsi_pserop * __restrict desc)
{
  size_t dstoff = 0;
  ddsi_plist_ser_generic_size_embeddable (&dstoff, src, srcoff, desc);
  return dstoff;
}

static uint32_t ser_generic_count (const ddsi_octetseq_t *src, size_t elem_size, const enum ddsi_pserop * __restrict desc)
{
  /* This whole thing exists solely for dealing with the vile "propagate" boolean, which must come first in an
     element, or one can't deserialize it at all.  Therefore, if desc doesn't start with XbPROP, all "length"
     elements are included in the output */
  if (*desc != XbPROP)
    return src->length;
  /* and if it does start with XbPROP, only those for which it is true are included */
  uint32_t count = 0;
  for (uint32_t i = 0; i < src->length; i++)
    if (src->value[i * elem_size])
      count++;
  return count;
}

dds_return_t ddsi_plist_ser_generic_embeddable (char * const data, size_t *dstoff, const void *src, size_t srcoff, const enum ddsi_pserop * __restrict desc, enum ddsrt_byte_order_selector bo)
{
  while (true)
  {
    switch (*desc)
    {
      case XSTOP:
        return 0;
      case XO: { /* octet sequence */
        ddsi_octetseq_t const * const x = deser_generic_src (src, &srcoff, plist_alignof (ddsi_octetseq_t));
        char * const p = ser_generic_align4 (data, dstoff);
        *((uint32_t *) p) = ddsrt_toBO4u(bo, x->length);
        if (x->length) memcpy (p + 4, x->value, x->length);
        *dstoff += 4 + x->length;
        srcoff += sizeof (*x);
        break;
      }
      case XS: { /* string */
        char const * const * const x = deser_generic_src (src, &srcoff, plist_alignof (char *));
        const uint32_t size = (uint32_t) (strlen (*x) + 1);
        char * const p = ser_generic_align4 (data, dstoff);
        *((uint32_t *) p) = ddsrt_toBO4u(bo, size);
        memcpy (p + 4, *x, size);
        *dstoff += 4 + size;
        srcoff += sizeof (*x);
        break;
      }
      case XE1: case XE2: case XE3: { /* enum */
        enum xe3_prototype const * const x = deser_generic_src (src, &srcoff, plist_alignof (enum xe3_prototype));
        uint32_t * const p = ser_generic_align4 (data, dstoff);
        *p = ddsrt_toBO4u(bo, (uint32_t) *x);
        *dstoff += 4;
        srcoff += sizeof (*x);
        break;
      }
      case Xs: { /* int16_t */
        int16_t const * const x = deser_generic_src (src, &srcoff, plist_alignof (int16_t));
        int16_t * const p = ser_generic_align2 (data, dstoff);
        *p = ddsrt_toBO2(bo, *x);
        *dstoff += sizeof (*x);
        srcoff += sizeof (*x);
        break;
      }
      case Xi: case Xix2: case Xix3: case Xix4: { /* int32_t(s) */
        int32_t const * const x = deser_generic_src (src, &srcoff, plist_alignof (int32_t));
        const uint32_t cnt = 1 + (uint32_t) (*desc - Xi);
        int32_t * const p = ser_generic_align4 (data, dstoff);
        for (uint32_t i = 0; i < cnt; i++)
          p[i] = ddsrt_toBO4(bo, x[i]);
        *dstoff += cnt * sizeof (*x);
        srcoff += cnt * sizeof (*x);
        break;
      }
      case Xu: case Xux2: case Xux3: case Xux4: case Xux5:  { /* uint32_t(s) */
        uint32_t const * const x = deser_generic_src (src, &srcoff, plist_alignof (uint32_t));
        const uint32_t cnt = 1 + (uint32_t) (*desc - Xu);
        uint32_t * const p = ser_generic_align4 (data, dstoff);
        for (uint32_t i = 0; i < cnt; i++)
          p[i] = ddsrt_toBO4u(bo, x[i]);
        *dstoff += cnt * sizeof (*x);
        srcoff += cnt * sizeof (*x);
        break;
      }
      case Xl: { /* int64_t */
        int64_t const * const x = deser_generic_src (src, &srcoff, plist_alignof (int64_t));
        int64_t * const p = ser_generic_align8 (data, dstoff);
        *p = ddsrt_toBO8(bo, *x);
        *dstoff += sizeof (*x);
        srcoff += sizeof (*x);
        break;
      }
      case XD: case XDx2: { /* duration(s): int64_t <=> int32_t.uint32_t (seconds.fraction) */
        dds_duration_t const * const x = deser_generic_src (src, &srcoff, plist_alignof (dds_duration_t));
        const uint32_t cnt = 1 + (uint32_t) (*desc - XD);
        uint32_t * const p = ser_generic_align4 (data, dstoff);
        for (uint32_t i = 0; i < cnt; i++)
        {
          ddsi_duration_t tmp = ddsi_duration_from_dds (x[i]);
          p[2 * i + 0] = ddsrt_toBO4u(bo, (uint32_t) tmp.seconds);
          p[2 * i + 1] = ddsrt_toBO4u(bo, tmp.fraction);
        }
        *dstoff += 2 * cnt * sizeof (uint32_t);
        srcoff += cnt * sizeof (*x);
        break;
      }
      case Xo: case Xox2: { /* octet(s) */
        unsigned char const * const x = deser_generic_src (src, &srcoff, plist_alignof (unsigned char));
        const uint32_t cnt = 1 + (uint32_t) (*desc - Xo);
        char * const p = data + *dstoff;
        memcpy (p, x, cnt);
        *dstoff += cnt;
        srcoff += cnt * sizeof (*x);
        break;
      }
      case Xb: case Xbx2: case Xbx3: case Xbx4: case Xbx5: case XbCOND: { /* boolean(s) */
        unsigned char const * const x = deser_generic_src (src, &srcoff, plist_alignof (unsigned char));
        const uint32_t cnt = 1 + ((*desc == XbCOND) ? 0 : (uint32_t) (*desc - Xb));
        char * const p = data + *dstoff;
        memcpy (p, x, cnt);
        *dstoff += cnt;
        srcoff += cnt * sizeof (*x);
        break;
      }
      case XbPROP: { /* "propagate" boolean: don't serialize, skip it and everything that follows if false */
        unsigned char const * const x = deser_generic_src (src, &srcoff, plist_alignof (unsigned char));
        if (! *x) return 0;
        srcoff++;
        break;
      }
      case XG: { /* GUID */
        ddsi_guid_t const * const x = deser_generic_src (src, &srcoff, plist_alignof (ddsi_guid_t));
        const ddsi_guid_t xn = ddsi_hton_guid (*x);
        char * const p = data + *dstoff;
        memcpy (p, &xn, sizeof (xn));
        *dstoff += sizeof (xn);
        srcoff += sizeof (*x);
        break;
      }
      case XK: { /* keyhash */
        ddsi_keyhash_t const * const x = deser_generic_src (src, &srcoff, plist_alignof (ddsi_keyhash_t));
        char * const p = data + *dstoff;
        memcpy (p, x, sizeof (*x));
        *dstoff += sizeof (*x);
        srcoff += sizeof (*x);
        break;
      }
      case XQ: { /* arbitrary sequence */
        ddsi_octetseq_t const * const x = deser_generic_src (src, &srcoff, plist_alignof (ddsi_octetseq_t));
        char * const p = ser_generic_align4 (data, dstoff);
        *dstoff += 4;
        if (x->length == 0)
          *((uint32_t *) p) = 0;
        else
        {
          const size_t elem_size = ser_generic_srcsize (desc + 1);
          *((uint32_t *) p) = ddsrt_toBO4u(bo, ser_generic_count (x, elem_size, desc + 1));
          for (uint32_t i = 0; i < x->length; i++)
            ddsi_plist_ser_generic_embeddable (data, dstoff, x->value, i * elem_size, desc + 1, bo);
        }
        srcoff += sizeof (*x);
        break;
      }
      case Xopt:
        break;
    }
    desc = pserop_advance(desc);
  }
}



static dds_return_t ser_generic (struct ddsi_xmsg *xmsg, ddsi_parameterid_t pid, const void *src, size_t srcoff, const enum ddsi_pserop * __restrict desc, enum ddsrt_byte_order_selector bo)
{
  char * const data = ddsi_xmsg_addpar_bo (xmsg, pid, ser_generic_size (src, srcoff, desc), bo);
  size_t dstoff = 0;
  return ddsi_plist_ser_generic_embeddable (data, &dstoff, src, srcoff, desc, bo);
}

dds_return_t ddsi_plist_ser_generic (void **dst, size_t *dstsize, const void *src, const enum ddsi_pserop * __restrict desc)
{
  const size_t srcoff = 0;
  size_t dstoff = 0;
  dds_return_t ret;
  *dstsize = ser_generic_size (src, srcoff, desc);
  if ((*dst = ddsrt_malloc (*dstsize == 0 ? 1 : *dstsize)) == NULL)
    return DDS_RETCODE_OUT_OF_RESOURCES;
  ret = ddsi_plist_ser_generic_embeddable (*dst, &dstoff, src, srcoff, desc, DDSRT_BOSEL_NATIVE);
  assert (dstoff == *dstsize);
  return ret;
}

dds_return_t ddsi_plist_ser_generic_be (void **dst, size_t *dstsize, const void *src, const enum ddsi_pserop * __restrict desc)
{
  const size_t srcoff = 0;
  size_t dstoff = 0;
  dds_return_t ret;
  *dstsize = ser_generic_size (src, srcoff, desc);
  if ((*dst = ddsrt_malloc (*dstsize == 0 ? 1 : *dstsize)) == NULL)
    return DDS_RETCODE_OUT_OF_RESOURCES;
  ret = ddsi_plist_ser_generic_embeddable (*dst, &dstoff, src, srcoff, desc, DDSRT_BOSEL_BE);
  assert (dstoff == *dstsize);
  return ret;
}

static dds_return_t unalias_generic (void * __restrict dst, size_t * __restrict dstoff, bool gen_seq_aliased, const enum ddsi_pserop * __restrict desc)
{
#define COMPLEX(basecase_, type_, ...) do {                            \
    type_ *x = deser_generic_dst (dst, dstoff, plist_alignof (type_)); \
    const uint32_t cnt = 1 + (uint32_t) (*desc - basecase_);           \
    for (uint32_t xi = 0; xi < cnt; xi++, x++) { __VA_ARGS__; }        \
    *dstoff += cnt * sizeof (*x);                                      \
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
      case XE1: case XE2: case XE3: COMPLEX (*desc, enum xe3_prototype, (void) 0); break;
      case Xs: SIMPLE (Xs, int16_t); break;
      case Xi: case Xix2: case Xix3: case Xix4: SIMPLE (Xi, int32_t); break;
      case Xu: case Xux2: case Xux3: case Xux4: case Xux5: SIMPLE (Xu, uint32_t); break;
      case Xl: SIMPLE(Xl, int64_t); break;
      case XD: case XDx2: SIMPLE (XD, dds_duration_t); break;
      case Xo: case Xox2: SIMPLE (Xo, unsigned char); break;
      case Xb: case Xbx2: case Xbx3: case Xbx4: case Xbx5: SIMPLE (Xb, unsigned char); break;
      case XbCOND: SIMPLE (XbCOND, unsigned char); break;
      case XbPROP: SIMPLE (XbPROP, unsigned char); break;
      case XG: SIMPLE (XG, ddsi_guid_t); break;
      case XK: SIMPLE (XK, ddsi_keyhash_t); break;
      case XQ: COMPLEX (XQ, ddsi_octetseq_t, if (x->length) {
        const size_t elem_size = ser_generic_srcsize (desc + 1);
        if (gen_seq_aliased)
        {
          /* The memory for the elements of a generic sequence are owned by the plist, the only aliased bits
             are the strings (XS) and octet sequences (XO) embedded in the elements.  So in principle, an
             unalias operation on a generic sequence should only operate on the elements of the sequence,
             not on the sequence buffer itself.

             However, the "mergein_missing" operation (and consequently the copy & dup operations) memcpy the
             source, then pretend it is aliased.  In this case, the sequence buffer is aliased, rather than
             private, and hence a new copy needs to be allocated. */
          x->value = ddsrt_memdup (x->value, x->length * elem_size);
        }
        for (uint32_t i = 0; i < x->length; i++) {
          size_t elem_off = i * elem_size;
          unalias_generic (x->value, &elem_off, gen_seq_aliased, desc + 1);
        }
      }); break;
      case Xopt: break;
    }
    desc = pserop_advance(desc);
  }
#undef SIMPLE
#undef COMPLEX
}

dds_return_t ddsi_plist_unalias_generic (void * __restrict dst, const enum ddsi_pserop * __restrict desc)
{
  size_t dstoff = 0;
  return unalias_generic (dst, &dstoff, false, desc);
}

static bool unalias_generic_required (const enum ddsi_pserop * __restrict desc)
{
  while (*desc != XSTOP)
  {
    switch (*desc++)
    {
      case XO: case XS: case XQ:
        return true;
      default:
        break;
    }
  }
  return false;
}

static bool fini_generic_required (const enum ddsi_pserop * __restrict desc)
{
  /* the two happen to be the same */
  return unalias_generic_required (desc);
}

static dds_return_t fini_generic (void * __restrict dst, size_t * __restrict dstoff, struct flagset *flagset, uint64_t flag, const enum ddsi_pserop * __restrict desc)
{
  (void)fini_generic_embeddable (dst, dstoff, desc, NULL, *flagset->aliased & flag);
  return 0;
}

void ddsi_plist_fini_generic (void * __restrict dst, const enum ddsi_pserop *desc, bool aliased)
{
  size_t dstoff = 0;
  (void)fini_generic_embeddable (dst, &dstoff, desc, NULL, aliased);
}

static dds_return_t valid_generic (const void *src, size_t srcoff, const enum ddsi_pserop * __restrict desc)
{
#define COMPLEX(basecase_, type_, cond_stmts_) do {                           \
    type_ const *x = deser_generic_src (src, &srcoff, plist_alignof (type_)); \
    const uint32_t cnt = 1 + (uint32_t) (*desc - (basecase_));                \
    for (uint32_t xi = 0; xi < cnt; xi++, x++) { cond_stmts_; }               \
    srcoff += cnt * sizeof (*x);                                              \
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
      case XE1: case XE2: case XE3: SIMPLE (*desc, enum xe3_prototype, (uint32_t) *x <= 1 + (uint32_t) *desc - XE1); break;
      case Xs: TRIVIAL (Xs, int16_t); break;
      case Xi: case Xix2: case Xix3: case Xix4: TRIVIAL (Xi, int32_t); break;
      case Xu: case Xux2: case Xux3: case Xux4: case Xux5: TRIVIAL (Xu, uint32_t); break;
      case Xl: TRIVIAL(Xl, int64_t); break;
      case XD: case XDx2: SIMPLE (XD, dds_duration_t, *x >= 0); break;
      case Xo: case Xox2: TRIVIAL (Xo, unsigned char); break;
      case Xb: case Xbx2: case Xbx3: case Xbx4: case Xbx5: SIMPLE (Xb, unsigned char, *x == 0 || *x == 1); break;
      case XbCOND: SIMPLE (XbCOND, unsigned char, *x == 0 || *x == 1); break;
      case XbPROP: SIMPLE (XbPROP, unsigned char, *x == 0 || *x == 1); break;
      case XG: TRIVIAL (XG, ddsi_guid_t); break;
      case XK: TRIVIAL (XK, ddsi_keyhash_t); break;
      case XQ: COMPLEX (XQ, ddsi_octetseq_t, {
        if ((x->length == 0) != (x->value == NULL))
          return DDS_RETCODE_BAD_PARAMETER;
        if (x->length) {
          const size_t elem_size = ser_generic_srcsize (desc + 1);
          dds_return_t ret;
          for (uint32_t i = 0; i < x->length; i++) {
            if ((ret = valid_generic (x->value, i * elem_size, desc + 1)) != 0)
              return ret;
          }
        }
      }); break;
      case Xopt: break;
    }
    desc = pserop_advance(desc);
  }
#undef TRIVIAL
#undef SIMPLE
#undef COMPLEX
}

static bool equal_generic (const void *srcx, const void *srcy, size_t srcoff, const enum ddsi_pserop * __restrict desc)
{
#define COMPLEX(basecase_, type_, cond_stmts_) do {                            \
    type_ const *x = deser_generic_src (srcx, &srcoff, plist_alignof (type_)); \
    type_ const *y = deser_generic_src (srcy, &srcoff, plist_alignof (type_)); \
    const uint32_t cnt = 1 + (uint32_t) (*desc - (basecase_));                 \
    for (uint32_t xi = 0; xi < cnt; xi++, x++, y++) { cond_stmts_; }           \
    srcoff += cnt * sizeof (*x);                                               \
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
      case XE1: case XE2: case XE3: TRIVIAL (*desc, enum xe3_prototype); break;
      case Xs: TRIVIAL (Xs, int16_t); break;
      case Xi: case Xix2: case Xix3: case Xix4: TRIVIAL (Xi, int32_t); break;
      case Xu: case Xux2: case Xux3: case Xux4: case Xux5: TRIVIAL (Xu, uint32_t); break;
      case Xl: TRIVIAL (Xl, int64_t); break;
      case XD: case XDx2: TRIVIAL (XD, dds_duration_t); break;
      case Xo: case Xox2: TRIVIAL (Xo, unsigned char); break;
      case Xb: case Xbx2: case Xbx3: case Xbx4: case Xbx5: TRIVIAL (Xb, unsigned char); break;
      case XbCOND:
        COMPLEX (XbCOND, unsigned char, {
          if (*x != *y)
            return false;
          if (*x == false)
            return true;
        });
        break;
      case XbPROP: TRIVIAL (XbPROP, unsigned char); break;
      case XG: SIMPLE (XG, ddsi_guid_t, memcmp (x, y, sizeof (*x)) == 0); break;
      case XK: SIMPLE (XK, ddsi_keyhash_t, memcmp (x, y, sizeof (*x)) == 0); break;
      case XQ: COMPLEX (XQ, ddsi_octetseq_t, {
        if (x->length != y->length)
          return false;
        if (x->length) {
          const size_t elem_size = ser_generic_srcsize (desc + 1);
          for (uint32_t i = 0; i < x->length; i++) {
            if (!equal_generic (x->value, y->value, i * elem_size, desc + 1))
              return false;
          }
        }
      }); break;
      case Xopt: break;
    }
    desc = pserop_advance(desc);
  }
#undef TRIVIAL
#undef SIMPLE
#undef COMPLEX
}

bool ddsi_plist_equal_generic (const void *srcx, const void *srcy, const enum ddsi_pserop * __restrict desc)
{
  return equal_generic (srcx, srcy, 0, desc);
}

static uint32_t isprint_runlen (uint32_t n, const unsigned char *xs)
{
  uint32_t m;
  for (m = 0; m < n && xs[m] != '"' && isprint (xs[m]) && xs[m] < 127; m++)
    ;
  return m;
}

static bool prtf_octetseq (char * __restrict *buf, size_t * __restrict bufsize, uint32_t n, const unsigned char *xs)
{
  /* Truncate octet sequences: 100 is arbitrary but experience suggests it is
     usually enough, and truncating it helps a lot when printing handshake
     messages during authentication. */
  const uint32_t lim = 100;
  bool trunc = (n > lim);
  uint32_t i = 0;
  if (trunc)
    n = lim;
  while (i < n)
  {
    uint32_t m = isprint_runlen (n - i, xs);
    if (m >= 4 || (i == 0 && m == n))
    {
      // m <= lim, so casting to int is safe
      if (!prtf (buf, bufsize, "%s\"%*.*s\"", i == 0 ? "" : ",", (int) m, (int) m, xs))
        return false;
      xs += m;
      i += m;
    }
    else
    {
      if (m == 0)
        m = 1;
      while (m--)
      {
        if (!prtf (buf, bufsize, "%s%u", i == 0 ? "" : ",", *xs++))
          return false;
        i++;
      }
    }
  }
  return trunc ? prtf (buf, bufsize, "...") : true;
}

static bool print_generic1 (char * __restrict *buf, size_t * __restrict bufsize, const void *src, size_t srcoff, const enum ddsi_pserop * __restrict desc, const char *sep)
{
  while (true)
  {
    switch (*desc)
    {
      case XSTOP:
        return true;
      case XO: { /* octet sequence */
        ddsi_octetseq_t const * const x = deser_generic_src (src, &srcoff, plist_alignof (ddsi_octetseq_t));
        prtf (buf, bufsize, "%s%"PRIu32"<", sep, x->length);
        (void) prtf_octetseq (buf, bufsize, x->length, x->value);
        if (!prtf (buf, bufsize, ">"))
          return false;
        srcoff += sizeof (*x);
        break;
      }
      case XS: { /* string */
        char const * const * const x = deser_generic_src (src, &srcoff, plist_alignof (char *));
        if (!prtf (buf, bufsize, "%s\"%s\"", sep, *x))
          return false;
        srcoff += sizeof (*x);
        break;
      }
      case XE1: case XE2: case XE3: { /* enum */
        enum xe3_prototype const * const x = deser_generic_src (src, &srcoff, plist_alignof (enum xe3_prototype));
        if (!prtf (buf, bufsize, "%s%"PRIu32, sep, *x))
          return false;
        srcoff += sizeof (*x);
        break;
      }
      case Xs: {
        int16_t const * const x = deser_generic_src (src, &srcoff, plist_alignof (int16_t));
        if (!prtf (buf, bufsize, "%s%"PRId16, sep, *x))
          return false;
        srcoff += sizeof (*x);
        break;
      }
      case Xi: case Xix2: case Xix3: case Xix4: { /* int32_t(s) */
        int32_t const * const x = deser_generic_src (src, &srcoff, plist_alignof (int32_t));
        const uint32_t cnt = 1 + (uint32_t) (*desc - Xi);
        for (uint32_t i = 0; i < cnt; i++)
        {
          if (!prtf (buf, bufsize, "%s%"PRId32, sep, x[i]))
            return false;
          sep = ":";
        }
        srcoff += cnt * sizeof (*x);
        break;
      }
      case Xu: case Xux2: case Xux3: case Xux4: case Xux5:  { /* uint32_t(s) */
        uint32_t const * const x = deser_generic_src (src, &srcoff, plist_alignof (uint32_t));
        const uint32_t cnt = 1 + (uint32_t) (*desc - Xu);
        for (uint32_t i = 0; i < cnt; i++)
        {
          if (!prtf (buf, bufsize, "%s%"PRIu32, sep, x[i]))
            return false;
          sep = ":";
        }
        srcoff += cnt * sizeof (*x);
        break;
      }
      case Xl: {
        int64_t const * const x = deser_generic_src (src, &srcoff, plist_alignof (int64_t));
        const uint32_t cnt = 1 + (uint32_t) (*desc - Xl);
        for (uint32_t i = 0; i < cnt; i++)
        {
          if (!prtf (buf, bufsize, "%s%"PRId64, sep, x[i]))
            return false;
          sep = ":";
        }
        srcoff += cnt * sizeof (*x);
        break;
      }
      case XD: case XDx2: { /* duration(s): int64_t <=> int32_t.uint32_t (seconds.fraction) */
        dds_duration_t const * const x = deser_generic_src (src, &srcoff, plist_alignof (dds_duration_t));
        const uint32_t cnt = 1 + (uint32_t) (*desc - XD);
        for (uint32_t i = 0; i < cnt; i++)
        {
          if (!prtf (buf, bufsize, "%s%"PRId64, sep, x[i]))
            return false;
          sep = ":";
        }
        srcoff += cnt * sizeof (*x);
        break;
      }
      case Xo: case Xox2: { /* octet(s) */
        unsigned char const * const x = deser_generic_src (src, &srcoff, plist_alignof (unsigned char));
        const uint32_t cnt = 1 + (uint32_t) (*desc - Xo);
        for (uint32_t i = 0; i < cnt; i++)
        {
          if (!prtf (buf, bufsize, "%s%d", sep, x[i]))
            return false;
          sep = ":";
        }
        srcoff += cnt * sizeof (*x);
        break;
      }
      case Xb: case Xbx2: case Xbx3: case Xbx4: case Xbx5: case XbCOND: { /* boolean(s) */
        unsigned char const * const x = deser_generic_src (src, &srcoff, plist_alignof (unsigned char));
        const uint32_t cnt = 1 + ((*desc == XbCOND) ? 0 : (uint32_t) (*desc - Xb));
        for (uint32_t i = 0; i < cnt; i++)
        {
          if (!prtf (buf, bufsize, "%s%d", sep, x[i]))
            return false;
          sep = ":";
        }
        srcoff += cnt * sizeof (*x);
        break;
      }
      case XbPROP: { /* "propagate" boolean: don't serialize, skip it and everything that follows if false */
        unsigned char const * const x = deser_generic_src (src, &srcoff, plist_alignof (unsigned char));
        if (!prtf (buf, bufsize, "%s%d", sep, *x))
          return false;
        srcoff++;
        break;
      }
      case XG: { /* GUID */
        ddsi_guid_t const * const x = deser_generic_src (src, &srcoff, plist_alignof (ddsi_guid_t));
        if (!prtf (buf, bufsize, "%s{"PGUIDFMT"}", sep, PGUID (*x)))
          return false;
        srcoff += sizeof (*x);
        break;
      }
      case XK: { /* keyhash */
        ddsi_keyhash_t const * const x = deser_generic_src (src, &srcoff, plist_alignof (ddsi_keyhash_t));
        if (!prtf (buf, bufsize, "%s{%02x%02x%02x%02x:%02x%02x%02x%02x:%02x%02x%02x%02x:%02x%02x%02x%02x}", sep,
                   x->value[0], x->value[1], x->value[2], x->value[3], x->value[4], x->value[5], x->value[6], x->value[7],
                   x->value[8], x->value[9], x->value[10], x->value[11], x->value[12], x->value[13], x->value[14], x->value[15]))
          return false;
        srcoff += sizeof (*x);
        break;
      }
      case XQ: {
        ddsi_octetseq_t const * const x = deser_generic_src (src, &srcoff, plist_alignof (ddsi_octetseq_t));
        if (!prtf (buf, bufsize, "%s{", sep))
          return false;
        if (x->length > 0)
        {
          const size_t elem_size = ser_generic_srcsize (desc + 1);
          for (uint32_t i = 0; i < x->length; i++)
            if (!print_generic1 (buf, bufsize, x->value, i * elem_size, desc + 1, (i == 0) ? "" : ","))
              return false;
        }
        if (!prtf (buf, bufsize, "}"))
          return false;
        srcoff += sizeof (*x);
        break;
      }
      case Xopt:
        break;
    }
    sep = ":";
    desc = pserop_advance(desc);
  }
}

static bool print_generic (char * __restrict *buf, size_t * __restrict bufsize, const void *src, size_t srcoff, const enum ddsi_pserop * __restrict desc)
{
  return print_generic1 (buf, bufsize, src, srcoff, desc, "");
}

#define membersize(type, member) sizeof (((type *) 0)->member)
#define ENTRY(PFX_, NAME_, member_, flag_, validate_, ...)                                 \
  { DDSI_PID_##NAME_, flag_, PFX_##_##NAME_, #NAME_, offsetof (struct ddsi_plist, member_),     \
    membersize (struct ddsi_plist, member_), { .desc = { __VA_ARGS__, XSTOP } }, validate_ \
  }
#define QPV(NAME_, name_, ...) ENTRY(DDSI_QP, NAME_, qos.name_, PDF_QOS, dvx_##name_, __VA_ARGS__)
#define PPV(NAME_, name_, ...) ENTRY(PP, NAME_, name_, 0, dvx_##name_, __VA_ARGS__)
#define QP(NAME_, name_, ...) ENTRY(DDSI_QP, NAME_, qos.name_, PDF_QOS, 0, __VA_ARGS__)
#define PP(NAME_, name_, ...) ENTRY(PP, NAME_, name_, 0, 0, __VA_ARGS__)
#define PPM(NAME_, name_, ...) ENTRY(PP, NAME_, name_, PDF_ALLOWMULTI, 0, __VA_ARGS__)

static int protocol_version_is_newer (ddsi_protocol_version_t pv)
{
  return (pv.major < DDSI_RTPS_MAJOR) ? 0 : (pv.major > DDSI_RTPS_MAJOR) ? 1 : (pv.minor > DDSI_RTPS_MINOR);
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
  const ddsi_guid_t *g = dst;
  (void) dd;
  if (g->prefix.u[0] == 0 && g->prefix.u[1] == 0 && g->prefix.u[2] == 0)
    return (g->entityid.u == 0) ? 0 : DDS_RETCODE_BAD_PARAMETER;
  else
    return (g->entityid.u == DDSI_ENTITYID_PARTICIPANT) ? 0 : DDS_RETCODE_BAD_PARAMETER;
}

static dds_return_t dvx_group_guid (void * __restrict dst, const struct dd * __restrict dd)
{
  const ddsi_guid_t *g = dst;
  (void) dd;
  if (g->prefix.u[0] == 0 && g->prefix.u[1] == 0 && g->prefix.u[2] == 0)
    return (g->entityid.u == 0) ? 0 : DDS_RETCODE_BAD_PARAMETER;
  else
    return (g->entityid.u != 0) ? 0 : DDS_RETCODE_BAD_PARAMETER;
}

static dds_return_t dvx_endpoint_guid (void * __restrict dst, const struct dd * __restrict dd)
{
  ddsi_guid_t *g = dst;
  if (g->prefix.u[0] == 0 && g->prefix.u[1] == 0 && g->prefix.u[2] == 0)
    return (g->entityid.u == 0) ? 0 : DDS_RETCODE_BAD_PARAMETER;
  switch (g->entityid.u & DDSI_ENTITYID_KIND_MASK)
  {
    case DDSI_ENTITYID_KIND_WRITER_WITH_KEY:
    case DDSI_ENTITYID_KIND_WRITER_NO_KEY:
    case DDSI_ENTITYID_KIND_READER_NO_KEY:
    case DDSI_ENTITYID_KIND_READER_WITH_KEY:
      return 0;
    default:
      return (protocol_version_is_newer (dd->protocol_version) ? 0 : DDS_RETCODE_BAD_PARAMETER);
  }
}

#ifdef DDS_HAS_SSM
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

/* Standardized parameters -- QoS _MUST_ come first (ddsi_plist_init_tables verifies this) because
   it allows early-out when processing a dds_qos_t instead of an ddsi_plist_t */
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
  //QP  (LIVELINESS,                          liveliness, XE2, XD),
  { DDSI_PID_LIVELINESS, PDF_QOS | PDF_FUNCTION, DDSI_QP_LIVELINESS, "LIVELINESS",
    offsetof (struct ddsi_plist, qos.liveliness), membersize (struct ddsi_plist, qos.liveliness),
    { .f = { .deser = deser_liveliness, .ser = ser_liveliness, .valid = valid_liveliness, .equal = equal_liveliness, .print = print_liveliness } }, 0 },
  /* Property list used to be of type [(String,String]), security changed into ([String,String],Maybe [(String,[Word8])]),
     the "Xopt" here is to allow both forms on input, with an assumed empty second sequence if the old form was received */
  QP  (PROPERTY_LIST,                       property, XQ, XbPROP, XS, XS, XSTOP, Xopt, XQ, XbPROP, XS, XO, XSTOP),
  /* Reliability encoding does not follow the rules (best-effort/reliable map to 1/2 instead of 0/1 */
  { DDSI_PID_RELIABILITY, PDF_QOS | PDF_FUNCTION, DDSI_QP_RELIABILITY, "RELIABILITY",
    offsetof (struct ddsi_plist, qos.reliability), membersize (struct ddsi_plist, qos.reliability),
    { .f = { .deser = deser_reliability, .ser = ser_reliability, .valid = valid_reliability, .equal = equal_reliability, .print = print_reliability } }, 0 },
  QP  (LIFESPAN,                            lifespan, XD),
  QP  (DESTINATION_ORDER,                   destination_order, XE1),
  /* History depth is ignored when kind = KEEP_ALL, and must be >= 1 when KEEP_LAST, so can't use "l" */
  QPV (HISTORY,                             history, XE1, Xi),
  QPV (RESOURCE_LIMITS,                     resource_limits, Xix3),
  QP  (OWNERSHIP,                           ownership, XE1),
  QP  (OWNERSHIP_STRENGTH,                  ownership_strength, Xi),
  QP  (PRESENTATION,                        presentation, XE2, Xbx2),
  QP  (PARTITION,                           partition, XQ, XS, XSTOP),
  QP  (TIME_BASED_FILTER,                   time_based_filter, XD),
  QP  (TRANSPORT_PRIORITY,                  transport_priority, Xi),
  QP  (ENTITY_NAME,                         entity_name, XS),
  /* Type consistency enforcement has some custom validations and uses a bitbound(16) enum */
  { DDSI_PID_TYPE_CONSISTENCY_ENFORCEMENT, PDF_QOS | PDF_FUNCTION, DDSI_QP_TYPE_CONSISTENCY_ENFORCEMENT, "TYPE_CONSISTENCY_ENFORCEMENT",
    offsetof (struct ddsi_plist, qos.type_consistency), membersize (struct ddsi_plist, qos.type_consistency),
    { .f = { .deser = deser_type_consistency, .ser = ser_type_consistency, .valid = valid_type_consistency, .equal = equal_type_consistency, .print = print_type_consistency } }, 0 },
  { DDSI_PID_DATA_REPRESENTATION, PDF_QOS | PDF_FUNCTION, DDSI_QP_DATA_REPRESENTATION, "DATA_REPRESENTATION",
    offsetof (struct ddsi_plist, qos.data_representation), membersize (struct ddsi_plist, qos.data_representation),
    { .f = { .deser = deser_data_representation, .ser = ser_data_representation, .valid = valid_data_representation, .equal = equal_data_representation, .unalias = unalias_data_representation, .fini = fini_data_representation, .print = print_data_representation } }, 0 },
#ifdef DDS_HAS_TYPE_DISCOVERY
  { DDSI_PID_TYPE_INFORMATION, PDF_QOS | PDF_FUNCTION, DDSI_QP_TYPE_INFORMATION, "TYPE_INFORMATION",
    offsetof (struct ddsi_plist, qos.type_information), membersize (struct ddsi_plist, qos.type_information),
    { .f = { .deser = deser_type_information, .ser = ser_type_information, .valid = valid_type_information, .unalias = unalias_type_information, .fini = fini_type_information, .equal = equal_type_information, .print = print_type_information } }, 0 },
#endif
  PP  (PROTOCOL_VERSION,                    protocol_version, Xox2),
  PP  (VENDORID,                            vendorid, Xox2),
  PP  (EXPECTS_INLINE_QOS,                  expects_inline_qos, Xb),
  PP  (PARTICIPANT_MANUAL_LIVELINESS_COUNT, participant_manual_liveliness_count, Xi),
  PP  (PARTICIPANT_BUILTIN_ENDPOINTS,       participant_builtin_endpoints, Xu),
  PPV (PARTICIPANT_GUID,                    participant_guid, XG),
  PPV (GROUP_GUID,                          group_guid, XG),
  PP  (BUILTIN_ENDPOINT_SET,                builtin_endpoint_set, Xu),
  PP  (KEYHASH,                             keyhash, XK),
  PPV (ENDPOINT_GUID,                       endpoint_guid, XG),
#ifdef DDS_HAS_SSM
  PPV (READER_FAVOURS_SSM,                  reader_favours_ssm, Xu),
#endif
#ifdef DDS_HAS_SECURITY
  PP  (ENDPOINT_SECURITY_INFO,              endpoint_security_info, Xu, Xu),
  PP  (PARTICIPANT_SECURITY_INFO,           participant_security_info, Xu, Xu),
  PP  (IDENTITY_TOKEN,                      identity_token,        XS, XQ, XbPROP, XS, XS, XSTOP, XQ, XbPROP, XS, XO, XSTOP),
  PP  (PERMISSIONS_TOKEN,                   permissions_token,     XS, XQ, XbPROP, XS, XS, XSTOP, XQ, XbPROP, XS, XO, XSTOP),
  PP  (IDENTITY_STATUS_TOKEN,               identity_status_token, XS, XQ, XbPROP, XS, XS, XSTOP, XQ, XbPROP, XS, XO, XSTOP),
  PP  (DATA_TAGS,                           data_tags, XQ, XS, XS, XSTOP),
#endif
  PP  (DOMAIN_ID,                           domain_id, Xu),
  PP  (DOMAIN_TAG,                          domain_tag, XS),
  { DDSI_PID_STATUSINFO, PDF_FUNCTION, PP_STATUSINFO, "STATUSINFO",
    offsetof (struct ddsi_plist, statusinfo), membersize (struct ddsi_plist, statusinfo),
    { .f = { .deser = deser_statusinfo, .ser = ser_statusinfo, .print = print_statusinfo } }, 0 },
  /* Locators are difficult to deal with because they can occur multi times to represent a set;
     that is manageable for deser, unalias and fini, but it breaks ser because that one only
     generates a single parameter header */
  { DDSI_PID_UNICAST_LOCATOR, PDF_FUNCTION | PDF_ALLOWMULTI,
    PP_UNICAST_LOCATOR, "UNICAST_LOCATOR",
    offsetof (struct ddsi_plist, unicast_locators), membersize (struct ddsi_plist, unicast_locators),
    { .f = { .deser = deser_locator, .ser = ser_locator, .unalias = unalias_locator, .fini = fini_locator, .print = print_locator } }, 0 },
  { DDSI_PID_MULTICAST_LOCATOR, PDF_FUNCTION | PDF_ALLOWMULTI,
    PP_MULTICAST_LOCATOR, "MULTICAST_LOCATOR",
    offsetof (struct ddsi_plist, multicast_locators), membersize (struct ddsi_plist, multicast_locators),
    { .f = { .deser = deser_locator, .ser = ser_locator, .unalias = unalias_locator, .fini = fini_locator, .print = print_locator } }, 0 },
  { DDSI_PID_DEFAULT_UNICAST_LOCATOR, PDF_FUNCTION | PDF_ALLOWMULTI,
    PP_DEFAULT_UNICAST_LOCATOR, "DEFAULT_UNICAST_LOCATOR",
    offsetof (struct ddsi_plist, default_unicast_locators), membersize (struct ddsi_plist, default_unicast_locators),
    { .f = { .deser = deser_locator, .ser = ser_locator, .unalias = unalias_locator, .fini = fini_locator, .print = print_locator } }, 0 },
  { DDSI_PID_DEFAULT_MULTICAST_LOCATOR, PDF_FUNCTION | PDF_ALLOWMULTI,
    PP_DEFAULT_MULTICAST_LOCATOR, "DEFAULT_MULTICAST_LOCATOR",
    offsetof (struct ddsi_plist, default_multicast_locators), membersize (struct ddsi_plist, default_multicast_locators),
    { .f = { .deser = deser_locator, .ser = ser_locator, .unalias = unalias_locator, .fini = fini_locator, .print = print_locator } }, 0 },
  { DDSI_PID_METATRAFFIC_UNICAST_LOCATOR, PDF_FUNCTION | PDF_ALLOWMULTI,
    PP_METATRAFFIC_UNICAST_LOCATOR, "METATRAFFIC_UNICAST_LOCATOR",
    offsetof (struct ddsi_plist, metatraffic_unicast_locators), membersize (struct ddsi_plist, metatraffic_unicast_locators),
    { .f = { .deser = deser_locator, .ser = ser_locator, .unalias = unalias_locator, .fini = fini_locator, .print = print_locator } }, 0 },
  { DDSI_PID_METATRAFFIC_MULTICAST_LOCATOR, PDF_FUNCTION | PDF_ALLOWMULTI,
    PP_METATRAFFIC_MULTICAST_LOCATOR, "METATRAFFIC_MULTICAST_LOCATOR",
    offsetof (struct ddsi_plist, metatraffic_multicast_locators), membersize (struct ddsi_plist, metatraffic_multicast_locators),
    { .f = { .deser = deser_locator, .ser = ser_locator, .unalias = unalias_locator, .fini = fini_locator, .print = print_locator } }, 0 },
  /* PID_..._{IPADDRESS,PORT} is impossible to deal with and are never generated, only accepted.
     The problem is that there one needs additional state (and even then there is no clear
     interpretation) ... So they'll have to be special-cased */
  { DDSI_PID_SENTINEL, 0, 0, NULL, 0, 0, { .desc = { XSTOP } }, 0 }
};

/* Understood parameters for Eclipse Foundation (Cyclone DDS) vendor code */
static const struct piddesc piddesc_eclipse[] = {
  QP  (ADLINK_ENTITY_FACTORY,            entity_factory, Xb),
  QP  (ADLINK_READER_LIFESPAN,           reader_lifespan, Xb, XD),
  QP  (ADLINK_WRITER_DATA_LIFECYCLE,     writer_data_lifecycle, Xb),
  QP  (ADLINK_READER_DATA_LIFECYCLE,     reader_data_lifecycle, XDx2),
  { DDSI_PID_PAD, PDF_QOS, DDSI_QP_CYCLONE_IGNORELOCAL, "CYCLONE_IGNORELOCAL",
    offsetof (struct ddsi_plist, qos.ignorelocal), membersize (struct ddsi_plist, qos.ignorelocal),
    { .desc = { XE2, XSTOP } }, 0 },
  { DDSI_PID_PAD, PDF_QOS, DDSI_QP_CYCLONE_WRITER_BATCHING, "CYCLONE_WRITER_BATCHING",
    offsetof (struct ddsi_plist, qos.writer_batching), membersize (struct ddsi_plist, qos.writer_batching),
    { .desc = { Xb, XSTOP } }, 0 },
  { DDSI_PID_PAD, PDF_QOS, DDSI_QP_LOCATOR_MASK, "CYCLONE_LOCATOR_MASK",
    offsetof(struct ddsi_plist, qos.ignore_locator_type), membersize(struct ddsi_plist, qos.ignore_locator_type),
    {.desc = { Xu, XSTOP } }, 0 },
#ifdef DDS_HAS_TOPIC_DISCOVERY
  PP  (CYCLONE_TOPIC_GUID,               topic_guid, XG),
#endif
  PP  (ADLINK_PARTICIPANT_VERSION_INFO,  adlink_participant_version_info, Xux5, XS),
  PP  (CYCLONE_RECEIVE_BUFFER_SIZE,      cyclone_receive_buffer_size, Xu),
  PP  (CYCLONE_REQUESTS_KEYHASH,         cyclone_requests_keyhash, Xb),
  PP  (CYCLONE_REDUNDANT_NETWORKING,     cyclone_redundant_networking, Xb),
  { DDSI_PID_SENTINEL, 0, 0, NULL, 0, 0, { .desc = { XSTOP } }, 0 }
};

/* Understood parameters for Adlink vendor code */
static const struct piddesc piddesc_adlink[] = {
  QP  (ADLINK_ENTITY_FACTORY,            entity_factory, Xb),
  QP  (ADLINK_READER_LIFESPAN,           reader_lifespan, Xb, XD),
  QP  (ADLINK_WRITER_DATA_LIFECYCLE,     writer_data_lifecycle, Xb),
  QP  (ADLINK_READER_DATA_LIFECYCLE,     reader_data_lifecycle, XDx2),
  PP  (ADLINK_PARTICIPANT_VERSION_INFO,  adlink_participant_version_info, Xux5, XS),
  { DDSI_PID_SENTINEL, 0, 0, NULL, 0, 0, { .desc = { XSTOP } }, 0 }
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
   ddsi_plist_init_tables.

   FIXME: should compute them at build-time */
#define DEFAULT_PROC_ARRAY_SIZE                20
#ifdef DDS_HAS_TYPE_DISCOVERY
#define DEFAULT_OMG_PIDS_ARRAY_SIZE            (DDSI_PID_TYPE_INFORMATION + 1)
#else
#define DEFAULT_OMG_PIDS_ARRAY_SIZE            (DDSI_PID_TYPE_CONSISTENCY_ENFORCEMENT + 1)
#endif
#ifdef DDS_HAS_SECURITY
#define SECURITY_OMG_PIDS_ARRAY_SIZE           (DDSI_PID_IDENTITY_STATUS_TOKEN - DDSI_PID_IDENTITY_TOKEN + 1)
#define SECURITY_PROC_ARRAY_SIZE               4
#else
#define SECURITY_OMG_PIDS_ARRAY_SIZE           0
#define SECURITY_PROC_ARRAY_SIZE               0
#endif

static const struct piddesc *piddesc_omg_index[DEFAULT_OMG_PIDS_ARRAY_SIZE + SECURITY_OMG_PIDS_ARRAY_SIZE];
static const struct piddesc *piddesc_eclipse_index[30];
static const struct piddesc *piddesc_adlink_index[17];

#define INDEX_ANY(vendorid_, tab_) [vendorid_] = { \
    .index_max = sizeof (piddesc_##tab_##_index) / sizeof (piddesc_##tab_##_index[0]) - 1, \
    .index = (const struct piddesc **) piddesc_##tab_##_index, \
    .table = piddesc_##tab_ }
#define INDEX(VENDOR_, tab_) INDEX_ANY (DDSI_VENDORID_MINOR_##VENDOR_, tab_)

static const struct piddesc_index piddesc_vendor_index[] = {
  INDEX_ANY (0, omg),
  INDEX (ECLIPSE, eclipse),
  INDEX (ADLINK_OSPL, adlink),
  INDEX (ADLINK_JAVA, adlink),
  INDEX (ADLINK_LITE, adlink),
  INDEX (ADLINK_GATEWAY, adlink),
  INDEX (ADLINK_CLOUD, adlink)
};

#undef INDEX
#undef INDEX_ANY

/* List of entries that require unalias, fini processing;
   initialized by ddsi_plist_init_tables; will assert when
   table too small or too large */
#ifdef DDS_HAS_TYPE_DISCOVERY
static const struct piddesc *piddesc_unalias[18 + SECURITY_PROC_ARRAY_SIZE];
static const struct piddesc *piddesc_fini[18 + SECURITY_PROC_ARRAY_SIZE];
#else
static const struct piddesc *piddesc_unalias[17 + SECURITY_PROC_ARRAY_SIZE];
static const struct piddesc *piddesc_fini[17 + SECURITY_PROC_ARRAY_SIZE];
#endif
static uint64_t plist_fini_mask, qos_fini_mask;
static ddsrt_once_t table_init_control = DDSRT_ONCE_INIT;

static size_t pid_to_index (ddsi_parameterid_t pid)
{
  /* pid without flags. */
  size_t idx = (size_t)(pid & ~(DDSI_PID_VENDORSPECIFIC_FLAG | DDSI_PID_UNRECOGNIZED_INCOMPATIBLE_FLAG));
#ifdef DDS_HAS_SECURITY
  if ((idx >= DDSI_PID_IDENTITY_TOKEN) && (idx <= DDSI_PID_IDENTITY_STATUS_TOKEN))
  {
    /* Security PIDs start after the 'normal' PIDs. */
    idx = (idx - DDSI_PID_IDENTITY_TOKEN) + DEFAULT_OMG_PIDS_ARRAY_SIZE;
  }
#endif
  return idx;
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

static void ddsi_plist_init_tables_real (void)
{
  /* make index of pid -> entry */
  for (size_t i = 0; i < sizeof (piddesc_vendor_index) / sizeof (piddesc_vendor_index[0]); i++)
  {
    const struct piddesc *table = piddesc_vendor_index[i].table;
    if (table == NULL)
      continue;
    struct piddesc const **index = piddesc_vendor_index[i].index;
#ifndef NDEBUG
    size_t maxpididx = 0;
    bool only_qos_seen = true;
#endif
    for (size_t j = 0; table[j].pid != DDSI_PID_SENTINEL; j++)
    {
      ddsi_parameterid_t pid = table[j].pid;
      size_t pididx = pid_to_index(pid);
#ifndef NDEBUG
      /* Table must first list QoS, then other parameters */
      assert (only_qos_seen || !(table[j].flags & PDF_QOS));
      if (!(table[j].flags & PDF_QOS))
        only_qos_seen = false;
      /* Track max PID so we can verify the table is no larger
         than necessary */
      if (pididx > maxpididx)
          maxpididx = pididx;
#endif
      /* PAD is used for entries that are never visible on the wire
         and the decoder assumes the PAD entries will be skipped
         because they don't map to an entry */
      if (pid == DDSI_PID_PAD)
        continue;
      assert (pididx <= piddesc_vendor_index[i].index_max);
      assert (index[pididx] == NULL || index[pididx] == &table[j]);
      index[pididx] = &table[j];
    }
    assert (maxpididx == piddesc_vendor_index[i].index_max);
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
    for (size_t j = 0; table[j].pid != DDSI_PID_SENTINEL; j++)
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
        if (table[j].flags & PDF_QOS)
          qos_fini_mask |= table[j].present_flag;
        else
          plist_fini_mask |= table[j].present_flag;
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

void ddsi_plist_init_tables (void)
{
  ddsrt_once (&table_init_control, ddsi_plist_init_tables_real);
}

static void plist_or_xqos_fini (void * __restrict dst, size_t shift, uint64_t pmask, uint64_t qmask)
{
  /* shift == 0: plist, shift > 0: just qos */
  struct flagset pfs, qfs;
  /* DDS manipulation can be done without creating a participant, so we may
     have to initialize tables just-in-time */
  if (piddesc_fini[0] == NULL)
    ddsi_plist_init_tables ();
  if (shift > 0)
  {
    dds_qos_t *qos = dst;
    if ((qos->present & qos_fini_mask) == 0)
      return;
    pfs = (struct flagset) { NULL, NULL, 0 };
    qfs = (struct flagset) { .present = &qos->present, .aliased = &qos->aliased };
  }
  else
  {
    ddsi_plist_t *plist = dst;
    if ((plist->present & plist_fini_mask) == 0 && (plist->qos.present & qos_fini_mask) == 0)
      return;
    pfs = (struct flagset) { .present = &plist->present, .aliased = &plist->aliased };
    qfs = (struct flagset) { .present = &plist->qos.present, .aliased = &plist->qos.aliased };
  }
  for (size_t i = 0; i < sizeof (piddesc_fini) / sizeof (piddesc_fini[0]); i++)
  {
    struct piddesc const * const entry = piddesc_fini[i];
    assert (entry);
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
}

static void plist_or_xqos_unalias (void * __restrict dst, size_t shift)
{
  /* shift == 0: plist, shift > 0: just qos */
  struct flagset pfs, qfs;
  /* DDS manipulation can be done without creating a participant, so we may
     have to initialize tables just-in-time */
  if (piddesc_unalias[0] == NULL)
    ddsi_plist_init_tables ();
  if (shift > 0)
  {
    dds_qos_t *qos = dst;
    pfs = (struct flagset) { NULL, NULL, 0 };
    qfs = (struct flagset) { .present = &qos->present, .aliased = &qos->aliased };
  }
  else
  {
    ddsi_plist_t *plist = dst;
    pfs = (struct flagset) { .present = &plist->present, .aliased = &plist->aliased };
    qfs = (struct flagset) { .present = &plist->qos.present, .aliased = &plist->qos.aliased };
  }
  for (size_t i = 0; i < sizeof (piddesc_unalias) / sizeof (piddesc_unalias[0]); i++)
  {
    struct piddesc const * const entry = piddesc_unalias[i];
    assert (entry);
    if (shift > 0 && !(entry->flags & PDF_QOS))
      break;
    assert (entry->plist_offset >= shift);
    assert (shift == 0 || entry->plist_offset - shift < sizeof (dds_qos_t));
    size_t dstoff = entry->plist_offset - shift;
    struct flagset * const fs = (entry->flags & PDF_QOS) ? &qfs : &pfs;
    if ((*fs->present & entry->present_flag) && (*fs->aliased & entry->present_flag))
    {
      if (!(entry->flags & PDF_FUNCTION))
        unalias_generic (dst, &dstoff, false, entry->op.desc);
      else if (entry->op.f.unalias)
        entry->op.f.unalias (dst, &dstoff, false);
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
  const uint64_t aliased_dst_inp = (shift == 0) ? ((ddsi_plist_t *) dst)->aliased : 0;
  const uint64_t aliased_dst_inq = (shift == 0) ? ((ddsi_plist_t *) dst)->qos.aliased : ((dds_qos_t *) dst)->aliased;
#endif
  if (shift > 0)
  {
    dds_qos_t *qos_dst = dst;
    const dds_qos_t *qos_src = src;
    pfs_dst = (struct flagset) { NULL, NULL, 0 };
    qfs_dst = (struct flagset) { .present = &qos_dst->present, .aliased = &qos_dst->aliased };
    pfs_src = (struct flagset) { NULL, NULL, 0 };
    qfs_src = (struct flagset) { .present = (uint64_t *) &qos_src->present, .aliased = (uint64_t *) &qos_src->aliased };
  }
  else
  {
    ddsi_plist_t *plist_dst = dst;
    const ddsi_plist_t *plist_src = src;
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
    for (uint32_t i = 0; table[i].pid != DDSI_PID_SENTINEL; i++)
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
          unalias_generic (dst, &dstoff, true, entry->op.desc);
        else if (entry->op.f.unalias)
          entry->op.f.unalias (dst, &dstoff, true);
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

static void plist_or_xqos_addtomsg (struct ddsi_xmsg *xmsg, const void * __restrict src, size_t shift, uint64_t pwanted, uint64_t qwanted, enum ddsrt_byte_order_selector bo, enum ddsi_plist_context_kind context_kind)
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
    const ddsi_plist_t *plist = src;
    pw = plist->present & pwanted;
    qw = plist->qos.present & qwanted;
  }
  for (size_t k = 0; k < sizeof (piddesc_tables_output) / sizeof (piddesc_tables_output[0]); k++)
  {
    struct piddesc const * const table = piddesc_tables_output[k];
    for (uint32_t i = 0; table[i].pid != DDSI_PID_SENTINEL; i++)
    {
      struct piddesc const * const entry = &table[i];
      if (entry->pid == DDSI_PID_PAD)
        continue;
      if (((entry->flags & PDF_QOS) ? qw : pw) & entry->present_flag)
      {
        assert (entry->plist_offset >= shift);
        assert (shift == 0 || entry->plist_offset - shift < sizeof (dds_qos_t));
        size_t srcoff = entry->plist_offset - shift;
        if (!(entry->flags & PDF_FUNCTION))
          ser_generic (xmsg, entry->pid, src, srcoff, entry->op.desc, bo);
        else
          entry->op.f.ser (xmsg, entry->pid, src, srcoff, bo, context_kind);
      }
    }
  }
}

void ddsi_plist_fini (ddsi_plist_t *plist)
{
  plist_or_xqos_fini (plist, 0, ~(uint64_t)0, ~(uint64_t)0);
#ifndef NDEBUG
  memset (plist, 0x55, sizeof (*plist));
  plist->present = plist->aliased = ~(uint64_t)0;
  plist->qos.present = plist->qos.aliased = ~(uint64_t)0;
#endif
}

void ddsi_plist_fini_mask (ddsi_plist_t *plist, uint64_t pmask, uint64_t qmask)
{
  plist_or_xqos_fini (plist, 0, pmask, qmask);
  plist->present &= ~pmask;
  plist->aliased &= ~pmask;
  plist->qos.present &= ~qmask;
  plist->qos.aliased &= ~qmask;
}

void ddsi_plist_unalias (ddsi_plist_t *plist)
{
  plist_or_xqos_unalias (plist, 0);
}

static dds_return_t ddsi_xqos_valid_strictness (const struct ddsrt_log_cfg *logcfg, const dds_qos_t *xqos, bool strict)
{
  dds_return_t ret;
  if (piddesc_unalias[0] == NULL)
    ddsi_plist_init_tables ();
  for (size_t k = 0; k < sizeof (piddesc_tables_all) / sizeof (piddesc_tables_all[0]); k++)
  {
    struct piddesc const * const table = piddesc_tables_all[k];
    for (uint32_t i = 0; table[i].pid != DDSI_PID_SENTINEL; i++)
    {
      struct piddesc const * const entry = &table[i];
      if (!(entry->flags & PDF_QOS))
        break;
      if (xqos->present & entry->present_flag)
      {
        const size_t srcoff = entry->plist_offset - offsetof (ddsi_plist_t, qos);
        if (!(entry->flags & PDF_FUNCTION))
          ret = valid_generic (xqos, srcoff, entry->op.desc);
        else
          ret = entry->op.f.valid (xqos, srcoff);
        if (ret < 0)
        {
          DDS_CLOG (DDS_LC_PLIST, logcfg, "ddsi_xqos_valid: %s invalid\n", entry->name);
          return ret;
        }
      }
    }
  }
  if ((ret = final_validation_qos (xqos, (ddsi_protocol_version_t) { DDSI_RTPS_MAJOR, DDSI_RTPS_MINOR }, DDSI_VENDORID_ECLIPSE, NULL, strict)) < 0)
  {
    DDS_CLOG (DDS_LC_PLIST, logcfg, "ddsi_xqos_valid: final validation failed\n");
  }
  return ret;
}

dds_return_t ddsi_xqos_valid (const struct ddsrt_log_cfg *logcfg, const dds_qos_t *xqos)
{
  return ddsi_xqos_valid_strictness (logcfg, xqos, true);
}

static void plist_or_xqos_delta (uint64_t *pdelta, uint64_t *qdelta, const void *srcx, const void *srcy, size_t shift, uint64_t pmask, uint64_t qmask)
{
  uint64_t pcheck, qcheck;

  if (piddesc_unalias[0] == NULL)
    ddsi_plist_init_tables ();
  if (shift > 0)
  {
    const dds_qos_t *x = srcx;
    const dds_qos_t *y = srcy;
    *pdelta = 0;
    pcheck = 0;
    *qdelta = (x->present ^ y->present) & qmask;
    qcheck = (x->present & y->present) & qmask;
  }
  else
  {
    const ddsi_plist_t *x = srcx;
    const ddsi_plist_t *y = srcy;
    *pdelta = (x->present ^ y->present) & pmask;
    pcheck = (x->present & y->present) & pmask;
    *qdelta = (x->qos.present ^ y->qos.present) & qmask;
    qcheck = (x->qos.present & y->qos.present) & qmask;
  }
  for (size_t k = 0; k < sizeof (piddesc_tables_all) / sizeof (piddesc_tables_all[0]); k++)
  {
    struct piddesc const * const table = piddesc_tables_all[k];
    for (uint32_t i = 0; table[i].pid != DDSI_PID_SENTINEL; i++)
    {
      struct piddesc const * const entry = &table[i];
      if (shift > 0 && !(entry->flags & PDF_QOS))
        break;
      assert (entry->plist_offset >= shift);
      assert (shift == 0 || entry->plist_offset - shift < sizeof (dds_qos_t));
      const uint64_t check = (entry->flags & PDF_QOS) ? qcheck : pcheck;
      uint64_t * const delta = (entry->flags & PDF_QOS) ? qdelta : pdelta;
      if (check & entry->present_flag)
      {
        const size_t off = entry->plist_offset - shift;
        bool equal;
        /* Partition is special-cased because it is a set (with a special rules
           for empty sets and empty strings to boot), and normal string sequence
           comparison requires the ordering to be the same */
        if (entry->pid == DDSI_PID_PARTITION)
          equal = partitions_equal (srcx, srcy, off);
        else if (!(entry->flags & PDF_FUNCTION))
          equal = equal_generic (srcx, srcy, off, entry->op.desc);
        else
          equal = entry->op.f.equal (srcx, srcy, off);
        if (!equal)
          *delta |= entry->present_flag;
      }
    }
  }
}

uint64_t ddsi_xqos_delta (const dds_qos_t *x, const dds_qos_t *y, uint64_t mask)
{
  uint64_t pdelta, qdelta;
  plist_or_xqos_delta (&pdelta, &qdelta, x, y, offsetof (ddsi_plist_t, qos), 0, mask);
  return qdelta;
}

void ddsi_plist_delta (uint64_t *pdelta, uint64_t *qdelta, const ddsi_plist_t *x, const ddsi_plist_t *y, uint64_t pmask, uint64_t qmask)
{
  plist_or_xqos_delta (pdelta, qdelta, x, y, 0, pmask, qmask);
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

static void add_locator (ddsi_locators_t *ls, uint64_t present, uint64_t wanted, uint64_t fl, const ddsi_locator_t *loc)
{
  if (wanted & fl)
  {
    struct ddsi_locators_one *nloc;
    if (!(present & fl))
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
  }
}

static bool locator_address_prefix_zero (const ddsi_locator_t *loc, size_t prefixlen)
{
  assert (prefixlen <= sizeof (loc->address));
  for (size_t i = 0; i < prefixlen; i++)
    if (loc->address[i] != 0)
      return false;
  return true;
}

static bool locator_address_zero (const ddsi_locator_t *loc)
{
  return locator_address_prefix_zero (loc, sizeof (loc->address));
}

static enum do_locator_result do_locator (ddsi_locators_t *ls, uint64_t present, uint64_t wanted, uint64_t fl, const struct dd *dd, struct ddsi_domaingv const * const gv)
{
  ddsi_locator_t loc;

  if (dd->bufsz < 24)
    return DOLOC_INVALID;

  memcpy (&loc.kind, dd->buf, 4);
  memcpy (&loc.port, dd->buf + 4, 4);
  memcpy (loc.address, dd->buf + 8, 16);
  if (dd->bswap)
  {
    loc.kind = ddsrt_bswap4 (loc.kind);
    loc.port = ddsrt_bswap4u (loc.port);
  }

  struct ddsi_tran_factory * fact = ddsi_factory_find_supported_kind (gv, loc.kind);
  if (fact == NULL || !fact->m_enable)
    return DOLOC_IGNORED;

  switch (loc.kind)
  {
    case DDSI_LOCATOR_KIND_UDPv4:
    case DDSI_LOCATOR_KIND_TCPv4:
      if (!ddsi_is_valid_port (fact, loc.port))
        return DOLOC_INVALID;
      if (!locator_address_prefix_zero (&loc, 12))
        return DOLOC_INVALID;
      break;
    case DDSI_LOCATOR_KIND_UDPv6:
    case DDSI_LOCATOR_KIND_TCPv6:
      if (!ddsi_is_valid_port (fact, loc.port))
        return DOLOC_INVALID;
      break;
    case DDSI_LOCATOR_KIND_UDPv4MCGEN:
      if (!ddsi_vendor_is_eclipse (dd->vendorid))
        return DOLOC_IGNORED;
      else
      {
        const ddsi_udpv4mcgen_address_t *x = (const ddsi_udpv4mcgen_address_t *) loc.address;
        if (!ddsi_is_valid_port (fact, loc.port))
          return DOLOC_INVALID;
        if (!ddsi_factory_supports (fact, DDSI_LOCATOR_KIND_UDPv4))
          return DOLOC_IGNORED;
        if ((uint32_t) x->base + x->count >= 28 || x->count == 0 || x->idx >= x->count)
          return DOLOC_INVALID;
      }
      break;
#ifdef DDS_HAS_SHM
    case DDSI_LOCATOR_KIND_SHEM:
      if (!ddsi_vendor_is_eclipse (dd->vendorid))
        return DOLOC_IGNORED;
      else
      {
        if (!ddsi_is_valid_port (fact, loc.port))
          return DOLOC_INVALID;
        if (0 != memcmp(loc.address, gv->loc_iceoryx_addr.address, 16))
          return DOLOC_IGNORED;
      }
      break;
#endif
    case DDSI_LOCATOR_KIND_INVALID:
      if (!locator_address_zero (&loc))
        return DOLOC_INVALID;
      if (loc.port != 0)
        return DOLOC_INVALID;
      /* silently drop correctly formatted "invalid" locators. */
      return DOLOC_IGNORED;
    case DDSI_LOCATOR_KIND_RESERVED:
      /* silently drop "reserved" locators. */
      return DOLOC_IGNORED;
    case DDSI_LOCATOR_KIND_RAWETH:
      if (!ddsi_vendor_is_eclipse (dd->vendorid))
        return DOLOC_IGNORED;
      else
      {
        if (!ddsi_is_valid_port (fact, loc.port))
          return DOLOC_INVALID;
        if (!locator_address_prefix_zero (&loc, 10))
          return DOLOC_INVALID;
      }
      break;
    default:
      return DOLOC_IGNORED;
  }

  add_locator (ls, present, wanted, fl, &loc);
  return DOLOC_ACCEPTED;
}

static void locator_from_ipv4address_port (ddsi_locator_t *loc, const ddsi_ipv4address_t *a, const ddsi_port_t *p)
{
  loc->kind = DDSI_LOCATOR_KIND_UDPv4;
  loc->port = *p;
  memset (loc->address, 0, 12);
  memcpy (loc->address + 12, a, 4);
}

static dds_return_t do_ipv4address (ddsi_plist_t *dest, ddsi_ipaddress_params_tmp_t *dest_tmp, uint64_t wanted, uint32_t fl_tmp, const struct dd *dd)
{
  ddsi_ipv4address_t *a;
  ddsi_port_t *p;
  ddsi_locators_t *ls;
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
      return DDS_RETCODE_BAD_PARAMETER;
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
    ddsi_locator_t loc;
    locator_from_ipv4address_port (&loc, a, p);
    add_locator (ls, dest->present, wanted, fldest, &loc);
    dest_tmp->present &= ~(fl_tmp | fl1_tmp);
    dest->present |= fldest;
  }
  return 0;
}

static dds_return_t do_port (ddsi_plist_t *dest, ddsi_ipaddress_params_tmp_t *dest_tmp, uint64_t wanted, uint32_t fl_tmp, const struct dd *dd)
{
  ddsi_ipv4address_t *a;
  ddsi_port_t *p;
  ddsi_locators_t *ls;
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
      return DDS_RETCODE_BAD_PARAMETER;
  }
  memcpy (p, dd->buf, sizeof (*p));
  if (dd->bswap)
    *p = ddsrt_bswap4u (*p);
  if (*p <= 0 || *p > 65535)
    return DDS_RETCODE_BAD_PARAMETER;
  dest_tmp->present |= fl_tmp;
  if ((dest_tmp->present & (fl_tmp | fl1_tmp)) == (fl_tmp | fl1_tmp))
  {
    /* If port already known, add corresponding locator and discard
       both address & port from the set of present plist: this
       allows adding another pair. */
    ddsi_locator_t loc;
    locator_from_ipv4address_port (&loc, a, p);
    add_locator (ls, dest->present, wanted, fldest, &loc);
    dest_tmp->present &= ~(fl_tmp | fl1_tmp);
    dest->present |= fldest;
  }
  return 0;
}

static dds_return_t return_unrecognized_pid (ddsi_plist_t *plist, ddsi_parameterid_t pid)
{
  if (!(pid & DDSI_PID_UNRECOGNIZED_INCOMPATIBLE_FLAG))
    return 0;
  else
  {
    plist->present |= PP_INCOMPATIBLE;
    return DDS_RETCODE_UNSUPPORTED;
  }
}

static dds_return_t init_one_parameter (ddsi_plist_t *plist, ddsi_ipaddress_params_tmp_t *dest_tmp, uint64_t pwanted, uint64_t qwanted, uint16_t pid, const struct dd *dd, struct ddsi_domaingv const * const gv)
{
  /* PID_LIVELINESS and PID_PARTICIPANT_LEASE_DURATION need some preprocessing because
     they are both considered "liveliness" by Cyclone, but the spec has the former not
     for participants and the latter only for participants */
  switch (dd->context_kind)
  {
    case DDSI_PLIST_CONTEXT_PARTICIPANT:
      if (pid == DDSI_PID_LIVELINESS)
        return 0;
      if (pid == DDSI_PID_PARTICIPANT_LEASE_DURATION)
        pid = DDSI_PID_LIVELINESS;
      break;
    case DDSI_PLIST_CONTEXT_ENDPOINT:
    case DDSI_PLIST_CONTEXT_TOPIC:
    case DDSI_PLIST_CONTEXT_INLINE_QOS:
      if (pid == DDSI_PID_PARTICIPANT_LEASE_DURATION)
        return 0;
      break;
    case DDSI_PLIST_CONTEXT_QOS_DISALLOWED:
      return DDS_RETCODE_BAD_PARAMETER;
  }

  /* special-cased ipv4address and port, because they have state beyond that what gets
     passed into the generic code */
  switch (pid)
  {
#define XA(NAME_) case DDSI_PID_##NAME_##_IPADDRESS: return do_ipv4address (plist, dest_tmp, pwanted, PPTMP_##NAME_##_IPADDRESS, dd)
#define XP(NAME_) case DDSI_PID_##NAME_##_PORT: return do_port (plist, dest_tmp, pwanted, PPTMP_##NAME_##_PORT, dd)
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
  if (!(pid & DDSI_PID_VENDORSPECIFIC_FLAG))
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
  size_t pididx = pid_to_index(pid);
  if (pididx > index->index_max || (entry = index->index[pididx]) == NULL)
    return return_unrecognized_pid (plist, pid);
  assert (pid_to_index (pid) == pid_to_index (entry->pid));
  if (pid != entry->pid)
    return return_unrecognized_pid (plist, pid);
  assert (pid != DDSI_PID_PAD);

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
    GVWARNING ("invalid parameter list (vendor %u.%u, version %u.%u): pid %"PRIx16" (%s) multiply defined\n",
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
  if (entry->flags & PDF_FUNCTION)
  {
    ret = entry->op.f.deser (dst, &flagset, entry->present_flag, dd, gv);
    if (ret == 0 && (*flagset.present & entry->present_flag) && entry->op.f.valid)
      ret = entry->op.f.valid (plist, entry->plist_offset);
  }
  else
    ret = deser_generic (dst, &flagset, entry->present_flag, dd, entry->op.desc);
  if (ret == 0 && (*flagset.present & entry->present_flag) && entry->deser_validate_xform)
    ret = entry->deser_validate_xform (dst, dd);
  if (ret < 0)
  {
    char tmp[256], *ptmp = tmp;
    size_t tmpsize = sizeof (tmp);
    (void) prtf_octetseq (&ptmp, &tmpsize, (uint32_t) dd->bufsz, dd->buf);
    GVWARNING ("invalid parameter list (vendor %u.%u, version %u.%u): pid %"PRIx16" (%s) invalid, input = %s\n",
               dd->vendorid.id[0], dd->vendorid.id[1],
               dd->protocol_version.major, dd->protocol_version.minor,
               pid, entry->name, tmp);
  }
  return ret;
}

void ddsi_plist_mergein_missing (ddsi_plist_t *a, const ddsi_plist_t *b, uint64_t pmask, uint64_t qmask)
{
  plist_or_xqos_mergein_missing (a, b, 0, pmask, qmask);
}

void ddsi_xqos_mergein_missing (dds_qos_t *a, const dds_qos_t *b, uint64_t mask)
{
  plist_or_xqos_mergein_missing (a, b, offsetof (ddsi_plist_t, qos), 0, mask);
}

void ddsi_plist_copy (ddsi_plist_t *dst, const ddsi_plist_t *src)
{
  ddsi_plist_init_empty (dst);
  ddsi_plist_mergein_missing (dst, src, ~(uint64_t)0, ~(uint64_t)0);
}

ddsi_plist_t *ddsi_plist_dup (const ddsi_plist_t *src)
{
  ddsi_plist_t *dst;
  dst = ddsrt_malloc (sizeof (*dst));
  ddsi_plist_copy (dst, src);
  assert (dst->aliased == 0);
  return dst;
}

void ddsi_plist_init_empty (ddsi_plist_t *dest)
{
#ifndef NDEBUG
  memset (dest, 0x55, sizeof (*dest));
#endif
  dest->present = dest->aliased = 0;
  ddsi_xqos_init_empty (&dest->qos);
}

static dds_return_t final_validation_qos (const dds_qos_t *dest, ddsi_protocol_version_t protocol_version, ddsi_vendorid_t vendorid, bool *dursvc_accepted_allzero, bool strict)
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
  if (dest->present & DDSI_QP_HISTORY)
    tmphist = dest->history;
  if (dest->present & DDSI_QP_RESOURCE_LIMITS)
    tmpreslim = dest->resource_limits;
  if ((res = validate_history_and_resource_limits (&tmphist, &tmpreslim)) < 0)
    return res;

  if ((dest->present & DDSI_QP_DEADLINE) && (dest->present & DDSI_QP_TIME_BASED_FILTER))
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
  if (dest->present & DDSI_QP_DURABILITY_SERVICE)
  {
    const dds_durability_kind_t durkind = (dest->present & DDSI_QP_DURABILITY) ? dest->durability.kind : DDS_DURABILITY_VOLATILE;
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
      acceptzero = ddsi_vendor_is_twinoaks (vendorid);
    else
      acceptzero = !ddsi_vendor_is_eclipse (vendorid);
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

static dds_return_t final_validation (ddsi_plist_t *dest, ddsi_protocol_version_t protocol_version, ddsi_vendorid_t vendorid, bool *dursvc_accepted_allzero, bool strict)
{
  return final_validation_qos (&dest->qos, protocol_version, vendorid, dursvc_accepted_allzero, strict);
}

dds_return_t ddsi_plist_init_frommsg (ddsi_plist_t *dest, char **nextafterplist, uint64_t pwanted, uint64_t qwanted, const ddsi_plist_src_t *src, struct ddsi_domaingv const * const gv, enum ddsi_plist_context_kind context_kind)
{
  const unsigned char *pl;
  struct dd dd;
  ddsi_ipaddress_params_tmp_t dest_tmp;

#ifndef NDEBUG
  memset (dest, 0, sizeof (*dest));
#endif

  if (nextafterplist)
    *nextafterplist = NULL;
  dd.protocol_version = src->protocol_version;
  dd.vendorid = src->vendorid;
  dd.context_kind = context_kind;
  switch (src->encoding)
  {
    case DDSI_RTPS_PL_CDR_LE:
#if DDSRT_ENDIAN == DDSRT_LITTLE_ENDIAN
      dd.bswap = 0;
#else
      dd.bswap = 1;
#endif
      break;
    case DDSI_RTPS_PL_CDR_BE:
#if DDSRT_ENDIAN == DDSRT_LITTLE_ENDIAN
      dd.bswap = 1;
#else
      dd.bswap = 0;
#endif
      break;
    default:
      GVWARNING ("plist(vendor %u.%u): unknown encoding (%d)\n",
                 src->vendorid.id[0], src->vendorid.id[1], src->encoding);
      return DDS_RETCODE_BAD_PARAMETER;
  }
  ddsi_plist_init_empty (dest);
  dest_tmp.present = 0;

  GVLOG (DDS_LC_PLIST, "DDSI_PLIST_INIT (bswap %d)\n", dd.bswap);

  pl = src->buf;
  while (pl + sizeof (ddsi_parameter_t) <= src->buf + src->bufsz)
  {
    ddsi_parameter_t *par = (ddsi_parameter_t *) pl;
    ddsi_parameterid_t pid;
    uint16_t length;
    dds_return_t res;
    /* swapping header partially based on wireshark dissector
       output, partially on intuition, and in a small part based on
       the spec */
    pid = (ddsi_parameterid_t) (dd.bswap ? ddsrt_bswap2u (par->parameterid) : par->parameterid);
    length = (uint16_t) (dd.bswap ? ddsrt_bswap2u (par->length) : par->length);
    if (pid == DDSI_PID_SENTINEL)
    {
      /* Sentinel terminates list, the length is ignored, DDSI 9.4.2.11. */
      bool dursvc_accepted_allzero;
      GVLOG (DDS_LC_PLIST, "%4"PRIx32" PID %"PRIx16"\n", (uint32_t) (pl - src->buf), pid);
      if ((res = final_validation (dest, src->protocol_version, src->vendorid, &dursvc_accepted_allzero, src->strict)) < 0)
      {
        ddsi_plist_fini (dest);
        return res;
      }
      else
      {
        /* If we accepted an all-zero durability service, that's awfully friendly of ours,
           but we'll pretend we never saw it */
        if (dursvc_accepted_allzero)
          dest->qos.present &= ~DDSI_QP_DURABILITY_SERVICE;
        pl += sizeof (*par);
        if (nextafterplist)
          *nextafterplist = (char *) pl;
        return 0;
      }
    }
    if (length > src->bufsz - sizeof (*par) - (uint32_t) (pl - src->buf))
    {
      GVWARNING ("plist(vendor %u.%u): parameter length %"PRIu16" out of bounds\n",
                 src->vendorid.id[0], src->vendorid.id[1], length);
      ddsi_plist_fini (dest);
      return DDS_RETCODE_BAD_PARAMETER;
    }
    if ((length % 4) != 0) /* DDSI 9.4.2.11 */
    {
      GVWARNING ("plist(vendor %u.%u): parameter length %"PRIu16" mod 4 != 0\n",
                 src->vendorid.id[0], src->vendorid.id[1], length);
      ddsi_plist_fini (dest);
      return DDS_RETCODE_BAD_PARAMETER;
    }

    if (gv->logconfig.c.mask & DDS_LC_PLIST)
    {
      char tmp[256], *ptmp = tmp;
      size_t tmpsize = sizeof (tmp);
      (void) prtf_octetseq (&ptmp, &tmpsize, length, (const unsigned char *) (par + 1));
      GVLOG (DDS_LC_PLIST, "%4"PRIx32" PID %"PRIx16" len %"PRIu16" %s\n", (uint32_t) (pl - src->buf), pid, length, tmp);
    }

    dd.buf = (const unsigned char *) (par + 1);
    dd.bufsz = length;
    if ((res = init_one_parameter (dest, &dest_tmp, pwanted, qwanted, pid, &dd, gv)) < 0)
    {
      /* make sure we print a trace message on error */
      GVTRACE ("plist(vendor %u.%u): failed at pid=%"PRIx16"\n", src->vendorid.id[0], src->vendorid.id[1], pid);
      ddsi_plist_fini (dest);
      return res;
    }
    pl += sizeof (*par) + length;
  }
  /* If we get here, that means we reached the end of the message
     without encountering a sentinel. That is an error */
  GVWARNING ("plist(vendor %u.%u): invalid parameter list: sentinel missing\n",
             src->vendorid.id[0], src->vendorid.id[1]);
  ddsi_plist_fini (dest);
  return DDS_RETCODE_BAD_PARAMETER;
}

dds_return_t ddsi_plist_findparam_checking (const void *buf, size_t bufsz, uint16_t encoding, ddsi_parameterid_t needle, void **needlep, size_t *needlesz)
{
  /* set needle to DDSI_PID_SENTINEL if all you want to do is scan the structure */
  assert (needle == DDSI_PID_SENTINEL || (needlep != NULL && needlesz != NULL));
  bool bswap;
  if (needlep)
    *needlep = NULL;
  DDSRT_WARNING_MSVC_OFF(6326)
  switch (encoding)
  {
    case DDSI_RTPS_PL_CDR_LE:
      bswap = (DDSRT_ENDIAN != DDSRT_LITTLE_ENDIAN);
      break;
    case DDSI_RTPS_PL_CDR_BE:
      bswap = (DDSRT_ENDIAN != DDSRT_BIG_ENDIAN);
      break;
    default:
      return DDS_RETCODE_BAD_PARAMETER;
  }
  DDSRT_WARNING_MSVC_ON(6326)
  const unsigned char *pl = buf;
  const unsigned char *endp = pl + bufsz;
  while (pl + sizeof (ddsi_parameter_t) <= endp)
  {
    const ddsi_parameter_t *par = (const ddsi_parameter_t *) pl;
    ddsi_parameterid_t pid;
    uint16_t length;
    pid = (ddsi_parameterid_t) (bswap ? ddsrt_bswap2u (par->parameterid) : par->parameterid);
    length = (uint16_t) (bswap ? ddsrt_bswap2u (par->length) : par->length);
    pl += sizeof (*par);

    if (pid == DDSI_PID_SENTINEL)
      return (needlep && *needlep == NULL) ? DDS_RETCODE_NOT_FOUND : DDS_RETCODE_OK;
    else if (length > (size_t) (endp - pl) || (length % 4) != 0 /* DDSI 9.4.2.11 */)
      return DDS_RETCODE_BAD_PARAMETER;
    else if (pid == needle)
    {
      *needlep = (void *) pl;
      *needlesz = length;
    }
    pl += length;
  }
  return DDS_RETCODE_BAD_PARAMETER;
}

unsigned char *ddsi_plist_quickscan (struct ddsi_rsample_info *dest, const ddsi_keyhash_t **keyhashp, const ddsi_plist_src_t *src, struct ddsi_domaingv const * const gv)
{
  /* Sets a few fields in dest, returns address of first byte
     following parameter list, or NULL on error.  Most errors will go
     undetected, unlike ddsi_plist_init_frommsg(). */
  const unsigned char *pl;
  dest->statusinfo = 0;
  dest->complex_qos = 0;
  *keyhashp = NULL;
  switch (src->encoding)
  {
    case DDSI_RTPS_PL_CDR_LE:
#if DDSRT_ENDIAN == DDSRT_LITTLE_ENDIAN
      dest->bswap = 0;
#else
      dest->bswap = 1;
#endif
      break;
    case DDSI_RTPS_PL_CDR_BE:
#if DDSRT_ENDIAN == DDSRT_LITTLE_ENDIAN
      dest->bswap = 1;
#else
      dest->bswap = 0;
#endif
      break;
    default:
      GVWARNING ("plist(vendor %u.%u): quickscan: unknown encoding (%d)\n",
                 src->vendorid.id[0], src->vendorid.id[1], src->encoding);
      return NULL;
  }
  GVLOG (DDS_LC_PLIST, "DDSI_PLIST_QUICKSCAN (bswap %d)\n", dest->bswap);
  pl = src->buf;
  while (pl + sizeof (ddsi_parameter_t) <= src->buf + src->bufsz)
  {
    ddsi_parameter_t *par = (ddsi_parameter_t *) pl;
    ddsi_parameterid_t pid;
    uint16_t length;
    pid = (ddsi_parameterid_t) (dest->bswap ? ddsrt_bswap2u (par->parameterid) : par->parameterid);
    length = (uint16_t) (dest->bswap ? ddsrt_bswap2u (par->length) : par->length);
    pl += sizeof (*par);
    if (pid == DDSI_PID_SENTINEL)
      return (unsigned char *) pl;
    if (length > src->bufsz - (size_t)(pl - src->buf))
    {
      GVWARNING ("plist(vendor %u.%u): quickscan: parameter length %"PRIu16" out of bounds\n",
                 src->vendorid.id[0], src->vendorid.id[1], length);
      return NULL;
    }
    if ((length % 4) != 0) /* DDSI 9.4.2.11 */
    {
      GVWARNING ("plist(vendor %u.%u): quickscan: parameter length %"PRIu16" mod 4 != 0\n",
                 src->vendorid.id[0], src->vendorid.id[1], length);
      return NULL;
    }
    switch (pid)
    {
      case DDSI_PID_PAD:
        break;
      case DDSI_PID_STATUSINFO:
        if (length < 4)
        {
          GVTRACE ("plist(vendor %u.%u): quickscan(DDSI_PID_STATUSINFO): buffer too small\n",
                   src->vendorid.id[0], src->vendorid.id[1]);
          return NULL;
        }
        else
        {
          /* can only represent 2 LSBs of statusinfo in "dest", so if others are set,
             mark it as a "complex_qos" and accept the hit of parsing the data completely. */
          uint32_t stinfo = ddsrt_fromBE4u (*((uint32_t *) pl));
          dest->statusinfo = stinfo & 3u;
          if ((stinfo & ~3u))
            dest->complex_qos = 1;
        }
        break;
      case DDSI_PID_KEYHASH:
        if (length < sizeof (ddsi_keyhash_t))
        {
          GVTRACE ("plist(vendor %u.%u): quickscan(DDSI_PID_KEYHASH): buffer too small\n",
                   src->vendorid.id[0], src->vendorid.id[1]);
          return NULL;
        }
        else
        {
          *keyhashp = (const ddsi_keyhash_t *) pl;
        }
        break;
      default:
        GVLOG (DDS_LC_PLIST, "(pid=%"PRIx16" complex_qos=1)", pid);
        dest->complex_qos = 1;
        break;
    }
    pl += length;
  }
  /* If we get here, that means we reached the end of the message
     without encountering a sentinel. That is an error */
  GVWARNING ("plist(vendor %u.%u): quickscan: invalid parameter list: sentinel missing\n",
             src->vendorid.id[0], src->vendorid.id[1]);
  return NULL;
}

void ddsi_xqos_init_empty (dds_qos_t *dest)
{
#ifndef NDEBUG
  memset (dest, 0x55, sizeof (*dest));
#endif
  dest->present = dest->aliased = 0;
}

const dds_qos_t ddsi_default_qos_reader = {
  .present = DDSI_QP_PRESENTATION | DDSI_QP_DURABILITY | DDSI_QP_DEADLINE | DDSI_QP_LATENCY_BUDGET | DDSI_QP_LIVELINESS | DDSI_QP_DESTINATION_ORDER | DDSI_QP_HISTORY | DDSI_QP_RESOURCE_LIMITS | DDSI_QP_TRANSPORT_PRIORITY | DDSI_QP_OWNERSHIP | DDSI_QP_CYCLONE_IGNORELOCAL | DDSI_QP_TOPIC_DATA | DDSI_QP_GROUP_DATA | DDSI_QP_USER_DATA | DDSI_QP_PARTITION | DDSI_QP_RELIABILITY | DDSI_QP_TIME_BASED_FILTER | DDSI_QP_ADLINK_READER_DATA_LIFECYCLE | DDSI_QP_ADLINK_READER_LIFESPAN | DDSI_QP_TYPE_CONSISTENCY_ENFORCEMENT | DDSI_QP_LOCATOR_MASK | DDSI_QP_DATA_REPRESENTATION,
  .aliased = DDSI_QP_DATA_REPRESENTATION,
  .presentation.access_scope = DDS_PRESENTATION_INSTANCE,
  .presentation.coherent_access = 0,
  .presentation.ordered_access = 0,
  .durability.kind = DDS_DURABILITY_VOLATILE,
  .deadline.deadline = DDS_INFINITY,
  .latency_budget.duration = 0,
  .liveliness.kind = DDS_LIVELINESS_AUTOMATIC,
  .liveliness.lease_duration = DDS_INFINITY,
  .destination_order.kind = DDS_DESTINATIONORDER_BY_RECEPTION_TIMESTAMP,
  .history.kind = DDS_HISTORY_KEEP_LAST,
  .history.depth = 1,
  .resource_limits.max_samples = DDS_LENGTH_UNLIMITED,
  .resource_limits.max_instances = DDS_LENGTH_UNLIMITED,
  .resource_limits.max_samples_per_instance = DDS_LENGTH_UNLIMITED,
  .transport_priority.value = 0,
  .ownership.kind = DDS_OWNERSHIP_SHARED,
  .ignorelocal.value = DDS_IGNORELOCAL_NONE,
  .topic_data.length = 0,
  .topic_data.value = NULL,
  .group_data.length = 0,
  .group_data.value = NULL,
  .user_data.length = 0,
  .user_data.value = NULL,
  .partition.n = 0,
  .partition.strs = NULL,
  .reliability.kind = DDS_RELIABILITY_BEST_EFFORT,
  .time_based_filter.minimum_separation = 0,
  .reader_data_lifecycle.autopurge_nowriter_samples_delay = DDS_INFINITY,
  .reader_data_lifecycle.autopurge_disposed_samples_delay = DDS_INFINITY,
  .reader_lifespan.use_lifespan = 0,
  .reader_lifespan.duration = DDS_INFINITY,
  .type_consistency.kind = DDS_TYPE_CONSISTENCY_ALLOW_TYPE_COERCION,
  .type_consistency.ignore_sequence_bounds = true,
  .type_consistency.ignore_string_bounds = true,
  .type_consistency.ignore_member_names = false,
  .type_consistency.prevent_type_widening = false,
  .type_consistency.force_type_validation = false,
  .ignore_locator_type = 0,
  .data_representation.value.n = 1,
  .data_representation.value.ids = (dds_data_representation_id_t []) { DDS_DATA_REPRESENTATION_XCDR1 }
};

const dds_qos_t ddsi_default_qos_writer = {
  .present = DDSI_QP_PRESENTATION | DDSI_QP_DURABILITY | DDSI_QP_DEADLINE | DDSI_QP_LATENCY_BUDGET | DDSI_QP_LIVELINESS | DDSI_QP_DESTINATION_ORDER | DDSI_QP_HISTORY | DDSI_QP_RESOURCE_LIMITS | DDSI_QP_OWNERSHIP | DDSI_QP_CYCLONE_IGNORELOCAL | DDSI_QP_TOPIC_DATA | DDSI_QP_GROUP_DATA | DDSI_QP_USER_DATA | DDSI_QP_PARTITION | DDSI_QP_DURABILITY_SERVICE | DDSI_QP_RELIABILITY | DDSI_QP_OWNERSHIP_STRENGTH | DDSI_QP_TRANSPORT_PRIORITY | DDSI_QP_LIFESPAN | DDSI_QP_ADLINK_WRITER_DATA_LIFECYCLE | DDSI_QP_LOCATOR_MASK | DDSI_QP_DATA_REPRESENTATION | DDSI_QP_CYCLONE_WRITER_BATCHING,
  .aliased = DDSI_QP_DATA_REPRESENTATION,
  .presentation.access_scope = DDS_PRESENTATION_INSTANCE,
  .presentation.coherent_access = 0,
  .presentation.ordered_access = 0,
  .durability.kind = DDS_DURABILITY_VOLATILE,
  .deadline.deadline = DDS_INFINITY,
  .latency_budget.duration = 0,
  .liveliness.kind = DDS_LIVELINESS_AUTOMATIC,
  .liveliness.lease_duration = DDS_INFINITY,
  .destination_order.kind = DDS_DESTINATIONORDER_BY_RECEPTION_TIMESTAMP,
  .history.kind = DDS_HISTORY_KEEP_LAST,
  .history.depth = 1,
  .resource_limits.max_samples = DDS_LENGTH_UNLIMITED,
  .resource_limits.max_instances = DDS_LENGTH_UNLIMITED,
  .resource_limits.max_samples_per_instance = DDS_LENGTH_UNLIMITED,
  .ownership.kind = DDS_OWNERSHIP_SHARED,
  .ignorelocal.value = DDS_IGNORELOCAL_NONE,
  .topic_data.length = 0,
  .topic_data.value = NULL,
  .group_data.length = 0,
  .group_data.value = NULL,
  .user_data.length = 0,
  .user_data.value = NULL,
  .partition.n = 0,
  .partition.strs = NULL,
  .durability_service.service_cleanup_delay = 0,
  .durability_service.history.kind = DDS_HISTORY_KEEP_LAST,
  .durability_service.history.depth = 1,
  .durability_service.resource_limits.max_samples = DDS_LENGTH_UNLIMITED,
  .durability_service.resource_limits.max_instances = DDS_LENGTH_UNLIMITED,
  .durability_service.resource_limits.max_samples_per_instance = DDS_LENGTH_UNLIMITED,
  .reliability.kind = DDS_RELIABILITY_RELIABLE,
  .reliability.max_blocking_time = DDS_MSECS (100),
  .ownership_strength.value = 0,
  .transport_priority.value = 0,
  .lifespan.duration = DDS_INFINITY,
  .writer_data_lifecycle.autodispose_unregistered_instances = 1,
  .writer_batching.batch_updates = 0,
  .ignore_locator_type = 0,
  .data_representation.value.n = 1,
  .data_representation.value.ids = (dds_data_representation_id_t []) { DDS_DATA_REPRESENTATION_XCDR1 }
};

const dds_qos_t ddsi_default_qos_topic = {
  .present = DDSI_QP_PRESENTATION | DDSI_QP_DURABILITY | DDSI_QP_DEADLINE | DDSI_QP_LATENCY_BUDGET | DDSI_QP_LIVELINESS | DDSI_QP_DESTINATION_ORDER | DDSI_QP_HISTORY | DDSI_QP_RESOURCE_LIMITS | DDSI_QP_TRANSPORT_PRIORITY | DDSI_QP_OWNERSHIP | DDSI_QP_CYCLONE_IGNORELOCAL | DDSI_QP_DURABILITY_SERVICE | DDSI_QP_RELIABILITY | DDSI_QP_LIFESPAN | DDSI_QP_DATA_REPRESENTATION,
  .aliased = DDSI_QP_DATA_REPRESENTATION,
  .presentation.access_scope = DDS_PRESENTATION_INSTANCE,
  .presentation.coherent_access = 0,
  .presentation.ordered_access = 0,
  .durability.kind = DDS_DURABILITY_VOLATILE,
  .deadline.deadline = DDS_INFINITY,
  .latency_budget.duration = 0,
  .liveliness.kind = DDS_LIVELINESS_AUTOMATIC,
  .liveliness.lease_duration = DDS_INFINITY,
  .destination_order.kind = DDS_DESTINATIONORDER_BY_RECEPTION_TIMESTAMP,
  .history.kind = DDS_HISTORY_KEEP_LAST,
  .history.depth = 1,
  .resource_limits.max_samples = DDS_LENGTH_UNLIMITED,
  .resource_limits.max_instances = DDS_LENGTH_UNLIMITED,
  .resource_limits.max_samples_per_instance = DDS_LENGTH_UNLIMITED,
  .transport_priority.value = 0,
  .ownership.kind = DDS_OWNERSHIP_SHARED,
  .ignorelocal.value = DDS_IGNORELOCAL_NONE,
  .reliability.kind = DDS_RELIABILITY_BEST_EFFORT,
  .reliability.max_blocking_time = DDS_MSECS (100),
  .durability_service.service_cleanup_delay = 0,
  .durability_service.history.kind = DDS_HISTORY_KEEP_LAST,
  .durability_service.history.depth = 1,
  .durability_service.resource_limits.max_samples = DDS_LENGTH_UNLIMITED,
  .durability_service.resource_limits.max_instances = DDS_LENGTH_UNLIMITED,
  .durability_service.resource_limits.max_samples_per_instance = DDS_LENGTH_UNLIMITED,
  .lifespan.duration = DDS_INFINITY,
  .data_representation.value.n = 1,
  .data_representation.value.ids = (dds_data_representation_id_t []) { DDS_DATA_REPRESENTATION_XCDR1 }
};

const dds_qos_t ddsi_default_qos_publisher_subscriber = {
  .present = DDSI_QP_GROUP_DATA | DDSI_QP_PARTITION | DDSI_QP_ADLINK_ENTITY_FACTORY,
  .aliased = 0,
  .presentation.access_scope = DDS_PRESENTATION_INSTANCE,
  .presentation.coherent_access = 0,
  .presentation.ordered_access = 0,
  .entity_factory.autoenable_created_entities = 1,
  .group_data.length = 0,
  .group_data.value = NULL,
  .partition.n = 0,
  .partition.strs = NULL
};

const dds_qos_t ddsi_default_qos_participant= {
  .present = DDSI_QP_ADLINK_ENTITY_FACTORY | DDSI_QP_USER_DATA | DDSI_QP_LIVELINESS,
  .aliased = 0,
  .entity_factory.autoenable_created_entities = 0,
  .user_data.length = 0,
  .user_data.value = NULL,
  .liveliness.kind = DDS_LIVELINESS_AUTOMATIC,
  .liveliness.lease_duration = DDS_SECS (100)
};

void ddsi_xqos_copy (dds_qos_t *dst, const dds_qos_t *src)
{
  ddsi_xqos_init_empty (dst);
  ddsi_xqos_mergein_missing (dst, src, ~(uint64_t)0);
}

void ddsi_xqos_fini (dds_qos_t *xqos)
{
  plist_or_xqos_fini (xqos, offsetof (ddsi_plist_t, qos), ~(uint64_t)0, ~(uint64_t)0);
#ifndef NDEBUG
  memset (xqos, 0x55, sizeof (*xqos));
  xqos->present = xqos->aliased = ~(uint64_t)0;
#endif
}

void ddsi_xqos_fini_mask (dds_qos_t *xqos, uint64_t mask)
{
  plist_or_xqos_fini (xqos, offsetof (ddsi_plist_t, qos), ~(uint64_t)0, mask);
  xqos->present &= ~mask;
  xqos->aliased &= ~mask;
}

void ddsi_xqos_unalias (dds_qos_t *xqos)
{
  plist_or_xqos_unalias (xqos, offsetof (ddsi_plist_t, qos));
}

dds_qos_t * ddsi_xqos_dup (const dds_qos_t *src)
{
  dds_qos_t *dst = ddsrt_malloc (sizeof (*dst));
  ddsi_xqos_copy (dst, src);
  assert (dst->aliased == 0);
  return dst;
}

bool ddsi_xqos_add_property_if_unset (dds_qos_t *q, bool propagate, const char *name, const char *value)
{
  if ((q->present & DDSI_QP_PROPERTY_LIST) == 0)
  {
    // No properties, definitely not set
    q->present |= DDSI_QP_PROPERTY_LIST;
    q->property.value.n = 1;
    q->property.value.props = ddsrt_malloc(sizeof(dds_property_t));
    q->property.binary_value.n = 0;
    q->property.binary_value.props = NULL;
    q->property.value.props[0].propagate = propagate;
    q->property.value.props[0].name = ddsrt_strdup(name);
    q->property.value.props[0].value = ddsrt_strdup(value);

    return true;
  }

  for (size_t i = 0; i < q->property.value.n; i++)
  {
    if (strcmp (q->property.value.props[i].name, name) == 0)
    {
      // Already exists
      return false;
    }
  }

  // does not exists, append
  q->property.value.props = dds_realloc (q->property.value.props,
    (q->property.value.n + 1) * sizeof (*q->property.value.props));
  q->property.value.props[q->property.value.n].propagate = propagate;
  q->property.value.props[q->property.value.n].name = ddsrt_strdup (name);
  q->property.value.props[q->property.value.n].value = ddsrt_strdup (value);
  q->property.value.n++;
  return true;
}

bool ddsi_xqos_has_prop_prefix (const dds_qos_t *xqos, const char *nameprefix)
{
  if (!(xqos->present & DDSI_QP_PROPERTY_LIST))
    return false;
  const size_t len = strlen (nameprefix);
  for (uint32_t i = 0; i < xqos->property.value.n; i++)
  {
    if (strncmp (xqos->property.value.props[i].name, nameprefix, len) == 0)
      return true;
  }
  return false;
}

bool ddsi_xqos_find_prop (const dds_qos_t *xqos, const char *name, const char **value)
{
  if (!(xqos->present & DDSI_QP_PROPERTY_LIST))
    return false;
  for (uint32_t i = 0; i < xqos->property.value.n; i++)
  {
    if (strcmp (xqos->property.value.props[i].name, name) == 0)
    {
      if (value)
        *value = xqos->property.value.props[i].value;
      return true;
    }
  }
  return false;
}

#ifdef DDS_HAS_SECURITY
static void fill_property(dds_property_t *prop, const char *name, const char *value)
{
  prop->name = ddsrt_strdup(name);
  prop->value = ddsrt_strdup(value);
  prop->propagate = false;
}

/**
 * Add DDS Security configuration to the QoS as a Property policy used by the security
 * plugins to get their proper settings. If security properties are already present in
 * the QoS, the settings from configuration are ignored.
 */
void ddsi_xqos_mergein_security_config (dds_qos_t *xqos, const struct ddsi_config_omg_security *cfg)
{
  assert(cfg != NULL);

  if (!(xqos->present & DDSI_QP_PROPERTY_LIST))
  {
    xqos->property.value.n = 0;
    xqos->property.value.props = NULL;
    xqos->property.binary_value.n = 0;
    xqos->property.binary_value.props = NULL;
    xqos->present |= DDSI_QP_PROPERTY_LIST;
  }

  /* assume that no security properties exist in qos: fill QoS properties with values from configuration */
  xqos->property.value.props = ddsrt_realloc (xqos->property.value.props, (xqos->property.value.n + 18) /* max */ * sizeof (dds_property_t));

  fill_property(&(xqos->property.value.props[xqos->property.value.n++]), DDS_SEC_PROP_AUTH_LIBRARY_PATH, cfg->authentication_plugin.library_path);
  fill_property(&(xqos->property.value.props[xqos->property.value.n++]), DDS_SEC_PROP_AUTH_LIBRARY_INIT, cfg->authentication_plugin.library_init);
  fill_property(&(xqos->property.value.props[xqos->property.value.n++]), DDS_SEC_PROP_AUTH_LIBRARY_FINALIZE, cfg->authentication_plugin.library_finalize);
  fill_property(&(xqos->property.value.props[xqos->property.value.n++]), DDS_SEC_PROP_CRYPTO_LIBRARY_PATH, cfg->cryptography_plugin.library_path);
  fill_property(&(xqos->property.value.props[xqos->property.value.n++]), DDS_SEC_PROP_CRYPTO_LIBRARY_INIT, cfg->cryptography_plugin.library_init);
  fill_property(&(xqos->property.value.props[xqos->property.value.n++]), DDS_SEC_PROP_CRYPTO_LIBRARY_FINALIZE, cfg->cryptography_plugin.library_finalize);
  fill_property(&(xqos->property.value.props[xqos->property.value.n++]), DDS_SEC_PROP_ACCESS_LIBRARY_PATH, cfg->access_control_plugin.library_path);
  fill_property(&(xqos->property.value.props[xqos->property.value.n++]), DDS_SEC_PROP_ACCESS_LIBRARY_INIT, cfg->access_control_plugin.library_init);
  fill_property(&(xqos->property.value.props[xqos->property.value.n++]), DDS_SEC_PROP_ACCESS_LIBRARY_FINALIZE, cfg->access_control_plugin.library_finalize);

  fill_property(&(xqos->property.value.props[xqos->property.value.n++]), DDS_SEC_PROP_AUTH_IDENTITY_CA, cfg->authentication_properties.identity_ca);
  fill_property(&(xqos->property.value.props[xqos->property.value.n++]), DDS_SEC_PROP_AUTH_PRIV_KEY, cfg->authentication_properties.private_key);
  fill_property(&(xqos->property.value.props[xqos->property.value.n++]), DDS_SEC_PROP_AUTH_IDENTITY_CERT, cfg->authentication_properties.identity_certificate);
  fill_property(&(xqos->property.value.props[xqos->property.value.n++]), DDS_SEC_PROP_ACCESS_PERMISSIONS_CA, cfg->access_control_properties.permissions_ca);
  fill_property(&(xqos->property.value.props[xqos->property.value.n++]), DDS_SEC_PROP_ACCESS_GOVERNANCE, cfg->access_control_properties.governance);
  fill_property(&(xqos->property.value.props[xqos->property.value.n++]), DDS_SEC_PROP_ACCESS_PERMISSIONS, cfg->access_control_properties.permissions);
  if (cfg->authentication_properties.password )
    fill_property(&(xqos->property.value.props[xqos->property.value.n++]), DDS_SEC_PROP_AUTH_PASSWORD, cfg->authentication_properties.password);
  if (cfg->authentication_properties.trusted_ca_dir )
    fill_property(&(xqos->property.value.props[xqos->property.value.n++]), DDS_SEC_PROP_ACCESS_TRUSTED_CA_DIR, cfg->authentication_properties.trusted_ca_dir);
  if (cfg->authentication_properties.crl )
    fill_property(&(xqos->property.value.props[xqos->property.value.n++]), ORG_ECLIPSE_CYCLONEDDS_SEC_AUTH_CRL, cfg->authentication_properties.crl);
}
#endif /* DDS_HAS_SECURITY */

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

static int partitions_equal (const void *srca, const void *srcb, size_t off)
{
  const dds_partition_qospolicy_t *a = (const dds_partition_qospolicy_t *) ((const char *) srca + off);
  const dds_partition_qospolicy_t *b = (const dds_partition_qospolicy_t *) ((const char *) srcb + off);
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

void ddsi_xqos_addtomsg (struct ddsi_xmsg *m, const dds_qos_t *xqos, uint64_t wanted, enum ddsi_plist_context_kind context_kind)
{
  plist_or_xqos_addtomsg (m, xqos, offsetof (struct ddsi_plist, qos), 0, wanted, DDSRT_BOSEL_NATIVE, context_kind);
}

void ddsi_plist_addtomsg_bo (struct ddsi_xmsg *m, const ddsi_plist_t *ps, uint64_t pwanted, uint64_t qwanted, enum ddsrt_byte_order_selector bo, enum ddsi_plist_context_kind context_kind)
{
  plist_or_xqos_addtomsg (m, ps, 0, pwanted, qwanted, bo, context_kind);
}

void ddsi_plist_addtomsg (struct ddsi_xmsg *m, const ddsi_plist_t *ps, uint64_t pwanted, uint64_t qwanted, enum ddsi_plist_context_kind context_kind)
{
  plist_or_xqos_addtomsg (m, ps, 0, pwanted, qwanted, DDSRT_BOSEL_NATIVE, context_kind);
}

/*************************/

static void plist_or_xqos_print (char * __restrict *buf, size_t * __restrict bufsize, const void * __restrict src, size_t shift, uint64_t pwanted, uint64_t qwanted)
{
  /* shift == 0: plist, shift > 0: just qos */
  const char *sep = "";
  uint64_t pw, qw;
  if (*bufsize == 0)
    return;
  (*buf)[0] = 0;
  if (shift > 0)
  {
    const dds_qos_t *qos = src;
    pw = 0;
    qw = qos->present & qwanted;
  }
  else
  {
    const ddsi_plist_t *plist = src;
    pw = plist->present & pwanted;
    qw = plist->qos.present & qwanted;
  }
  for (size_t k = 0; k < sizeof (piddesc_tables_output) / sizeof (piddesc_tables_output[0]); k++)
  {
    struct piddesc const * const table = piddesc_tables_output[k];
    for (uint32_t i = 0; table[i].pid != DDSI_PID_SENTINEL; i++)
    {
      struct piddesc const * const entry = &table[i];
      if (entry->pid == DDSI_PID_PAD)
        continue;
      if (((entry->flags & PDF_QOS) ? qw : pw) & entry->present_flag)
      {
        assert (entry->plist_offset >= shift);
        assert (shift == 0 || entry->plist_offset - shift < sizeof (dds_qos_t));
        size_t srcoff = entry->plist_offset - shift;
        /* convert name to lower case for making the trace easier on the eyes */
        char lcname[64];
        const size_t namelen = strlen (entry->name);
        assert (namelen < sizeof (lcname));
        for (size_t p = 0; p < namelen; p++)
          lcname[p] = (char) tolower (entry->name[p]);
        lcname[namelen] = 0;
        if (!prtf (buf, bufsize, "%s%s=", sep, lcname))
          return;
        sep = ",";
        bool cont;
        if (!(entry->flags & PDF_FUNCTION))
          cont = print_generic (buf, bufsize, src, srcoff, entry->op.desc);
        else
          cont = entry->op.f.print (buf, bufsize, src, srcoff);
        if (!cont)
          return;
      }
    }
  }
}

static void plist_or_xqos_log (uint32_t cat, const struct ddsrt_log_cfg *logcfg, const void * __restrict src, size_t shift, uint64_t pwanted, uint64_t qwanted)
{
  if (logcfg->c.mask & cat)
  {
    char tmp[2048], *ptmp = tmp;
    size_t tmpsize = sizeof (tmp);
    plist_or_xqos_print (&ptmp, &tmpsize, src, shift, pwanted, qwanted);
    DDS_CLOG (cat, logcfg, "%s", tmp);
  }
}

size_t ddsi_xqos_print (char * __restrict buf, size_t bufsize, const dds_qos_t *xqos)
{
  const size_t bufsize_in = bufsize;
  (void) prtf (&buf, &bufsize, "{");
  plist_or_xqos_print (&buf, &bufsize, xqos, offsetof (ddsi_plist_t, qos), 0, ~(uint64_t)0);
  (void) prtf (&buf, &bufsize, "}");
  return bufsize_in - bufsize;
}

size_t ddsi_plist_print (char * __restrict buf, size_t bufsize, const ddsi_plist_t *plist)
{
  const size_t bufsize_in = bufsize;
  (void) prtf (&buf, &bufsize, "{");
  plist_or_xqos_print (&buf, &bufsize, plist, 0, ~(uint64_t)0, ~(uint64_t)0);
  (void) prtf (&buf, &bufsize, "}");
  return bufsize_in - bufsize;
}

void ddsi_xqos_log (uint32_t cat, const struct ddsrt_log_cfg *logcfg, const dds_qos_t *xqos)
{
  plist_or_xqos_log (cat, logcfg, xqos, offsetof (ddsi_plist_t, qos), 0, ~(uint64_t)0);
}

void ddsi_plist_log (uint32_t cat, const struct ddsrt_log_cfg *logcfg, const ddsi_plist_t *plist)
{
  plist_or_xqos_log (cat, logcfg, plist, 0, ~(uint64_t)0, ~(uint64_t)0);
}

size_t ddsi_plist_print_generic (char * __restrict buf, size_t bufsize, const void * __restrict src, const enum ddsi_pserop * __restrict desc)
{
  const size_t bufsize_in = bufsize;
  (void) print_generic (&buf, &bufsize, src, 0, desc);
  return bufsize_in - bufsize;
}

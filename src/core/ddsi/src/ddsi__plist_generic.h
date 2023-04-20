// Copyright(c) 2019 to 2021 ZettaScale Technology and others
//
// This program and the accompanying materials are made available under the
// terms of the Eclipse Public License v. 2.0 which is available at
// http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
// v. 1.0 which is available at
// http://www.eclipse.org/org/documents/edl-v10.php.
//
// SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

#ifndef DDSI__PLIST_GENERIC_H
#define DDSI__PLIST_GENERIC_H

#include <stddef.h>
#include <assert.h>
#include <stdbool.h>

#include "dds/export.h"
#include "dds/ddsrt/bswap.h"
#include "dds/ddsrt/attributes.h"
#include "dds/ddsrt/retcode.h"

#if defined (__cplusplus)
extern "C" {
#endif

/* Instructions for the generic serializer (&c) that handles most parameters.
   The "packed" attribute means single-byte instructions on GCC and Clang. */
enum ddsi_pserop {
  XSTOP,
  XO, /* octet sequence */
  XS, /* string */
  XE1, XE2, XE3, /* enum 0..1, 0..2, 0..3; mapping to integral type assumed to be the same for all three */
  Xs, /* int16_t */
  Xi, Xix2, Xix3, Xix4, /* int32_t, 1 .. 4 in a row */
  Xu, Xux2, Xux3, Xux4, Xux5, /* uint32_t, 1 .. 5 in a row */
  XD, XDx2, /* duration, 1 .. 2 in a row */
  Xl,       /* int64_t */
  Xo, Xox2, /* octet, 1 .. 2 in a row */
  Xb, Xbx2, Xbx3, Xbx4, Xbx5, /* boolean, 1 .. 5 in a row */
  XbCOND, /* boolean: compare to ignore remainder if false (for use_... flags) */
  XbPROP, /* boolean: omit in serialized form; skip serialization if false; always true on deserialize */
  XG, /* GUID */
  XK, /* keyhash */
  XQ, /* arbitary non-nested sequence */
  Xopt, /* remainder is optional on deser, 0-init if not present */
} ddsrt_attribute_packed;

/** @component parameter_list */
void ddsi_plist_fini_generic (void * __restrict dst, const enum ddsi_pserop *desc, bool aliased);

/** @component parameter_list */
void ddsi_plist_ser_generic_size_embeddable (size_t *dstoff, const void *src, size_t srcoff, const enum ddsi_pserop * __restrict desc);

/** @component parameter_list */
dds_return_t ddsi_plist_deser_generic (void * __restrict dst, const void * __restrict src, size_t srcsize, bool bswap, const enum ddsi_pserop * __restrict desc);

/** @component parameter_list */
dds_return_t ddsi_plist_deser_generic_srcoff (void * __restrict dst, const void * __restrict src, size_t srcsize, size_t *srcoff, bool bswap, const enum ddsi_pserop * __restrict desc);

/** @component parameter_list */
dds_return_t ddsi_plist_ser_generic_embeddable (char * const data, size_t *dstoff, const void *src, size_t srcoff, const enum ddsi_pserop * __restrict desc, enum ddsrt_byte_order_selector bo);

/** @component parameter_list */
dds_return_t ddsi_plist_ser_generic (void **dst, size_t *dstsize, const void *src, const enum ddsi_pserop * __restrict desc);

/** @component parameter_list */
dds_return_t ddsi_plist_ser_generic_be (void **dst, size_t *dstsize, const void *src, const enum ddsi_pserop * __restrict desc);

/** @component parameter_list */
dds_return_t ddsi_plist_unalias_generic (void * __restrict dst, const enum ddsi_pserop * __restrict desc);

/** @component parameter_list */
bool ddsi_plist_equal_generic (const void *srcx, const void *srcy, const enum ddsi_pserop * __restrict desc);

/** @component parameter_list */
size_t ddsi_plist_memsize_generic (const enum ddsi_pserop * __restrict desc);

/** @component parameter_list */
size_t ddsi_plist_print_generic (char * __restrict buf, size_t bufsize, const void * __restrict src, const enum ddsi_pserop * __restrict desc);

#if defined (__cplusplus)
}
#endif

#endif /* DDSI__PLIST_GENERIC_H */

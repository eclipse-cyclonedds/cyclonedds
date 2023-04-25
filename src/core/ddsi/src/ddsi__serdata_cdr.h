// Copyright(c) 2006 to 2022 ZettaScale Technology and others
//
// This program and the accompanying materials are made available under the
// terms of the Eclipse Public License v. 2.0 which is available at
// http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
// v. 1.0 which is available at
// http://www.eclipse.org/org/documents/edl-v10.php.
//
// SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

#ifndef DDSI__SERDATA_CDR_H
#define DDSI__SERDATA_CDR_H

#include "dds/ddsrt/endian.h"
#include "dds/ddsrt/avl.h"
#include "dds/ddsi/ddsi_freelist.h"
#include "dds/ddsi/ddsi_serdata.h"
#include "dds/ddsi/ddsi_typelib.h"
#include "ddsi__protocol.h"
#include "ddsi__typelookup.h"
#include "dds/cdr/dds_cdrstream.h"
#include "dds/dds.h"

#if defined (__cplusplus)
extern "C" {
#endif

/* There is an alignment requirement on the raw data (it must be at
   offset mod 8 for the conversion to/from a dds_stream to work).
   So we define two types: one without any additional padding, and
   one where the appropriate amount of padding is inserted */
#define DDSI_SERDATA_CDR_PREPAD   \
  struct ddsi_serdata c;          \
  uint32_t pos;                   \
  uint32_t size
/* We suppress the zero-array warning (MSVC C4200) here ONLY for MSVC
   and ONLY if it is being compiled as C++ code, as it only causes
   issues there */
#if defined _MSC_VER && defined __cplusplus
#define DDSI_SERDATA_CDR_POSTPAD  \
  struct dds_cdr_header hdr;      \
  DDSRT_WARNING_MSVC_OFF(4200)    \
  char data[];                    \
  DDSRT_WARNING_MSVC_ON(4200)
#else
#define DDSI_SERDATA_CDR_POSTPAD  \
  struct dds_cdr_header hdr;      \
  char data[]
#endif

struct ddsi_serdata_cdr_unpadded {
  DDSI_SERDATA_CDR_PREPAD;
  DDSI_SERDATA_CDR_POSTPAD;
};

#ifdef __GNUC__
#define DDSI_SERDATA_CDR_PAD(n) ((n) % 8)
#else
#define DDSI_SERDATA_CDR_PAD(n) (n)
#endif

struct ddsi_serdata_cdr {
  DDSI_SERDATA_CDR_PREPAD;
  char pad[DDSI_SERDATA_CDR_PAD (8 - (offsetof (struct ddsi_serdata_cdr_unpadded, data) % 8))];
  DDSI_SERDATA_CDR_POSTPAD;
};

DDSRT_STATIC_ASSERT ((offsetof (struct ddsi_serdata_cdr, data) % 8) == 0);

#undef DDSI_SERDATA_CDR_PAD
#undef DDSI_SERDATA_CDR_POSTPAD
#undef DDSI_SERDATA_CDR_PREPAD
#undef DDSI_SERDATA_CDR_FIXED_FIELD

/**
 * @brief Sertype used for built-in type look service
 *
 * This sertype is used for the built-in type lookup service endpoints,
 * which require mutable and appendable types for exchanging XTypes
 * type information. The serdata_cdr implementation uses the CDR stream
 * serializer.
 *
 * @note: This type is specialized for types that have no key fields
 */
struct ddsi_sertype_cdr {
  struct ddsi_sertype c;
  uint16_t encoding_format; /* DDSI_RTPS_CDR_ENC_FORMAT_(PLAIN|DELIMITED|PL) - CDR encoding format for the top-level type in this sertype */
  struct dds_cdrstream_desc type;
};

extern const struct ddsi_serdata_ops ddsi_serdata_ops_cdr;

/** @component typesupport_cdr */
dds_return_t ddsi_sertype_cdr_init (const struct ddsi_domaingv *gv, struct ddsi_sertype_cdr *st, const dds_topic_descriptor_t *desc);

#if defined (__cplusplus)
}
#endif

#endif /* DDSI__SERDATA_CDR_H */

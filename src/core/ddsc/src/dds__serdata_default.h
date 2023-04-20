// Copyright(c) 2006 to 2022 ZettaScale Technology and others
//
// This program and the accompanying materials are made available under the
// terms of the Eclipse Public License v. 2.0 which is available at
// http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
// v. 1.0 which is available at
// http://www.eclipse.org/org/documents/edl-v10.php.
//
// SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

#ifndef DDS__SERDATA_DEFAULT_H
#define DDS__SERDATA_DEFAULT_H

#include "dds/ddsrt/endian.h"
#include "dds/ddsrt/avl.h"
#include "dds/ddsi/ddsi_protocol.h"
#include "dds/ddsi/ddsi_freelist.h"
#include "dds/ddsi/ddsi_serdata.h"
#include "dds/ddsi/ddsi_typelib.h"
#include "dds/cdr/dds_cdrstream.h"
#include "dds__types.h"
#include "dds/dds.h"

#if defined (__cplusplus)
extern "C" {
#endif

#define KEYBUFTYPE_UNSET    0u
#define KEYBUFTYPE_STATIC   1u // uses u.stbuf
#define KEYBUFTYPE_DYNALIAS 2u // points into payload
#define KEYBUFTYPE_DYNALLOC 3u // dynamically allocated

#define SERDATA_DEFAULT_KEYSIZE_MASK        0x3FFFFFFFu

struct dds_serdata_default_key {
  unsigned buftype : 2;
  unsigned keysize : 30;
  union {
    unsigned char stbuf[DDS_FIXED_KEY_MAX_SIZE];
    unsigned char *dynbuf;
  } u;
};

struct dds_serdatapool {
  struct ddsi_freelist freelist;
};

/* Debug builds may want to keep some additional state */
#ifndef NDEBUG
#define DDS_SERDATA_DEFAULT_DEBUG_FIELDS \
  bool fixed;
#else
#define DDS_SERDATA_DEFAULT_DEBUG_FIELDS
#endif

/* There is an alignment requirement on the raw data (it must be at
   offset mod 8 for the conversion to/from a dds_stream to work).
   So we define two types: one without any additional padding, and
   one where the appropriate amount of padding is inserted */
#define DDS_SERDATA_DEFAULT_PREPAD    \
  struct ddsi_serdata c;              \
  uint32_t pos;                       \
  uint32_t size;                      \
  DDS_SERDATA_DEFAULT_DEBUG_FIELDS    \
  struct dds_serdata_default_key key; \
  struct dds_serdatapool *serpool;    \
  struct dds_serdata_default *next /* in pool->freelist */
/* We suppress the zero-array warning (MSVC C4200) here ONLY for MSVC
   and ONLY if it is being compiled as C++ code, as it only causes
   issues there */
#if defined _MSC_VER && defined __cplusplus
#define DDS_SERDATA_DEFAULT_POSTPAD   \
  struct dds_cdr_header hdr;          \
  DDSRT_WARNING_MSVC_OFF(4200)        \
  char data[];                        \
  DDSRT_WARNING_MSVC_ON(4200)
#else
#define DDS_SERDATA_DEFAULT_POSTPAD   \
  struct dds_cdr_header hdr;          \
  char data[]
#endif

struct dds_serdata_default_unpadded {
  DDS_SERDATA_DEFAULT_PREPAD;
  DDS_SERDATA_DEFAULT_POSTPAD;
};

#ifdef __GNUC__
#define DDS_SERDATA_DEFAULT_PAD(n) ((n) % 8)
#else
#define DDS_SERDATA_DEFAULT_PAD(n) (n)
#endif

struct dds_serdata_default {
  DDS_SERDATA_DEFAULT_PREPAD;
  char pad[DDS_SERDATA_DEFAULT_PAD (8 - (offsetof (struct dds_serdata_default_unpadded, data) % 8))];
  DDS_SERDATA_DEFAULT_POSTPAD;
};

DDSRT_STATIC_ASSERT ((offsetof (struct dds_serdata_default, data) % 8) == 0);

#undef DDS_SERDATA_DEFAULT_PAD
#undef DDS_SERDATA_DEFAULT_POSTPAD
#undef DDS_SERDATA_DEFAULT_PREPAD
#undef DDS_SERDATA_DEFAULT_FIXED_FIELD

struct dds_sertype_default_cdr_data {
  uint32_t sz;
  unsigned char *data;
};

struct dds_sertype_default {
  struct ddsi_sertype c;
  uint16_t encoding_format; /* DDSI_RTPS_CDR_ENC_FORMAT_(PLAIN|DELIMITED|PL) - CDR encoding format for the top-level type in this sertype */
  uint16_t write_encoding_version; /* DDSI_RTPS_CDR_ENC_VERSION_(1|2) - CDR encoding version used for writing data using this sertype */
  struct dds_serdatapool *serpool;
  struct dds_cdrstream_desc type;
  struct dds_sertype_default_cdr_data typeinfo_ser;
  struct dds_sertype_default_cdr_data typemap_ser;
};

extern const struct ddsi_sertype_ops dds_sertype_ops_default;

extern const struct ddsi_serdata_ops dds_serdata_ops_cdr;
extern const struct ddsi_serdata_ops dds_serdata_ops_cdr_nokey;

extern const struct ddsi_serdata_ops dds_serdata_ops_xcdr2;
extern const struct ddsi_serdata_ops dds_serdata_ops_xcdr2_nokey;

/** @component typesupport_c */
struct dds_serdatapool * dds_serdatapool_new (void);

/** @component typesupport_c */
void dds_serdatapool_free (struct dds_serdatapool * pool);

/** @component typesupport_c */
dds_return_t dds_sertype_default_init (const struct dds_domain *domain, struct dds_sertype_default *st, const dds_topic_descriptor_t *desc, uint16_t min_xcdrv, dds_data_representation_id_t data_representation);

#if defined (__cplusplus)
}
#endif

#endif /* DDS__SERDATA_DEFAULT_H */

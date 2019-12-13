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
#ifndef DDSI_SERDATA_DEFAULT_H
#define DDSI_SERDATA_DEFAULT_H

#include "dds/ddsrt/endian.h"
#include "dds/ddsi/q_protocol.h" /* for nn_parameterid_t */
#include "dds/ddsi/q_freelist.h"
#include "dds/ddsrt/avl.h"
#include "dds/ddsi/ddsi_serdata.h"
#include "dds/ddsi/ddsi_sertopic.h"

#include "dds/dds.h"
#include "dds__topic.h"

#if defined (__cplusplus)
extern "C" {
#endif

#if DDSRT_ENDIAN == DDSRT_LITTLE_ENDIAN
#define CDR_BE 0x0000
#define CDR_LE 0x0100
#else
#define CDR_BE 0x0000
#define CDR_LE 0x0001
#endif

struct CDRHeader {
  unsigned short identifier;
  unsigned short options;
};

struct serdatapool {
  struct nn_freelist freelist;
};

typedef struct dds_keyhash {
  unsigned char m_hash [16]; /* Key hash value. Also possibly key. Suitably aligned for accessing as uint32_t's */
  unsigned m_set : 1;        /* has it been initialised? */
  unsigned m_iskey : 1;      /* m_hash is key value */
  unsigned m_keysize : 5;    /* size of the key within the hash buffer */
} dds_keyhash_t;

/* Debug builds may want to keep some additional state */
#ifndef NDEBUG
#define DDSI_SERDATA_DEFAULT_DEBUG_FIELDS \
  bool fixed;
#else
#define DDSI_SERDATA_DEFAULT_DEBUG_FIELDS
#endif

/* There is an alignment requirement on the raw data (it must be at
   offset mod 8 for the conversion to/from a dds_stream to work).
   So we define two types: one without any additional padding, and
   one where the appropriate amount of padding is inserted */
#define DDSI_SERDATA_DEFAULT_PREPAD   \
  struct ddsi_serdata c;              \
  uint32_t pos;                       \
  uint32_t size;                      \
  DDSI_SERDATA_DEFAULT_DEBUG_FIELDS   \
  dds_keyhash_t keyhash;              \
  struct serdatapool *serpool;        \
  struct ddsi_serdata_default *next /* in pool->freelist */
#define DDSI_SERDATA_DEFAULT_POSTPAD  \
  struct CDRHeader hdr;               \
  char data[]

struct ddsi_serdata_default_unpadded {
  DDSI_SERDATA_DEFAULT_PREPAD;
  DDSI_SERDATA_DEFAULT_POSTPAD;
};

#ifdef __GNUC__
#define DDSI_SERDATA_DEFAULT_PAD(n) ((n) % 8)
#else
#define DDSI_SERDATA_DEFAULT_PAD(n) (n)
#endif

struct ddsi_serdata_default {
  DDSI_SERDATA_DEFAULT_PREPAD;
  char pad[DDSI_SERDATA_DEFAULT_PAD (8 - (offsetof (struct ddsi_serdata_default_unpadded, data) % 8))];
  DDSI_SERDATA_DEFAULT_POSTPAD;
};

#undef DDSI_SERDATA_DEFAULT_PAD
#undef DDSI_SERDATA_DEFAULT_POSTPAD
#undef DDSI_SERDATA_DEFAULT_PREPAD
#undef DDSI_SERDATA_DEFAULT_FIXED_FIELD

struct dds_key_descriptor;
struct dds_topic_descriptor;

#ifndef DDS_TOPIC_INTERN_FILTER_FN_DEFINED
#define DDS_TOPIC_INTERN_FILTER_FN_DEFINED
typedef bool (*dds_topic_intern_filter_fn) (const void * sample, void *ctx);
#endif

struct ddsi_sertopic_default {
  struct ddsi_sertopic c;
  uint16_t native_encoding_identifier; /* (PL_)?CDR_(LE|BE) */
  struct serdatapool *serpool;

  struct dds_topic_descriptor * type;
  unsigned nkeys;

  uint32_t flags;
  size_t opt_size;
  dds_topic_intern_filter_fn filter_fn;
  void * filter_sample;
  void * filter_ctx;
  const struct dds_key_descriptor * keys;

  /*
    Array of keys, represented as offset in the OpenSplice internal
    format data blob. Keys must be stored in the order visited by
    serializer (so that the serializer can simply compare the current
    offset with the next key offset). Also: keys[nkeys].off =def=
    ~0u, which won't equal any real offset so that there is no need
    to test for the end of the array.

    Offsets work 'cos only primitive types, enums and strings are
    accepted as keys. So there is no ambiguity if a key happens to
    be inside a nested struct.
  */
};

struct ddsi_plist_sample {
  void *blob;
  size_t size;
  nn_parameterid_t keyparam;
};

struct ddsi_rawcdr_sample {
  void *blob;
  size_t size;
  void *key;
  size_t keysize;
};

extern DDS_EXPORT const struct ddsi_sertopic_ops ddsi_sertopic_ops_default;

extern DDS_EXPORT const struct ddsi_serdata_ops ddsi_serdata_ops_cdr;
extern DDS_EXPORT const struct ddsi_serdata_ops ddsi_serdata_ops_cdr_nokey;
extern DDS_EXPORT const struct ddsi_serdata_ops ddsi_serdata_ops_plist;
extern DDS_EXPORT const struct ddsi_serdata_ops ddsi_serdata_ops_rawcdr;

struct serdatapool * ddsi_serdatapool_new (void);
void ddsi_serdatapool_free (struct serdatapool * pool);

#if defined (__cplusplus)
}
#endif

#endif

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
#ifndef DDSI_SER_H
#define DDSI_SER_H

#include "os/os.h"
#include "ddsi/q_plist.h" /* for nn_prismtech_writer_info */
#include "ddsi/q_freelist.h"
#include "util/ut_avl.h"
#include "sysdeps.h"
#include "ddsi/ddsi_serdata.h"
#include "ddsi/ddsi_sertopic.h"

#include "ddsc/dds.h"
#include "dds__topic.h"

#ifndef PLATFORM_IS_LITTLE_ENDIAN
#  if OS_ENDIANNESS == OS_BIG_ENDIAN
#    define PLATFORM_IS_LITTLE_ENDIAN 0
#  elif OS_ENDIANNESS == OS_LITTLE_ENDIAN
#    define PLATFORM_IS_LITTLE_ENDIAN 1
#  else
#    error "invalid endianness setting"
#  endif
#endif /* PLATFORM_IS_LITTLE_ENDIAN */

#if PLATFORM_IS_LITTLE_ENDIAN
#define CDR_BE 0x0000
#define CDR_LE 0x0100
#else
#define CDR_BE 0x0000
#define CDR_LE 0x0001
#endif

struct CDRHeader
{
  unsigned short identifier;
  unsigned short options;
};

struct serstatepool /* FIXME: now a serdatapool */
{
  struct nn_freelist freelist;
};

struct serstate {
  struct ddsi_serdata_default *data;
};

#define DDS_KEY_SET 0x0001
#define DDS_KEY_HASH_SET 0x0002
#define DDS_KEY_IS_HASH 0x0004

typedef struct dds_key_hash
{
  char m_hash [16];          /* Key hash value. Also possibly key. Suitably aligned for accessing as uint32_t's */
  uint32_t m_key_len;        /* Length of key (may be in m_hash or m_key_buff) */
  uint32_t m_key_buff_size;  /* Size of allocated key buffer (m_key_buff) */
  char * m_key_buff;         /* Key buffer */
  uint32_t m_flags;          /* State of key/hash (see DDS_KEY_XXX) */
}
dds_key_hash_t;

struct ddsi_serdata_default
{
  struct ddsi_serdata c;
  uint32_t pos;
  uint32_t size;
  bool bswap;
#ifndef NDEBUG
  bool fixed;
#endif
  dds_key_hash_t keyhash;

  struct serstatepool *pool;
  struct ddsi_serdata_default *next; /* in pool->freelist */

  /* padding to ensure CDRHeader is at an offset 4 mod 8 from the
     start of the memory, so that data is 8-byte aligned provided
     serdata is 8-byte aligned */
  char pad[8 - ((sizeof (struct ddsi_serdata) + 4) % 8)];
  struct CDRHeader hdr;
  char data[1];
};

struct dds_key_descriptor;

#ifndef DDS_TOPIC_INTERN_FILTER_FN_DEFINED
#define DDS_TOPIC_INTERN_FILTER_FN_DEFINED
typedef bool (*dds_topic_intern_filter_fn) (const void * sample, void *ctx);
#endif

struct ddsi_sertopic_default
{
  struct ddsi_sertopic c;
  uint16_t native_encoding_identifier; /* (PL_)?CDR_(LE|BE) */

  void * type;
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

extern const struct ddsi_sertopic_ops ddsi_sertopic_ops_default;

extern const struct ddsi_serdata_ops ddsi_serdata_ops_cdr;
extern const struct ddsi_serdata_ops ddsi_serdata_ops_plist;
extern const struct ddsi_serdata_ops ddsi_serdata_ops_rawcdr;

struct serstatepool * ddsi_serstatepool_new (void);
void ddsi_serstatepool_free (struct serstatepool * pool);

OSAPI_EXPORT void ddsi_serstate_append_blob (struct serstate * st, size_t align, size_t sz, const void *data);
void * ddsi_serstate_append (struct serstate * st, size_t n);
void * ddsi_serstate_append_aligned (struct serstate * st, size_t n, size_t a);

#endif

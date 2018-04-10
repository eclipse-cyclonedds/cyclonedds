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

typedef struct serstatepool * serstatepool_t;
typedef struct serstate * serstate_t;
typedef struct serdata * serdata_t;
typedef struct sertopic * sertopic_t;

struct CDRHeader
{
  unsigned short identifier;
  unsigned short options;
};

struct serdata_msginfo
{
  unsigned statusinfo;
  nn_wctime_t timestamp;
};

enum serstate_kind {
  STK_EMPTY,
  STK_KEY,
  STK_DATA
};

struct serstate
{
  serdata_t data;
  nn_mtime_t twrite; /* write time, not source timestamp, set post-throttling */
  os_atomic_uint32_t refcount;
  size_t pos;
  size_t size;
  const struct sertopic * topic;
  enum serstate_kind kind;
  serstatepool_t pool;
  struct serstate *next; /* in pool->freelist */
};

struct serstatepool
{
  struct nn_freelist freelist;
};


#define DDS_KEY_SET 0x0001
#define DDS_KEY_HASH_SET 0x0002
#define DDS_KEY_IS_HASH 0x0004

typedef struct dds_key_hash
{
  char m_hash [16];          /* Key hash value. Also possibly key. */
  uint32_t m_key_len;        /* Length of key (may be in m_hash or m_key_buff) */
  uint32_t m_key_buff_size;  /* Size of allocated key buffer (m_key_buff) */
  char * m_key_buff;         /* Key buffer */
  uint32_t m_flags;          /* State of key/hash (see DDS_KEY_XXX) */
}
dds_key_hash_t;

struct serdata_base
{
  serstate_t st;        /* back pointer to (opaque) serstate so RTPS impl only needs serdata */
  struct serdata_msginfo msginfo;
  int hash_valid;       /* whether hash is valid or must be computed from key/data */
  uint32_t hash;       /* cached serdata hash, valid only if hash_valid != 0 */
  dds_key_hash_t keyhash;
  bool bswap;           /* Whether state is native endian or requires swapping */
};

struct serdata
{
  struct serdata_base v;
  /* padding to ensure CDRHeader is at an offset 4 mod 8 from the
     start of the memory, so that data is 8-byte aligned provided
     serdata is 8-byte aligned */
  char pad[8 - ((sizeof (struct serdata_base) + 4) % 8)];
  struct CDRHeader hdr;
  char data[1];
};


struct dds_key_descriptor;


struct dds_topic;
typedef void (*topic_cb_t) (struct dds_topic * topic);
#ifndef DDS_TOPIC_INTERN_FILTER_FN_DEFINED
#define DDS_TOPIC_INTERN_FILTER_FN_DEFINED
typedef bool (*dds_topic_intern_filter_fn) (const void * sample, void *ctx);
#endif

struct sertopic
{
  ut_avlNode_t avlnode;
  char * name_typename;
  char * name;
  char * typename;
  void * type;
  unsigned nkeys;

  uint32_t id;
  uint32_t hash;
  uint32_t flags;
  size_t opt_size;
  os_atomic_uint32_t refcount;
  topic_cb_t status_cb;
  dds_topic_intern_filter_fn filter_fn;
  void * filter_sample;
  void * filter_ctx;
  struct dds_topic * status_cb_entity;
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

serstatepool_t ddsi_serstatepool_new (void);
void ddsi_serstatepool_free (serstatepool_t pool);

serdata_t ddsi_serdata_ref (serdata_t serdata);
OSAPI_EXPORT void ddsi_serdata_unref (serdata_t serdata);
int ddsi_serdata_refcount_is_1 (serdata_t serdata);
nn_mtime_t ddsi_serdata_twrite (const struct serdata * serdata);
void ddsi_serdata_set_twrite (struct serdata * serdata, nn_mtime_t twrite);
uint32_t ddsi_serdata_size (const struct serdata * serdata);
int ddsi_serdata_is_key (const struct serdata * serdata);
int ddsi_serdata_is_empty (const struct serdata * serdata);

OSAPI_EXPORT void ddsi_serstate_append_blob (serstate_t st, size_t align, size_t sz, const void *data);
OSAPI_EXPORT void ddsi_serstate_set_msginfo
(
  serstate_t st, unsigned statusinfo, nn_wctime_t timestamp,
  void * dummy
);
OSAPI_EXPORT serstate_t ddsi_serstate_new (serstatepool_t pool, const struct sertopic * topic);
OSAPI_EXPORT serdata_t ddsi_serstate_fix (serstate_t st);
nn_mtime_t ddsi_serstate_twrite (const struct serstate *serstate);
void ddsi_serstate_set_twrite (struct serstate *serstate, nn_mtime_t twrite);
void ddsi_serstate_release (serstate_t st);
void * ddsi_serstate_append (serstate_t st, size_t n);
void * ddsi_serstate_append_align (serstate_t st, size_t a);
void * ddsi_serstate_append_aligned (serstate_t st, size_t n, size_t a);

#endif

// Copyright(c) 2006 to 2022 ZettaScale Technology and others
//
// This program and the accompanying materials are made available under the
// terms of the Eclipse Public License v. 2.0 which is available at
// http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
// v. 1.0 which is available at
// http://www.eclipse.org/org/documents/edl-v10.php.
//
// SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

#ifndef DDS_CDRSTREAM_H
#define DDS_CDRSTREAM_H

#include "dds/ddsrt/bswap.h"
#include "dds/ddsrt/static_assert.h"
#include "dds/dds.h"

#if defined (__cplusplus)
extern "C" {
#endif

#define DDS_CDRSTREAM_MAX_NESTING_DEPTH 32  /* maximum level of nesting for key extraction */

/*
  Encoding version to be used for serialization. Encoding version 1
  represents the XCDR1 format as defined in the DDS XTypes specification,
  with PLAIN_CDR(1) that is backwards compatible with the CDR encoding
  used by non-XTypes enabled nodes.
*/
#define DDSI_RTPS_CDR_ENC_VERSION_UNDEF     0
#define DDSI_RTPS_CDR_ENC_VERSION_1         1
#define DDSI_RTPS_CDR_ENC_VERSION_2         2

#define DDSI_RTPS_CDR_ENC_FORMAT_PLAIN      0
#define DDSI_RTPS_CDR_ENC_FORMAT_DELIMITED  1
#define DDSI_RTPS_CDR_ENC_FORMAT_PL         2

/* X-Types spec 7.6.3.1.2: Implementations of this specification shall set the
least significant two bits in the second byte of the options field to a value
that encodes the number of padding bytes needed after the end of the serialized
payload in order to reach the next 4-byte aligned offset. */
#define DDS_CDR_HDR_PADDING_MASK 0x3

struct dds_cdr_header {
  unsigned short identifier;
  unsigned short options;
};

enum dds_cdr_type_extensibility
{
  DDS_CDR_TYPE_EXT_FINAL = 0,
  DDS_CDR_TYPE_EXT_APPENDABLE = 1,
  DDS_CDR_TYPE_EXT_MUTABLE = 2
};

typedef struct dds_istream {
  const unsigned char *m_buffer;
  uint32_t m_size;          /* Buffer size */
  uint32_t m_index;         /* Read/write offset from start of buffer */
  uint32_t m_xcdr_version;  /* XCDR version of the data */
} dds_istream_t;

typedef struct dds_ostream {
  unsigned char *m_buffer;
  uint32_t m_size;          /* Buffer size */
  uint32_t m_index;         /* Read/write offset from start of buffer */
  uint32_t m_xcdr_version;  /* XCDR version to use for serializing data */
} dds_ostream_t;

typedef struct dds_ostreamBE {
  dds_ostream_t x;
} dds_ostreamBE_t;

typedef struct dds_ostreamLE {
  dds_ostream_t x;
} dds_ostreamLE_t;

typedef struct dds_cdrstream_allocator {
  void* (*malloc) (size_t size);
  void* (*realloc) (void *ptr, size_t new_size);
  void (*free) (void *pt);
  /* In a future version, a void ptr may be needed here as a parameter for
     custom allocator implementations. */
} dds_cdrstream_allocator_t;

typedef struct dds_cdrstream_desc_key {
  uint32_t ops_offs;   /* Offset for key ops */
  uint32_t idx;        /* Key index (used for key order) */
} dds_cdrstream_desc_key_t;

typedef struct dds_cdrstream_desc_key_seq {
  uint32_t nkeys;
  dds_cdrstream_desc_key_t *keys;
} dds_cdrstream_desc_key_seq_t;

typedef struct dds_cdrstream_desc_op_seq {
  uint32_t nops;    /* Number of words in ops (which >= number of ops stored in preproc output) */
  uint32_t *ops;    /* Marshalling meta data */
} dds_cdrstream_desc_op_seq_t;

struct dds_cdrstream_desc {
  uint32_t size;    /* Size of type */
  uint32_t align;   /* Alignment of top-level type */
  uint32_t flagset; /* Flags */
  dds_cdrstream_desc_key_seq_t keys;
  dds_cdrstream_desc_op_seq_t ops;
  size_t opt_size_xcdr1;
  size_t opt_size_xcdr2;
};

DDSRT_STATIC_ASSERT (offsetof (dds_ostreamLE_t, x) == 0);
DDSRT_STATIC_ASSERT (offsetof (dds_ostreamBE_t, x) == 0);

/** @component cdr_serializer */
uint32_t dds_cdr_alignto4_clear_and_resize (dds_ostream_t * __restrict os, const struct dds_cdrstream_allocator * __restrict allocator, uint32_t xcdr_version);

/** @component cdr_serializer */
DDS_EXPORT void dds_istream_init (dds_istream_t * __restrict is, uint32_t size, const void * __restrict input, uint32_t xcdr_version);

/** @component cdr_serializer */
DDS_EXPORT void dds_istream_fini (dds_istream_t * __restrict is);

/** @component cdr_serializer */
DDS_EXPORT void dds_ostream_init (dds_ostream_t * __restrict os, const struct dds_cdrstream_allocator * __restrict allocator, uint32_t size, uint32_t xcdr_version);

/** @component cdr_serializer */
DDS_EXPORT void dds_ostream_fini (dds_ostream_t * __restrict os, const struct dds_cdrstream_allocator * __restrict allocator);

/** @component cdr_serializer */
DDS_EXPORT void dds_ostreamLE_init (dds_ostreamLE_t * __restrict os, const struct dds_cdrstream_allocator * __restrict allocator, uint32_t size, uint32_t xcdr_version);

/** @component cdr_serializer */
DDS_EXPORT void dds_ostreamLE_fini (dds_ostreamLE_t * __restrict os, const struct dds_cdrstream_allocator * __restrict allocator);

/** @component cdr_serializer */
DDS_EXPORT void dds_ostreamBE_init (dds_ostreamBE_t * __restrict os, const struct dds_cdrstream_allocator * __restrict allocator, uint32_t size, uint32_t xcdr_version);

/** @component cdr_serializer */
DDS_EXPORT void dds_ostreamBE_fini (dds_ostreamBE_t * __restrict os, const struct dds_cdrstream_allocator * __restrict allocator);

/** @component cdr_serializer */
dds_ostream_t dds_ostream_from_buffer(void *buffer, size_t size, uint16_t write_encoding_version);

/**
 * @brief Normalized and validates CDR data
 * @component cdr_serializer
 *
 * @param data          data sample
 * @param size          size of the data
 * @param bswap         byte-swapping required
 * @param xcdr_version  XCDR version of the CDR data
 * @param type          type descriptor
 * @param just_key      indicates if the data is a serialized key or a complete sample
 * @param actual_size   is set to the actual size of the data (*actual_size <= size) on successful return
 * @returns             True iff validation and normalization succeeded
 */
DDS_EXPORT bool dds_stream_normalize (void * __restrict data, uint32_t size, bool bswap, uint32_t xcdr_version, const struct dds_cdrstream_desc * __restrict type, bool just_key, uint32_t * __restrict actual_size) ddsrt_attribute_warn_unused_result ddsrt_nonnull_all;

/** @component cdr_serializer */
DDS_EXPORT const uint32_t *dds_stream_normalize_data (char * __restrict data, uint32_t * __restrict off, uint32_t size, bool bswap, uint32_t xcdr_version, const uint32_t * __restrict ops) ddsrt_attribute_warn_unused_result ddsrt_nonnull_all;

/** @component cdr_serializer */
DDS_EXPORT const uint32_t *dds_stream_write (dds_ostream_t * __restrict os, const struct dds_cdrstream_allocator * __restrict allocator, const char * __restrict data, const uint32_t * __restrict ops);

/** @component cdr_serializer */
DDS_EXPORT const uint32_t *dds_stream_writeLE (dds_ostreamLE_t * __restrict os, const struct dds_cdrstream_allocator * __restrict allocator, const char * __restrict data, const uint32_t * __restrict ops);

/** @component cdr_serializer */
DDS_EXPORT const uint32_t *dds_stream_writeBE (dds_ostreamBE_t * __restrict os, const struct dds_cdrstream_allocator * __restrict allocator, const char * __restrict data, const uint32_t * __restrict ops);

/** @component cdr_serializer */
DDS_EXPORT const uint32_t * dds_stream_write_with_byte_order (dds_ostream_t * __restrict os, const struct dds_cdrstream_allocator * __restrict allocator, const char * __restrict data, const uint32_t * __restrict ops, enum ddsrt_byte_order_selector bo);

/** @component cdr_serializer */
DDS_EXPORT bool dds_stream_write_sample (dds_ostream_t * __restrict os, const struct dds_cdrstream_allocator * __restrict allocator, const void * __restrict data, const struct dds_cdrstream_desc * __restrict type);

/** @component cdr_serializer */
DDS_EXPORT bool dds_stream_write_sampleLE (dds_ostreamLE_t * __restrict os, const struct dds_cdrstream_allocator * __restrict allocator, const void * __restrict data, const struct dds_cdrstream_desc * __restrict type);

/** @component cdr_serializer */
DDS_EXPORT bool dds_stream_write_sampleBE (dds_ostreamBE_t * __restrict os, const struct dds_cdrstream_allocator * __restrict allocator, const void * __restrict data, const struct dds_cdrstream_desc * __restrict type);

/** @component cdr_serializer */
DDS_EXPORT void dds_stream_read_sample (dds_istream_t * __restrict is, void * __restrict data, const struct dds_cdrstream_allocator * __restrict allocator, const struct dds_cdrstream_desc * __restrict type);

/** @component cdr_serializer */
DDS_EXPORT void dds_stream_free_sample (void * __restrict data, const struct dds_cdrstream_allocator * __restrict allocator, const uint32_t * __restrict ops);

/** @component cdr_serializer */
DDS_EXPORT uint32_t dds_stream_countops (const uint32_t * __restrict ops, uint32_t nkeys, const dds_key_descriptor_t * __restrict keys);

/** @component cdr_serializer */
size_t dds_stream_check_optimize (const struct dds_cdrstream_desc * __restrict desc, uint32_t xcdr_version);

/** @component cdr_serializer */
void dds_stream_write_key (dds_ostream_t * __restrict os, const struct dds_cdrstream_allocator * __restrict allocator, const char * __restrict sample, const struct dds_cdrstream_desc * __restrict type);

/** @component cdr_serializer */
void dds_stream_write_keyBE (dds_ostreamBE_t * __restrict os, const struct dds_cdrstream_allocator * __restrict allocator, const char * __restrict sample, const struct dds_cdrstream_desc * __restrict type);

/** @component cdr_serializer */
DDS_EXPORT bool dds_stream_extract_key_from_data (dds_istream_t * __restrict is, dds_ostream_t * __restrict os, const struct dds_cdrstream_allocator * __restrict allocator, const struct dds_cdrstream_desc * __restrict type);

/** @component cdr_serializer */
DDS_EXPORT void dds_stream_extract_key_from_key (dds_istream_t * __restrict is, dds_ostream_t * __restrict os, const struct dds_cdrstream_allocator * __restrict allocator, const struct dds_cdrstream_desc * __restrict type);

/** @component cdr_serializer */
DDS_EXPORT bool dds_stream_extract_keyBE_from_data (dds_istream_t * __restrict is, dds_ostreamBE_t * __restrict os, const struct dds_cdrstream_allocator * __restrict allocator, const struct dds_cdrstream_desc * __restrict type);

/** @component cdr_serializer */
DDS_EXPORT void dds_stream_extract_keyBE_from_key (dds_istream_t * __restrict is, dds_ostreamBE_t * __restrict os, const struct dds_cdrstream_allocator * __restrict allocator, const struct dds_cdrstream_desc * __restrict type);

/** @component cdr_serializer */
DDS_EXPORT const uint32_t *dds_stream_read (dds_istream_t * __restrict is, char * __restrict data, const struct dds_cdrstream_allocator * __restrict allocator, const uint32_t * __restrict ops);

/** @component cdr_serializer */
DDS_EXPORT void dds_stream_read_key (dds_istream_t * __restrict is, char * __restrict sample, const struct dds_cdrstream_allocator * __restrict allocator, const struct dds_cdrstream_desc * __restrict type);

/** @component cdr_serializer */
DDS_EXPORT size_t dds_stream_print_key (dds_istream_t * __restrict is, const struct dds_cdrstream_desc * __restrict type, char * __restrict buf, size_t size);

/** @component cdr_serializer */
DDS_EXPORT size_t dds_stream_print_sample (dds_istream_t * __restrict is, const struct dds_cdrstream_desc * __restrict type, char * __restrict buf, size_t size);

/** @component cdr_serializer */
uint16_t dds_stream_minimum_xcdr_version (const uint32_t * __restrict ops);

/** @component cdr_serializer */
uint32_t dds_stream_type_nesting_depth (const uint32_t * __restrict ops);

/** @component cdr_serializer */
bool dds_stream_extensibility (const uint32_t * __restrict ops, enum dds_cdr_type_extensibility *ext);

/** @component cdr_serializer */
DDS_EXPORT void dds_cdrstream_desc_fini (struct dds_cdrstream_desc *desc, const struct dds_cdrstream_allocator * __restrict allocator);


#if defined (__cplusplus)
}
#endif
#endif

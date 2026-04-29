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

#include "dds/dds.h"
#include "dds/ddsrt/bswap.h"
#include "dds/ddsrt/hopscotch.h"
#include "dds/ddsrt/static_assert.h"
#include "dds/ddsc/dds_data_type_properties.h"

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
enum dds_cdr_enc_version {
  DDSI_RTPS_CDR_ENC_VERSION_UNDEF,
  DDSI_RTPS_CDR_ENC_VERSION_1,
  DDSI_RTPS_CDR_ENC_VERSION_2
};

enum dds_cdr_enc_format {
 DDSI_RTPS_CDR_ENC_FORMAT_PLAIN,
 DDSI_RTPS_CDR_ENC_FORMAT_DELIMITED,
 DDSI_RTPS_CDR_ENC_FORMAT_PL
};

/* X-Types spec 7.6.3.1.2: Implementations of this specification shall set the
least significant two bits in the second byte of the options field to a value
that encodes the number of padding bytes needed after the end of the serialized
payload in order to reach the next 4-byte aligned offset. */
#define DDS_CDR_HDR_PADDING_MASK 0x3


#define DDS_XCDR1_PL_SHORT_MAX_PARAM_ID     0x3f00u        // Maximum parameter ID that can be used with short PL encoding
#define DDS_XCDR1_PL_SHORT_MAX_PARAM_LEN    UINT16_MAX     // Maximum parameter length that can be used with short PL encoding
#define DDS_XCDR1_PL_SHORT_PID_EXTENDED     0x3f01u        // Indicates the extended (long) PL encoding is used
#define DDS_XCDR1_PL_SHORT_PID_LIST_END     0x3f02u        // Indicates the end of the parameter list data structure
#define DDS_XCDR1_PL_SHORT_PID_EXT_LEN      0x8u           // Value of the param header length field in case of extended PL encoding
#define DDS_XCDR1_PL_SHORT_FLAG_IMPL_EXT    0x8000u        // Flag for implementation specific interpretation of the parameter (not implemented)
#define DDS_XCDR1_PL_SHORT_FLAG_MU          0x4000u        // Flag to indicate the parameter is must-understand in short PL header

// Mask for the member ID in the short PL header; we don't use implementation-defined parameter ids (except
// in discovery data, but that's handled elsewhere anyway) and including this bit in the mask means we
// automatically treat them as unrecognised ids
#define DDS_XCDR1_PL_SHORT_PID_MASK         (0x3fffu | DDS_XCDR1_PL_SHORT_FLAG_IMPL_EXT)

#define DDS_XCDR1_PL_LONG_FLAG_IMPL_EXT     0x80000000u    // Flag used for RTPS discovery data types
#define DDS_XCDR1_PL_LONG_FLAG_MU           0x40000000u    // Flag to indicate the parameter is must-understand in extended PL header
#define DDS_XCDR1_PL_LONG_UNSPECIFIED1      0x20000000u    // For future extension
#define DDS_XCDR1_PL_LONG_UNSPECIFIED2      0x10000000u    // For future extension

// Mask for the member ID in the long PL header
#define DDS_XCDR1_PL_LONG_MID_MASK          (0x0fffffffu | DDS_XCDR1_PL_LONG_FLAG_IMPL_EXT)


#define DDS_CDR_CALCULATED_FLAGS (DDS_TOPIC_FIXED_KEY | DDS_TOPIC_FIXED_KEY_XCDR2 | DDS_TOPIC_FIXED_KEY_XCDR2_KEYHASH | DDS_TOPIC_KEY_APPENDABLE | DDS_TOPIC_KEY_MUTABLE | DDS_TOPIC_KEY_SEQUENCE | DDS_TOPIC_KEY_ARRAY_NONPRIM)

struct dds_cdr_header {
  unsigned short identifier;
  unsigned short options;
};

/**
 * @brief XTypes extensibility kind used by a CDR-serialized type.
 * @component cdr_serializer
 */
enum dds_cdr_type_extensibility
{
  DDS_CDR_TYPE_EXT_FINAL = 0,      ///< Final extensibility; serialized layout is fixed.
  DDS_CDR_TYPE_EXT_APPENDABLE = 1, ///< Appendable extensibility; serialized data uses a delimiter header.
  DDS_CDR_TYPE_EXT_MUTABLE = 2     ///< Mutable extensibility; serialized data uses parameter/member headers.
};

/**
 * @brief Selects the serialized form to use for key data.
 * @component cdr_serializer
 */
enum dds_cdr_key_serialization_kind
{
  DDS_CDR_KEY_SERIALIZATION_SAMPLE, ///< Serialize keys as key-only sample data, using definition order where applicable.
  DDS_CDR_KEY_SERIALIZATION_KEYHASH ///< Serialize keys in the canonical form used as input for key-hash calculation.
};

/**
 * @brief Result of validating and normalizing serialized CDR data.
 * @component cdr_serializer
 */
enum dds_stream_normalize_result {
  DDS_STREAM_NORMALIZE_ERROR,   ///< Validation failed or normalization could not be completed.
  DDS_STREAM_NORMALIZE_SUCCESS, ///< Validation and normalization succeeded.
  DDS_STREAM_NORMALIZE_DISCARD  ///< Sample is well-formed but must be discarded by try-construct handling.
};

typedef struct dds_istream {
  const unsigned char *m_buffer;
  uint32_t m_size;          /* Buffer size */
  uint32_t m_index;         /* Read/write offset from start of buffer */
  enum dds_cdr_enc_version m_xcdr_version;  /* XCDR version of the data */
} dds_istream_t;

typedef struct dds_ostream {
  unsigned char *m_buffer;
  uint32_t m_size;          /* Buffer size */
  uint32_t m_index;         /* Read/write offset from start of buffer */
  enum dds_cdr_enc_version m_xcdr_version;  /* XCDR version to use for serializing data */
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
  uint32_t idx;        /* Key index in containing type (definition order) */
} dds_cdrstream_desc_key_t;

typedef struct dds_cdrstream_desc_keys {
  uint32_t nkeys;
  struct dds_cdrstream_desc_key *keys; // keys in member-id order
  struct dds_cdrstream_desc_key *keys_definition_order;
} dds_cdrstream_desc_keys_t;

typedef struct dds_cdrstream_desc_op_seq {
  uint32_t nops;    /* Number of words in ops (which >= number of ops stored in preproc output) */
  uint32_t *ops;    /* Marshalling meta data */
} dds_cdrstream_desc_op_seq_t;

struct dds_cdrstream_desc_mid_table {
  struct ddsrt_hh *table;
  const uint32_t * op0;
};

struct dds_cdrstream_desc_mid {
  uint32_t adr_offs;
  uint32_t mid;
};

struct dds_cdrstream_desc {
  uint32_t size;    /* Size of type */
  uint32_t align;   /* Alignment of top-level type */
  uint32_t flagset; /* Flags */
  struct dds_cdrstream_desc_keys keys;
  dds_cdrstream_desc_op_seq_t ops;
  size_t opt_size_xcdr1;
  size_t opt_size_xcdr2;
  struct dds_cdrstream_desc_mid_table member_ids;
};


DDSRT_STATIC_ASSERT (offsetof (dds_ostreamLE_t, x) == 0);
DDSRT_STATIC_ASSERT (offsetof (dds_ostreamBE_t, x) == 0);

/**
 * @brief Pad an output stream to a 4-byte CDR boundary and ensure capacity.
 * @component cdr_serializer
 *
 * Zeroes any padding bytes written while advancing @p os to the next 4-byte
 * boundary.
 *
 * @param os            output stream to update
 * @param allocator     allocator used to grow the stream buffer if needed
 * @returns             number of padding bytes written
 */
uint32_t dds_cdr_alignto4_clear_and_resize (dds_ostream_t *os, const struct dds_cdrstream_allocator *allocator)
  ddsrt_nonnull_all;

/**
 * @brief Initialize an input CDR stream over an existing buffer.
 * @component cdr_serializer
 *
 * The input buffer remains owned by the caller and must stay valid while the
 * stream is used. The buffer must contain well-formed CDR data in native
 * endianness. Use @ref dds_stream_normalize to verify well-formedness and
 * transform serialized input to native endianness before reading it with an
 * input stream.
 *
 * @param is            input stream to initialize
 * @param size          size of @p input in bytes
 * @param input         serialized CDR payload buffer
 * @param xcdr_version  XCDR version of the serialized payload
 */
DDS_EXPORT void dds_istream_init (dds_istream_t *is, uint32_t size, const void *input, enum dds_cdr_enc_version xcdr_version)
  ddsrt_nonnull_all;

/**
 * @brief Finalize an input CDR stream.
 * @component cdr_serializer
 *
 * This currently does not release any resources because the input buffer is not
 * owned by the stream.
 *
 * @param is  input stream to finalize
 */
DDS_EXPORT void dds_istream_fini (dds_istream_t *is)
  ddsrt_nonnull_all;

/**
 * @brief Initialize a native-endian output CDR stream.
 * @component cdr_serializer
 *
 * Allocates an initial buffer of @p size bytes, or leaves the buffer NULL when
 * @p size is zero. The stream owns the allocated buffer until finalized and
 * serializes using native endianness.
 *
 * @param os            output stream to initialize
 * @param allocator     allocator used for stream storage
 * @param size          initial stream buffer size in bytes
 * @param xcdr_version  XCDR version to use when serializing
 */
DDS_EXPORT void dds_ostream_init (dds_ostream_t *os, const struct dds_cdrstream_allocator *allocator, uint32_t size, enum dds_cdr_enc_version xcdr_version)
  ddsrt_nonnull_all;

/**
 * @brief Finalize an output CDR stream.
 * @component cdr_serializer
 *
 * Releases the stream buffer with @p allocator. The stream object itself remains
 * owned by the caller.
 *
 * @param os         output stream to finalize
 * @param allocator  allocator used for stream storage
 */
DDS_EXPORT void dds_ostream_fini (dds_ostream_t *os, const struct dds_cdrstream_allocator *allocator)
  ddsrt_nonnull_all;

/**
 * @brief Initialize a little-endian output CDR stream.
 * @component cdr_serializer
 *
 * @param os            little-endian output stream to initialize
 * @param allocator     allocator used for stream storage
 * @param size          initial stream buffer size in bytes
 * @param xcdr_version  XCDR version to use when serializing
 */
DDS_EXPORT void dds_ostreamLE_init (dds_ostreamLE_t *os, const struct dds_cdrstream_allocator *allocator, uint32_t size, enum dds_cdr_enc_version xcdr_version)
  ddsrt_nonnull_all;

/**
 * @brief Finalize a little-endian output CDR stream.
 * @component cdr_serializer
 *
 * @param os         little-endian output stream to finalize
 * @param allocator  allocator used for stream storage
 */
DDS_EXPORT void dds_ostreamLE_fini (dds_ostreamLE_t *os, const struct dds_cdrstream_allocator *allocator)
  ddsrt_nonnull_all;

/**
 * @brief Initialize a big-endian output CDR stream.
 * @component cdr_serializer
 *
 * @param os            big-endian output stream to initialize
 * @param allocator     allocator used for stream storage
 * @param size          initial stream buffer size in bytes
 * @param xcdr_version  XCDR version to use when serializing
 */
DDS_EXPORT void dds_ostreamBE_init (dds_ostreamBE_t *os, const struct dds_cdrstream_allocator *allocator, uint32_t size, enum dds_cdr_enc_version xcdr_version)
  ddsrt_nonnull_all;

/**
 * @brief Finalize a big-endian output CDR stream.
 * @component cdr_serializer
 *
 * @param os         big-endian output stream to finalize
 * @param allocator  allocator used for stream storage
 */
DDS_EXPORT void dds_ostreamBE_fini (dds_ostreamBE_t *os, const struct dds_cdrstream_allocator *allocator)
  ddsrt_nonnull_all;

/**
 * @brief Create a native-endian output stream over a caller-owned buffer.
 * @component cdr_serializer
 *
 * The returned stream does not take ownership of @p buffer. It can be used for
 * bounded serialization into existing storage and serializes using native
 * endianness.
 *
 * @param buffer        caller-owned output buffer
 * @param size          size of @p buffer in bytes
 * @param xcdr_version  XCDR version to use when serializing
 * @returns             initialized output stream value
 */
dds_ostream_t dds_ostream_from_buffer (void *buffer, size_t size, enum dds_cdr_enc_version xcdr_version)
  ddsrt_nonnull_all;

/**
 * @brief Normalize and validate CDR data.
 * @component cdr_serializer
 *
 * @param data          data sample
 * @param size          size of the data
 * @param bswap         byte-swapping required
 * @param xcdr_version  XCDR version of the CDR data
 * @param desc          type descriptor
 * @param just_key      indicates if the data is a serialized key or a complete sample
 * @param actual_size   is set to the actual size of the data (*actual_size <= size) on successful return, undefined on failure
 * @returns             DDS_STREAM_NORMALIZE_SUCCESS when validation and normalization succeeded;
 *                      DDS_STREAM_NORMALIZE_DISCARD when the sample is well-formed but must be
 *                      discarded because try-construct handling rejects it; DDS_STREAM_NORMALIZE_ERROR
 *                      when validation failed or normalization could not be completed.
 */
DDS_EXPORT enum dds_stream_normalize_result dds_stream_normalize (void *data, uint32_t size, bool bswap, enum dds_cdr_enc_version xcdr_version, const struct dds_cdrstream_desc *desc, bool just_key, uint32_t *actual_size)
  ddsrt_attribute_warn_unused_result ddsrt_nonnull_all;

/**
 * @brief Normalize and validate an XCDR2 data fragment.
 * @component cdr_serializer
 *
 * Normalization starts at @p off and advances it past the normalized data on
 * success. Byte swapping is applied in place when @p bswap is true.
 *
 * @param data   serialized XCDR2 payload buffer
 * @param off    offset at which to start normalizing, updated on success
 * @param size   size of @p data in bytes
 * @param bswap  byte-swapping required
 * @param ops    marshalling metadata for the data type
 * @returns      DDS_STREAM_NORMALIZE_SUCCESS when validation and normalization succeeded;
 *               DDS_STREAM_NORMALIZE_DISCARD when the sample is well-formed but must be
 *               discarded because try-construct handling rejects it; DDS_STREAM_NORMALIZE_ERROR
 *               when validation failed or normalization could not be completed.
 */
DDS_EXPORT enum dds_stream_normalize_result dds_stream_normalize_xcdr2_data (char *data, uint32_t *off, uint32_t size, bool bswap, const uint32_t *ops)
  ddsrt_attribute_warn_unused_result ddsrt_nonnull_all;

/**
 * @brief Serialize data using native byte order.
 * @component cdr_serializer
 *
 * @param os         output stream to append to
 * @param allocator  allocator used to grow the stream
 * @param data       sample or key data to serialize
 * @param ops        marshalling metadata for @p data
 * @returns          pointer to the next operation on success, NULL on failure
 */
DDS_EXPORT const uint32_t *dds_stream_write (dds_ostream_t *os, const struct dds_cdrstream_allocator *allocator, const char *data, const uint32_t *ops)
  ddsrt_attribute_warn_unused_result ddsrt_nonnull_all;

/**
 * @brief Serialize data using little-endian byte order.
 * @component cdr_serializer
 *
 * @param os         little-endian output stream to append to
 * @param allocator  allocator used to grow the stream
 * @param data       sample or key data to serialize
 * @param ops        marshalling metadata for @p data
 * @returns          pointer to the next operation on success, NULL on failure
 */
DDS_EXPORT const uint32_t *dds_stream_writeLE (dds_ostreamLE_t *os, const struct dds_cdrstream_allocator *allocator, const char *data, const uint32_t *ops)
  ddsrt_attribute_warn_unused_result ddsrt_nonnull_all;

/**
 * @brief Serialize data using big-endian byte order.
 * @component cdr_serializer
 *
 * @param os         big-endian output stream to append to
 * @param allocator  allocator used to grow the stream
 * @param data       sample or key data to serialize
 * @param ops        marshalling metadata for @p data
 * @returns          pointer to the next operation on success, NULL on failure
 */
DDS_EXPORT const uint32_t *dds_stream_writeBE (dds_ostreamBE_t *os, const struct dds_cdrstream_allocator *allocator, const char *data, const uint32_t *ops)
  ddsrt_attribute_warn_unused_result ddsrt_nonnull_all;

/**
 * @brief Serialize data using native byte order and a member-id table.
 * @component cdr_serializer
 *
 * @param os         output stream to append to
 * @param allocator  allocator used to grow the stream
 * @param mid_table  member-id table for non-mutable member headers, or NULL
 * @param data       sample or key data to serialize
 * @param ops        marshalling metadata for @p data
 * @returns          pointer to the next operation on success, NULL on failure
 */
DDS_EXPORT const uint32_t *dds_stream_write_with_mid (dds_ostream_t *os, const struct dds_cdrstream_allocator *allocator, const struct dds_cdrstream_desc_mid_table *mid_table, const char *data, const uint32_t *ops)
  ddsrt_attribute_warn_unused_result ddsrt_nonnull ((1, 2, 4, 5));

/**
 * @brief Serialize data using little-endian byte order and a member-id table.
 * @component cdr_serializer
 *
 * @param os         little-endian output stream to append to
 * @param allocator  allocator used to grow the stream
 * @param mid_table  member-id table for non-mutable member headers, or NULL
 * @param data       sample or key data to serialize
 * @param ops        marshalling metadata for @p data
 * @returns          pointer to the next operation on success, NULL on failure
 */
DDS_EXPORT const uint32_t *dds_stream_write_with_midLE (dds_ostreamLE_t *os, const struct dds_cdrstream_allocator *allocator, const struct dds_cdrstream_desc_mid_table *mid_table, const char *data, const uint32_t *ops)
  ddsrt_attribute_warn_unused_result ddsrt_nonnull ((1, 2, 4, 5));

/**
 * @brief Serialize data using big-endian byte order and a member-id table.
 * @component cdr_serializer
 *
 * @param os         big-endian output stream to append to
 * @param allocator  allocator used to grow the stream
 * @param mid_table  member-id table for non-mutable member headers, or NULL
 * @param data       sample or key data to serialize
 * @param ops        marshalling metadata for @p data
 * @returns          pointer to the next operation on success, NULL on failure
 */
DDS_EXPORT const uint32_t *dds_stream_write_with_midBE (dds_ostreamBE_t *os, const struct dds_cdrstream_allocator *allocator, const struct dds_cdrstream_desc_mid_table *mid_table, const char *data, const uint32_t *ops)
  ddsrt_attribute_warn_unused_result ddsrt_nonnull ((1, 2, 4, 5));


/**
 * @brief Serialize data using a selected byte order.
 * @component cdr_serializer
 *
 * Dispatches to the native, little-endian, or big-endian writer according to
 * @p bo.
 *
 * @param os         output stream to append to
 * @param allocator  allocator used to grow the stream
 * @param mid_table  member-id table for non-mutable member headers, or NULL
 * @param data       sample or key data to serialize
 * @param ops        marshalling metadata for @p data
 * @param bo         byte order selector
 * @returns          pointer to the next operation on success, NULL on failure
 */
DDS_EXPORT const uint32_t * dds_stream_write_with_byte_order (dds_ostream_t *os, const struct dds_cdrstream_allocator *allocator, const struct dds_cdrstream_desc_mid_table *mid_table, const char *data, const uint32_t *ops, enum ddsrt_byte_order_selector bo)
  ddsrt_attribute_warn_unused_result  ddsrt_nonnull ((1, 2, 4, 5));

/**
 * @brief Serialize a complete sample using native byte order.
 * @component cdr_serializer
 *
 * @param os         output stream to append to
 * @param allocator  allocator used to grow the stream
 * @param data       sample data described by @p desc
 * @param desc       CDR stream descriptor for the sample type
 * @returns          true on success, false on serialization failure
 */
DDS_EXPORT bool dds_stream_write_sample (dds_ostream_t *os, const struct dds_cdrstream_allocator *allocator, const void *data, const struct dds_cdrstream_desc *desc)
  ddsrt_attribute_warn_unused_result ddsrt_nonnull_all;

/**
 * @brief Serialize a complete sample using little-endian byte order.
 * @component cdr_serializer
 *
 * @param os         little-endian output stream to append to
 * @param allocator  allocator used to grow the stream
 * @param data       sample data described by @p desc
 * @param desc       CDR stream descriptor for the sample type
 * @returns          true on success, false on serialization failure
 */
DDS_EXPORT bool dds_stream_write_sampleLE (dds_ostreamLE_t *os, const struct dds_cdrstream_allocator *allocator, const void *data, const struct dds_cdrstream_desc *desc)
  ddsrt_attribute_warn_unused_result ddsrt_nonnull_all;

/**
 * @brief Serialize a complete sample using big-endian byte order.
 * @component cdr_serializer
 *
 * @param os         big-endian output stream to append to
 * @param allocator  allocator used to grow the stream
 * @param data       sample data described by @p desc
 * @param desc       CDR stream descriptor for the sample type
 * @returns          true on success, false on serialization failure
 */
DDS_EXPORT bool dds_stream_write_sampleBE (dds_ostreamBE_t *os, const struct dds_cdrstream_allocator *allocator, const void *data, const struct dds_cdrstream_desc *desc)
  ddsrt_attribute_warn_unused_result ddsrt_nonnull_all;

/**
 * @brief Deserialize a complete sample from an input stream.
 * @component cdr_serializer
 *
 * @param is         input stream containing serialized sample data
 * @param data       destination sample storage described by @p desc
 * @param allocator  allocator used for dynamically allocated sample members
 * @param desc       CDR stream descriptor for the sample type
 */
DDS_EXPORT void dds_stream_read_sample (dds_istream_t *is, void *data, const struct dds_cdrstream_allocator *allocator, const struct dds_cdrstream_desc *desc)
  ddsrt_nonnull_all;

/**
 * @brief Release dynamically allocated members in a deserialized sample.
 * @component cdr_serializer
 *
 * Frees strings, sequences, external members and nested allocated data described
 * by @p ops. The sample storage itself remains owned by the caller.
 *
 * @param data       sample storage to clean up
 * @param allocator  allocator used for allocated sample members
 * @param ops        marshalling metadata for @p data
 */
DDS_EXPORT void dds_stream_free_sample (void *data, const struct dds_cdrstream_allocator *allocator, const uint32_t *ops)
  ddsrt_nonnull_all;

/**
 * @brief Count the number of CDR stream operations for a type.
 * @component cdr_serializer
 *
 * Includes any additional operations reachable through key offset descriptors.
 *
 * @param ops    marshalling metadata to inspect
 * @param nkeys  number of key descriptors in @p keys
 * @param keys   key descriptors, or NULL when @p nkeys is zero
 * @returns      number of operation words required
 */
DDS_EXPORT uint32_t dds_stream_countops (const uint32_t *ops, uint32_t nkeys, const dds_key_descriptor_t *keys)
  ddsrt_nonnull ((1));

/**
 * @brief Check whether a descriptor can use optimized direct-copy serialization.
 * @component cdr_serializer
 *
 * @param desc          CDR stream descriptor to inspect
 * @param xcdr_version  XCDR version for the optimization check
 * @returns             optimized serialized size, or 0 when not optimizable
 */
size_t dds_stream_check_optimize (const struct dds_cdrstream_desc *desc, enum dds_cdr_enc_version xcdr_version)
  ddsrt_nonnull_all;

/**
 * @brief Serialize the key fields from a sample using native byte order.
 * @component cdr_serializer
 *
 * @param os         output stream to append to
 * @param ser_kind   key serialization kind to produce
 * @param allocator  allocator used to grow the stream
 * @param sample     sample containing the key fields
 * @param desc       CDR stream descriptor for the sample type
 * @returns          true on success, false on serialization failure
 */
bool dds_stream_write_key (dds_ostream_t *os, enum dds_cdr_key_serialization_kind ser_kind, const struct dds_cdrstream_allocator *allocator, const char *sample, const struct dds_cdrstream_desc *desc)
  ddsrt_attribute_warn_unused_result ddsrt_nonnull_all;

/**
 * @brief Serialize the key fields from a sample using big-endian byte order.
 * @component cdr_serializer
 *
 * @param os         big-endian output stream to append to
 * @param ser_kind   key serialization kind to produce
 * @param allocator  allocator used to grow the stream
 * @param sample     sample containing the key fields
 * @param desc       CDR stream descriptor for the sample type
 * @returns          true on success, false on serialization failure
 */
bool dds_stream_write_keyBE (dds_ostreamBE_t *os, enum dds_cdr_key_serialization_kind ser_kind, const struct dds_cdrstream_allocator *allocator, const char *sample, const struct dds_cdrstream_desc *desc)
  ddsrt_attribute_warn_unused_result ddsrt_nonnull_all;

/**
 * @brief Extract serialized key fields from serialized sample data.
 * @component cdr_serializer
 *
 * Reads sample data from @p is and writes the serialized key representation to
 * @p os using native byte order. The generated key uses
 * @ref DDS_CDR_KEY_SERIALIZATION_SAMPLE.
 *
 * @param is         input stream containing serialized sample data
 * @param os         output stream for the serialized key
 * @param allocator  allocator used for temporary storage and stream growth
 * @param desc       CDR stream descriptor for the sample type
 * @returns          true on success, false on extraction failure
 */
DDS_EXPORT bool dds_stream_extract_key_from_data (dds_istream_t *is, dds_ostream_t *os, const struct dds_cdrstream_allocator *allocator, const struct dds_cdrstream_desc *desc)
  ddsrt_attribute_warn_unused_result ddsrt_nonnull_all;

/**
 * @brief Convert serialized key data to another key serialization form.
 * @component cdr_serializer
 *
 * Reads a serialized key from @p is and writes the requested key serialization
 * kind to @p os using native byte order.
 *
 * @param is         input stream containing serialized key data
 * @param os         output stream for the converted key data
 * @param ser_kind   key serialization kind to produce
 * @param allocator  allocator used for temporary storage and stream growth
 * @param desc       CDR stream descriptor for the sample type
 */
DDS_EXPORT void dds_stream_extract_key_from_key (dds_istream_t *is, dds_ostream_t *os, enum dds_cdr_key_serialization_kind ser_kind, const struct dds_cdrstream_allocator *allocator, const struct dds_cdrstream_desc *desc)
  ddsrt_nonnull_all;

/**
 * @brief Extract serialized key fields from sample data as big-endian CDR.
 * @component cdr_serializer
 *
 * The generated key uses @ref DDS_CDR_KEY_SERIALIZATION_SAMPLE.
 *
 * @param is         input stream containing serialized sample data
 * @param os         big-endian output stream for the serialized key
 * @param allocator  allocator used for temporary storage and stream growth
 * @param desc       CDR stream descriptor for the sample type
 * @returns          true on success, false on extraction failure
 */
DDS_EXPORT bool dds_stream_extract_keyBE_from_data (dds_istream_t *is, dds_ostreamBE_t *os, const struct dds_cdrstream_allocator *allocator, const struct dds_cdrstream_desc *desc)
  ddsrt_attribute_warn_unused_result ddsrt_nonnull_all;

/**
 * @brief Convert serialized key data to another big-endian key form.
 * @component cdr_serializer
 *
 * @param is         input stream containing serialized key data
 * @param os         big-endian output stream for the converted key data
 * @param ser_kind   key serialization kind to produce
 * @param allocator  allocator used for temporary storage and stream growth
 * @param desc       CDR stream descriptor for the sample type
 */
DDS_EXPORT void dds_stream_extract_keyBE_from_key (dds_istream_t *is, dds_ostreamBE_t *os, enum dds_cdr_key_serialization_kind ser_kind, const struct dds_cdrstream_allocator *allocator, const struct dds_cdrstream_desc *desc)
  ddsrt_nonnull_all;

/**
 * @brief Deserialize data according to stream operations.
 * @component cdr_serializer
 *
 * @param is         input stream containing serialized data
 * @param data       destination storage for deserialized data
 * @param allocator  allocator used for dynamically allocated members
 * @param ops        marshalling metadata for the data type
 * @returns          pointer to the next operation after the data that was read
 */
DDS_EXPORT const uint32_t *dds_stream_read (dds_istream_t *is, char *data, const struct dds_cdrstream_allocator *allocator, const uint32_t *ops)
  ddsrt_nonnull_all;

/**
 * @brief Deserialize serialized key data into sample storage.
 * @component cdr_serializer
 *
 * Only key fields are populated. The input stream is expected to contain
 * @ref DDS_CDR_KEY_SERIALIZATION_SAMPLE data.
 *
 * @param is         input stream containing serialized key data
 * @param sample     destination sample storage
 * @param allocator  allocator used for dynamically allocated key members
 * @param desc       CDR stream descriptor for the sample type
 */
DDS_EXPORT void dds_stream_read_key (dds_istream_t *is, char *sample, const struct dds_cdrstream_allocator *allocator, const struct dds_cdrstream_desc *desc)
  ddsrt_nonnull_all;

/**
 * @brief Pretty-print serialized key data.
 * @component cdr_serializer
 *
 * Writes a textual representation into @p buf when space permits. The input
 * stream is expected to contain @ref DDS_CDR_KEY_SERIALIZATION_SAMPLE data.
 *
 * @param is    input stream containing serialized key data
 * @param desc  CDR stream descriptor for the sample type
 * @param buf   destination text buffer
 * @param size  size of @p buf in bytes
 * @returns     remaining capacity in @p buf after printing
 */
DDS_EXPORT size_t dds_stream_print_key (dds_istream_t *is, const struct dds_cdrstream_desc *desc, char *buf, size_t size)
  ddsrt_nonnull_all;

/**
 * @brief Pretty-print serialized sample data.
 * @component cdr_serializer
 *
 * Writes a textual representation into @p buf when space permits.
 *
 * @param is    input stream containing serialized sample data
 * @param desc  CDR stream descriptor for the sample type
 * @param buf   destination text buffer
 * @param size  size of @p buf in bytes
 * @returns     remaining capacity in @p buf after printing
 */
DDS_EXPORT size_t dds_stream_print_sample (dds_istream_t *is, const struct dds_cdrstream_desc *desc, char *buf, size_t size)
  ddsrt_nonnull_all;

/**
 * @brief Compute the serialized size of a complete sample.
 * @component cdr_serializer
 *
 * @param data          sample data described by @p desc
 * @param desc          CDR stream descriptor for the sample type
 * @param xcdr_version  XCDR version to size for
 * @returns             serialized size in bytes, or SIZE_MAX on error
 */
DDS_EXPORT size_t dds_stream_getsize_sample (const char *data, const struct dds_cdrstream_desc *desc, enum dds_cdr_enc_version xcdr_version)
  ddsrt_nonnull_all ddsrt_attribute_warn_unused_result;

/**
 * @brief Compute the serialized size of a sample key.
 * @component cdr_serializer
 *
 * @param sample        sample containing key fields
 * @param desc          CDR stream descriptor for the sample type
 * @param xcdr_version  XCDR version to size for
 * @returns             serialized key size in bytes, or SIZE_MAX on error
 */
DDS_EXPORT size_t dds_stream_getsize_key (const char *sample, const struct dds_cdrstream_desc *desc, enum dds_cdr_enc_version xcdr_version)
  ddsrt_nonnull_all ddsrt_attribute_warn_unused_result;

/**
 * @brief Determine the minimum XCDR version required by stream operations.
 * @component cdr_serializer
 *
 * @param ops  marshalling metadata to inspect
 * @returns    minimum required XCDR version
 */
enum dds_cdr_enc_version dds_stream_minimum_xcdr_version (const uint32_t *ops)
  ddsrt_nonnull_all;

/**
 * @brief Determine the maximum type nesting depth in stream operations.
 * @component cdr_serializer
 *
 * @param ops  marshalling metadata to inspect
 * @returns    maximum nested aggregate depth
 */
uint32_t dds_stream_type_nesting_depth (const uint32_t *ops)
  ddsrt_nonnull_all;

/**
 * @brief Compute key-related topic flags and optional key sizes.
 * @component cdr_serializer
 *
 * The returned flags are limited to @ref DDS_CDR_CALCULATED_FLAGS.
 *
 * @param desc           CDR stream descriptor to inspect
 * @param keysz_xcdrv1   optional output for XCDR1 key size
 * @param keysz_xcdrv2   optional output for XCDR2 key size
 * @returns              calculated key flags
 */
uint32_t dds_stream_key_flags (struct dds_cdrstream_desc *desc, uint32_t *keysz_xcdrv1, uint32_t *keysz_xcdrv2)
  ddsrt_nonnull ((1));

/**
 * @brief Determine the top-level type extensibility from stream operations.
 * @component cdr_serializer
 *
 * @param ops  marshalling metadata to inspect
 * @param ext  output for the detected top-level extensibility
 * @returns    true when extensibility was detected, false otherwise
 */
bool dds_stream_extensibility (const uint32_t *ops, enum dds_cdr_type_extensibility *ext)
  ddsrt_nonnull_all;

/**
 * @brief Determine data type properties used by stream operations.
 * @component cdr_serializer
 *
 * @param ops  marshalling metadata to inspect
 * @returns    bitset of data type properties present in @p ops
 */
dds_data_type_properties_t dds_stream_data_types (const uint32_t *ops)
  ddsrt_nonnull_all;

/**
 * @brief Initialize a CDR stream descriptor with an explicit operation count.
 * @component cdr_serializer
 *
 * Copies the operation stream and key descriptors into storage owned by @p desc.
 * The descriptor must be finalized with @ref dds_cdrstream_desc_fini.
 * @p nops is normally the number of operation words in @p ops. For historical
 * compatibility with older topic descriptors, it may also be the number of
 * operations instead, excluding additional words consumed by multi-word
 * operations.
 *
 * @param desc       descriptor to initialize
 * @param allocator  allocator used for descriptor storage
 * @param size       size of the sample type
 * @param align      alignment of the top-level sample type
 * @param flagset    topic descriptor flags
 * @param ops        marshalling metadata to copy
 * @param nops       number of operation words supplied
 * @param keys       key descriptors, or NULL when @p nkeys is zero
 * @param nkeys      number of key descriptors
 */
DDS_EXPORT void dds_cdrstream_desc_init_with_nops (struct dds_cdrstream_desc *desc, const struct dds_cdrstream_allocator *allocator,
    uint32_t size, uint32_t align, uint32_t flagset, const uint32_t *ops, uint32_t nops, const dds_key_descriptor_t *keys, uint32_t nkeys)
  ddsrt_nonnull ((1, 2, 6));

/**
 * @brief Initialize a legacy CDR stream descriptor.
 * @component cdr_serializer
 *
 * Copies the operation stream and key descriptors into storage owned by @p desc.
 * The descriptor must be finalized with @ref dds_cdrstream_desc_fini.
 * This function is retained for compatibility and relies on the historical
 * interpretation of the operation count, where the operation stream length is
 * derived from the operations rather than supplied as a word count. That means
 * some descriptor features are unavailable; in particular, appendable types in
 * XCDR1 mode are not supported for descriptors constructed with this function.
 *
 * @param desc       descriptor to initialize
 * @param allocator  allocator used for descriptor storage
 * @param size       size of the sample type
 * @param align      alignment of the top-level sample type
 * @param flagset    topic descriptor flags
 * @param ops        marshalling metadata to copy
 * @param keys       key descriptors, or NULL when @p nkeys is zero
 * @param nkeys      number of key descriptors
 */
DDS_EXPORT void dds_cdrstream_desc_init (struct dds_cdrstream_desc *desc, const struct dds_cdrstream_allocator *allocator,
    uint32_t size, uint32_t align, uint32_t flagset, const uint32_t *ops, const dds_key_descriptor_t *keys, uint32_t nkeys)
  ddsrt_nonnull ((1, 2, 6));

/**
 * @brief Finalize a CDR stream descriptor.
 * @component cdr_serializer
 *
 * Releases storage owned by @p desc. The descriptor object itself remains owned
 * by the caller.
 *
 * @param desc       descriptor to finalize
 * @param allocator  allocator used for descriptor storage
 */
DDS_EXPORT void dds_cdrstream_desc_fini (struct dds_cdrstream_desc *desc, const struct dds_cdrstream_allocator *allocator)
  ddsrt_nonnull_all;

/**
 * @brief Initialize a CDR stream descriptor from a topic descriptor.
 * @component cdr_serializer
 *
 * Uses the default CDR stream allocator and copies the serialization metadata
 * from @p topic_desc. The descriptor must be finalized with
 * @ref dds_cdrstream_desc_fini.
 *
 * @param desc        descriptor to initialize
 * @param topic_desc  topic descriptor containing serialization metadata
 */
DDS_EXPORT void dds_cdrstream_desc_from_topic_desc (struct dds_cdrstream_desc *desc, const dds_topic_descriptor_t *topic_desc)
  ddsrt_nonnull_all;


#if defined (__cplusplus)
}
#endif
#endif

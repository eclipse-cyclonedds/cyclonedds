// Copyright(c) 2021 to 2023 ZettaScale Technology
// Copyright(c) 2021 Apex.AI, Inc
//
// This program and the accompanying materials are made available under the
// terms of the Eclipse Public License v. 2.0 which is available at
// http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
// v. 1.0 which is available at
// http://www.eclipse.org/org/documents/edl-v10.php.
//
// SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

/**
 * @defgroup loaned_sample (Loaned samples)
 * @ingroup dds
 */

#ifndef DDS_LOANED_SAMPLE_H
#define DDS_LOANED_SAMPLE_H

#include "dds/export.h"
#include "dds/ddsrt/retcode.h"
#include "dds/ddsrt/atomics.h"
#include "dds/ddsrt/time.h"
#include "dds/ddsc/dds_basic_types.h"

#if defined(__cplusplus)
extern "C" {
#endif

struct dds_loan_pool;
struct dds_loaned_sample;
struct dds_psmx_endpoint;

/**
 * @brief State of the data contained in a memory block
 */
typedef enum dds_loaned_sample_state {
  DDS_LOANED_SAMPLE_STATE_UNITIALIZED,
  DDS_LOANED_SAMPLE_STATE_RAW_KEY,
  DDS_LOANED_SAMPLE_STATE_RAW_DATA,
  DDS_LOANED_SAMPLE_STATE_SERIALIZED_KEY,
  DDS_LOANED_SAMPLE_STATE_SERIALIZED_DATA
} dds_loaned_sample_state_t;

/**
 * @brief Identifier used to distinguish between raw data types (C/C++/Python/...)
 */
typedef uint32_t dds_loan_data_type_t;

/**
 * @brief Definition for function to cleanup loaned sample
 *
 * @param[in] loaned_sample  A loaned sample
 */
typedef void (*dds_loaned_sample_free_f) (struct dds_loaned_sample *loaned_sample);

/**
 * @brief Definition for function to increment refcount on a loaned sample
 *
 * @param[in] loaned_sample  A loaned sample
 */
typedef dds_return_t (*dds_loaned_sample_ref_f) (struct dds_loaned_sample *loaned_sample);

/**
 * @brief Definition for function to decrement refcount on a loaned sample
 *
 * @param[in] loaned_sample  A loaned sample
 */
typedef dds_return_t (*dds_loaned_sample_unref_f) (struct dds_loaned_sample *loaned_sample);

/**
 * @brief Definition for function to reset contents of a loaned sample
 *
 * @param[in] loaned_sample  A loaned sample
 */
typedef void (*dds_loaned_sample_reset_f) (struct dds_loaned_sample *loaned_sample);

/**
 * @brief Container for implementation specific operations
 */
typedef struct dds_loaned_sample_ops {
  dds_loaned_sample_free_f    free;
  dds_loaned_sample_reset_f   reset;
} dds_loaned_sample_ops_t;

typedef enum dds_loaned_sample_origin_kind {
  DDS_LOAN_ORIGIN_KIND_HEAP,
  DDS_LOAN_ORIGIN_KIND_PSMX
} dds_loaned_sample_origin_kind_t;

typedef struct dds_loaned_sample_origin {
  enum dds_loaned_sample_origin_kind origin_kind;
  struct dds_psmx_endpoint *psmx_endpoint;
} dds_loaned_sample_origin_t;

/**
 * @brief The definition of a block of memory originating from a PSMX
 */
typedef struct dds_loaned_sample {
  dds_loaned_sample_ops_t ops; //!< the implementation specific ops for this sample
  struct dds_loaned_sample_origin loan_origin; //!< the origin of the loan
  struct dds_loan_pool *loan_pool; //!< the associated loan pool
  struct dds_psmx_metadata * metadata; //!< pointer to the associated metadata
  void * sample_ptr; //!< pointer to the loaned sample
  uint32_t loan_idx; //!< the storage index of the loan
  ddsrt_atomic_uint32_t refc; //!< the number of references to this loan
} dds_loaned_sample_t;

/**
 * @brief Generic function to increase the refcount for a sample
 *
 * This function calls the implementation specific function
 *
 * @param[in] loaned_sample  A loaned sample
 * @return a DDS return code
 */
DDS_EXPORT dds_return_t dds_loaned_sample_ref (dds_loaned_sample_t *loaned_sample);

/**
 * @brief Generic function to decrease the refcount for a sample
 *
 * This function calls the implementation specific function
 *
 * @param[in] loaned_sample  A loaned sample
 * @return a DDS return code
 */
DDS_EXPORT dds_return_t dds_loaned_sample_unref (dds_loaned_sample_t *loaned_sample);

/**
 * @brief insert data from a loaned sample into the reader history cache
 * @ingroup reading
 *
 * @param[in] reader The reader entity.
 * @param[in] data Pointer to the loaned sample of the entity received
 *
 * @returns A dds_return_t indicating success or failure.
 *
 * @retval DDS_RETCODE_OK
 *             The operation was successful.
 * @retval DDS_RETCODE_BAD_PARAMETER
 *             One or more parameters are invalid.
 * @retval DDS_RETCODE_ILLEGAL_OPERATION
 *             The operation is invoked on an inappropriate object.
 * @retval DDS_RETCODE_ALREADY_DELETED
 *             The reader entity has already been deleted.
 */
DDS_EXPORT dds_return_t dds_reader_store_loaned_sample (dds_entity_t reader, dds_loaned_sample_t *data);

#if defined(__cplusplus)
}
#endif

#endif /* DDS_LOANED_SAMPLE_H */

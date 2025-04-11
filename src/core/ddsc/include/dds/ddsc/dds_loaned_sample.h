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
#include "dds/ddsrt/attributes.h"
#include "dds/ddsc/dds_basic_types.h"

#if defined(__cplusplus)
extern "C" {
#endif

struct dds_loan_pool;
struct dds_loaned_sample;
struct dds_psmx_endpoint;

/**
 * @brief State of the data contained in a Loaned Sample
 * @ingroup loaned_sample
 *
 */
typedef enum dds_loaned_sample_state {
  DDS_LOANED_SAMPLE_STATE_UNITIALIZED,    //!< state not set yet; not passed through PSMX interface
  DDS_LOANED_SAMPLE_STATE_RAW_KEY,        //!< application representation, only key fields initialized
  DDS_LOANED_SAMPLE_STATE_RAW_DATA,       //!< application representation, full sample initialized
  DDS_LOANED_SAMPLE_STATE_SERIALIZED_KEY, //!< CDR/XCDR1/XCDR2-serialized key value serialized
  DDS_LOANED_SAMPLE_STATE_SERIALIZED_DATA //!< CDR/XCDR1/XCDR2-serialized sample value serialized
} dds_loaned_sample_state_t;

/**
 * @brief Identifier used to distinguish between raw data types (C/C++/Python/...) in a Loaned Sample
 * @ingroup loaned_sample
 */
typedef uint32_t dds_loan_data_type_t;

/**
 * @brief Definition for function to free a Loaned Sample
 * @ingroup loaned_sample
 *
 * @param[in] loaned_sample  A loaned sample
 */
typedef void (*dds_loaned_sample_free_f) (struct dds_loaned_sample *loaned_sample)
  ddsrt_nonnull_all;

/**
 * @brief Container for implementation specific operations
 * @ingroup loaned_sample
 */
typedef struct dds_loaned_sample_ops {
  dds_loaned_sample_free_f free;
} dds_loaned_sample_ops_t;

typedef enum dds_loaned_sample_origin_kind {
  DDS_LOAN_ORIGIN_KIND_HEAP, //!< Loaned sample allocated on the heap (and owned by Cyclone DDS)
  DDS_LOAN_ORIGIN_KIND_PSMX  //!< Loaned sample owned by a PSMX Endpoint
} dds_loaned_sample_origin_kind_t;

typedef struct dds_loaned_sample_origin {
  enum dds_loaned_sample_origin_kind origin_kind; //!< Whether on the HEAP or provided by a PSMX Endpoint
  struct dds_psmx_endpoint *psmx_endpoint; //!< Owning PSMX Endpoint, null if KIND_HEAP
} dds_loaned_sample_origin_t;

/**
 * @brief The definition of a Loaned Sample
 * @ingroup loaned_sample
 */
typedef struct dds_loaned_sample {
  dds_loaned_sample_ops_t ops; //!< the implementation specific ops for this sample
  struct dds_loaned_sample_origin loan_origin; //!< the origin of the loan
  struct dds_psmx_metadata * metadata; //!< pointer to the associated metadata
  void * sample_ptr; //!< pointer to the loaned sample
  ddsrt_atomic_uint32_t refc; //!< the number of references to this loan
} dds_loaned_sample_t;

/**
 * @brief Generic function to increase the refcount for a loaned sample
 * @ingroup loaned_sample
 *
 * @param[in] loaned_sample  A loaned sample
 */
DDS_INLINE_EXPORT inline void ddsrt_nonnull_all dds_loaned_sample_ref (dds_loaned_sample_t *loaned_sample) {
  ddsrt_atomic_inc32 (&loaned_sample->refc);
}

/**
 * @brief Generic function to decrease the refcount for a loaned sample
 * @ingroup loaned_sample
 *
 * Calls the PSMX plugin specific free function once the reference count reaches 0.
 *
 * Loaned Samples are freed in the order in which their reference counts drop to 0.
 *
 * On the writing side, writing (or "returning") Loaned Samples in the order in which they
 * were requested will ensure that the "free" operations is also invoked in the that
 * order.
 *
 * On the reading side, the `serdata` constructed by `dds_reader_store_loaned_sample` may
 * or may not reference the Loaned Sample, and if not the Loaned Sample will be dropped
 * once the PSMX Plugin invokes `dds_loaned_sample_unref` on it.
 *
 * Otherwise, the Loaned Sample will be referenced by a sample in the reader history cache
 * and stay there until it gets dropped (and the PSMX Plugin also invokes
 * `dds_loaned_sample_unref` on it). If the reading application does not use loans in the
 * reads/takes, that will be once it is pushed out of the history or taken. If the
 * application uses `takecdr` or `take_with_collector`, then it is dependent on when the
 * application logic drops the serdata via `ddsi_serdata_unref`.
 *
 * If the application does use loans in reads/takes, and Cyclone DDS decides to hand the
 * data in the Loaned Sample to the application, the earliest time at which it can be
 * dropped is when it is removed from the reader history cache *and* the loan is returned
 * by the application using `dds_return_loan`.
 *
 * @param[in] loaned_sample  A loaned sample
 */
DDS_INLINE_EXPORT inline void ddsrt_nonnull_all dds_loaned_sample_unref (dds_loaned_sample_t *loaned_sample) {
  if (ddsrt_atomic_dec32_ov (&loaned_sample->refc) == 1)
    loaned_sample->ops.free (loaned_sample);
}

/**
 * @brief insert data from a loaned sample into the reader history cache
 * @ingroup reading
 *
 * Constructs a serdata of the reader's type from the loaned sample and stores it in the
 * reader's history cache. If the serdata references the loan, `dds_loaned_sample_ref` is
 * used to increment its reference count.
 *
 * If data is not from a known writer, this is a no-op.
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
DDS_EXPORT dds_return_t dds_reader_store_loaned_sample (dds_entity_t reader, dds_loaned_sample_t *data)
  ddsrt_nonnull_all;

/**
 * @brief insert data from a loaned sample into the reader history cache using the provided writer meta-data
 * @ingroup reading
 *
 * Constructs a serdata of the reader's type from the loaned sample and stores it in the
 * reader's history cache. If the serdata references the loan, `dds_loaned_sample_ref` is
 * used to increment its reference count.
 *
 * It is equivalent to `dds_reader_store_loaned_sample`, except that for this function no
 * writer look-up takes place (it doesn't need to because the required information is
 * passed in directly) and data unknown writers is accepted.

 * @param[in] reader The reader entity.
 * @param[in] data Pointer to the loaned sample of the entity received
 * @param[in] ownership_strength The ownership strength of the writer
 * @param[in] autodispose_unregistered_instances Writer setting for auto-disposing unregistered entities
 * @param[in] lifespan_duration Lifespan duration value configured for the writer
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
DDS_EXPORT dds_return_t dds_reader_store_loaned_sample_wr_metadata (dds_entity_t reader, dds_loaned_sample_t *data, int32_t ownership_strength, bool autodispose_unregistered_instances, dds_duration_t lifespan_duration)
  ddsrt_nonnull_all;

#if defined(__cplusplus)
}
#endif

#endif /* DDS_LOANED_SAMPLE_H */

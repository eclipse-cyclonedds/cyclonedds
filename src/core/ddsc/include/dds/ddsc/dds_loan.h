

// API extension
// defines functions needed for loaning and shared memory usage

#ifndef _DDS_LOAN_H_
#define _DDS_LOAN_H_

#include "dds/ddsc/dds_public_types.h"
#include "dds/ddsrt/retcode.h"
#include "dds/export.h"

#if defined(__cplusplus)
extern "C" {
#endif

/** @brief Check if a Loan is available to reader/writer
 * The loan is available if the shared memory is enabled and all the constraints
 * to enable shared memory are met and the type is fixed
 *
 * @param[in] entity the handle of the entity
 *
 * @returns loan available or not
 */
DDS_EXPORT bool dds_is_loan_available(const dds_entity_t entity);

/** @brief Check if a shared memory is available to reader/writer.
 *
 * @note shared memory available and loan available have different meaning
 * 1)We may not be able to use the loan API due to type constraints (not fixed)
 * 2) We may also allow using the loan API but without shared memory
 * but in this case not zero-copy.
 *
 * @param[in] entity the handle of the entity
 *
 * @returns true if shared memory is available, false otherwise
 */
DDS_EXPORT bool dds_is_shared_memory_available(const dds_entity_t entity);

DDS_DEPRECATED_EXPORT bool is_loan_available(const dds_entity_t entity);

DDS_EXPORT dds_return_t dds_loan_shared_memory_buffer(dds_entity_t writer,
                                                      size_t size,
                                                      void **buffer);

// MAKI TODO: doxygen if we agree on the structure

DDS_EXPORT bool dds_writer_loan_sample_supported(dds_entity_t writer);

DDS_EXPORT bool dds_writer_shared_memory_supported(dds_entity_t writer);

DDS_EXPORT bool dds_reader_loan_supported(dds_entity_t reader);

DDS_EXPORT bool dds_reader_shared_memory_supported(dds_entity_t reader);

// TODO: move other functions that are used for loaning here as well?
// The loan functions themselves exist at writer and reader for now.
// It is requires some more refactoring to move them (bookkeeping of loans at
// writer etc.)

#if defined(__cplusplus)
}
#endif
#endif
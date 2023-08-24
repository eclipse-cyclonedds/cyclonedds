// Copyright(c) 2021 ZettaScale Technology
// Copyright(c) 2021 Apex.AI, Inc
//
// This program and the accompanying materials are made available under the
// terms of the Eclipse Public License v. 2.0 which is available at
// http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
// v. 1.0 which is available at
// http://www.eclipse.org/org/documents/edl-v10.php.
//
// SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

// API extension
// defines functions needed for loaning and shared memory usage

#ifndef DDS_PUBLIC_LOAN_API_H
#define DDS_PUBLIC_LOAN_API_H

#include "dds/ddsc/dds_basic_types.h"
#include "dds/ddsrt/retcode.h"
#include "dds/export.h"

#if defined(__cplusplus)
extern "C" {
#endif

/**
 * @defgroup loan (Loans API)
 * @ingroup dds
 */

/**
 * @brief Request loans from an entity.
 * @ingroup loan
 *
 * Borrow one or more samples from the entity, which currently must be a writer. These samples
 * can then be returned using @ref `dds_return_loan` or they can be used to publish data
 * using @ref `dds_write` or @ref `dds_writedispose`.
 *
 * If the topic type has a fixed size (and so no internal pointers) and a PSMX interface is configured,
 * the memory will be borrowed from the PSMX implementation, which allows Cyclone to avoid copies
 * and/or serialization if there is no need for sending the data over a network interface or storing it
 * in the WHC.
 *
 * @param[in] entity The entity to request loans from.
 * @param[out] buf Pointer to the array to store the pointers to the loaned samples into.
 * @param[out] bufsz The number of loans to request (> 0)
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
 *             The entity has already been deleted.
 * @retval DDS_RETCODE_ERROR
 *             An unfortunate incident occurred.
 */
DDS_EXPORT dds_return_t dds_request_loan (dds_entity_t entity, void **buf, int32_t bufsz);

/**
 * @brief Return loaned samples to a reader or writer
 * @ingroup loan
 * @component read_data
 *
 * Used to release sample buffers returned by a read/take operation (a reader-loan)
 * or, in case shared memory is enabled, of the loan_sample operation (a writer-loan).
 *
 * When the application provides an empty buffer to a reader-loan, memory is allocated and
 * managed by DDS. By calling dds_return_loan(), the reader-loan is released so that the buffer
 * can be reused during a successive read/take operation. When a condition is provided, the
 * reader to which the condition belongs is looked up.
 *
 * Writer-loans are normally released implicitly when writing a loaned sample, but you can
 * cancel a writer-loan prematurely by invoking the return_loan() operation. For writer loans, buf is
 * overwritten with null pointers for all successfully returned entries. Any failure causes it to abort,
 * possibly midway through buf.
 *
 * @param[in] entity The entity that the loan belongs to.
 * @param[in,out] buf An array of (pointers to) samples, some or all of which will be set to null pointers.
 * @param[in] bufsz The number of (pointers to) samples stored in buf.
 *
 * @returns A dds_return_t indicating success or failure
 * @retval DDS_RETCODE_OK
 *             - the operation was successful; for a writer loan, all entries in buf are set to null
 *             - this specifically includes cases where bufsz <= 0 while entity is valid
 * @retval DDS_RETCODE_BAD_PARAMETER
 *             - the entity parameter is not a valid parameter
 *             - buf is null, or bufsz > 0 and buf[0] = null
 *             - (for writer loans) buf[0 <= i < bufsz] is null; operation is aborted, all buf[j < i] = null on return
 * @retval DDS_RETCODE_PRECONDITION_NOT_MET
 *             - (for reader loans) buf was already returned (not guaranteed to be detected)
 *             - (for writer loans) buf[0 <= i < bufsz] does not correspond to an outstanding loan, all buf[j < i] = null on return
 * @retval DDS_RETCODE_UNSUPPORTED
 *             - (for writer loans) invoked on a writer not supporting loans.
 * @retval DDS_RETCODE_ILLEGAL_OPERATION
 *             - the operation is invoked on an inappropriate object.
 */
DDS_EXPORT dds_return_t dds_return_loan (dds_entity_t entity, void **buf, int32_t bufsz);

/**
 * @ingroup loan
 * @component read_data
 * @brief Check if a shared memory is available to reader/writer.
 *
 * @note dds_loan_shared_memory_buffer can be used if and only if
 * dds_is_shared_memory_available returns true.
 *
 * @param[in] entity the handle of the entity
 *
 * @returns true if shared memory is available, false otherwise
 */
DDS_EXPORT bool dds_is_shared_memory_available (const dds_entity_t entity);

/**
 * @ingroup loan
 * @component read_data
 * @brief Loan a shared memory buffer of a specific size from the writer.
 *
 * @note Currently this function is to be used with dds_writecdr by adding the
 * loaned buffer to serdata as iox_chunk.
 * @note The function can only be used if dds_is_shared_memory_available is
 *       true for the writer.
 *
 * @param[in] writer the writer to loan the buffer from
 * @param[in] size the requested buffer size
 * @param[out] buffer the loaned buffer
 *
 * @returns DDS_RETCODE_OK if successful, DDS_RETCODE_ERROR otherwise
 */
DDS_EXPORT dds_return_t dds_loan_shared_memory_buffer (dds_entity_t writer, size_t size, void **buffer);

/**
 * @ingroup deprecated
 * @component read_data
 * @brief Check if a Loan is available to reader/writer
 * @deprecated Use @ref dds_request_loan instead, returns 0 if loan is not available
 *
 * The loan is available if the shared memory is enabled and all the constraints
 * to enable shared memory are met and the type is fixed
 * @note dds_loan_sample can be used if and only if
 * dds_is_loan_available returns true.
 *
 * @param[in] entity the handle of the entity
 *
 * @returns loan available or not
 */
DDS_DEPRECATED_EXPORT bool dds_is_loan_available (const dds_entity_t entity);

/**
 * @ingroup deprecated
 * @component read_data
 * @brief Loan a sample from the writer.
 * @deprecated Use @ref dds_request_loan
 * @note This function is to be used with dds_write to publish the loaned
 * sample.
 * @note The function can only be used if dds_is_loan_available is
 *       true for the writer.
 *
 * @param[in] writer the writer to loan the buffer from
 * @param[out] sample the loaned sample
 *
 * @returns DDS_RETCODE_OK if successful, DDS_RETCODE_ERROR otherwise
 */
DDS_DEPRECATED_EXPORT dds_return_t dds_loan_sample (dds_entity_t writer, void **sample);

#if defined(__cplusplus)
}
#endif

#endif /* DDS_PUBLIC_LOAN_API_H */

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
 * @brief Request a loan from an entity.
 * @ingroup loan
 *
 * Borrow a sample from the entity, which currently must be a writer. This sample
 * can then be returned using @ref dds_return_loan or can be used to publish data
 * using @ref dds_write or @ref dds_writedispose.
 *
 * If the topic type has a fixed size (and so no internal pointers) and a PSMX interface is configured,
 * the memory will be borrowed from the PSMX implementation, which allows Cyclone to avoid copies
 * and/or serialization if there is no need for sending the data over a network interface or storing it
 * in the WHC.
 *
 * @param[in] entity The entity to request loans from.
 * @param[out] sample Where to store the address of the loaned sample.
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
DDS_EXPORT dds_return_t dds_request_loan (dds_entity_t entity, void **sample);

/**
 * @brief Return loaned samples to a reader or writer
 * @ingroup loan
 * @component read_data
 *
 * Used to release middleware-owned samples returned by a read/take operation and samples
 * borrowed from the writer using @ref dds_request_loan.
 *
 * For reader loans, the @ref dds_read and @ref dds_take operations implicitly return
 * outstanding loans referenced by the sample array passed in.  Looping until no data is
 * returned therefore often eliminates the need for calling this function.
 *
 * For writer loans, a @ref dds_write operation takes over the loan.  Consequently, this
 * function is only needed in the exceptional case where a loan is taken but ultimately
 * not used to publish data.
 *
 * @param[in] entity The entity that the loan(s) belong to. If a read or query condition is passed in for the entity, the reader for that condition is used.
 * @param[in,out] buf An array of (pointers to) samples, some or all of which will be set to null pointers.
 * @param[in] bufsz The size of the buffer.
 *
 * @returns A dds_return_t indicating success or failure
 * @retval DDS_RETCODE_OK
 *             - the operation was successful
 *             - a no-op if bufsz <= 0, otherwise
 *             - buf[0] .. buf[k-1] were successfully returned loans, k = bufsz or buf[k] = null
 *             - buf[0] is set to a null pointer, buf[k > 0] undefined
 * @retval DDS_RETCODE_BAD_PARAMETER
 *             - the entity parameter is not a valid parameter
 *             - buf is null, or bufsz > 0 and buf[0] = null
 *             - a non-loan was encountered (all loans are still returned)
 * @retval DDS_RETCODE_PRECONDITION_NOT_MET
 *             - bufsz > 0 and buf[0] != null but not a loan, nothing done
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
 * @note dds_request_loan_of_size can be used if and only if
 * dds_is_shared_memory_available returns true.
 *
 * @param[in] entity the handle of the entity
 *
 * @returns true if shared memory is available, false otherwise
 */
DDS_EXPORT bool dds_is_shared_memory_available (const dds_entity_t entity);

/**
 * @brief Request a loan of a specified size from an entity.
 * @ingroup loan
 *
 * Borrow a sample of a specified size from the entity, which currently must be a
 * writer. This sample can then be returned using @ref dds_return_loan or can be
 * used to publish data using @ref dds_write or @ref dds_writedispose.
 *
 * @note The function can only be used if dds_is_shared_memory_available is
 *       true for the writer.
 *
 * @param[in] writer The entity to request loans from.
 * @param[in] size the requested loan size
 * @param[out] sample Where to store the address of the loaned sample.
 *
 * @returns DDS_RETCODE_OK if successful, DDS_RETCODE_ERROR otherwise
 */
DDS_EXPORT dds_return_t dds_request_loan_of_size (dds_entity_t writer, size_t size, void **sample);

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

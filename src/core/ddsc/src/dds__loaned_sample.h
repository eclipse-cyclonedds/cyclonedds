// Copyright(c) 2006 to 2023 ZettaScale Technology and others
//
// This program and the accompanying materials are made available under the
// terms of the Eclipse Public License v. 2.0 which is available at
// http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
// v. 1.0 which is available at
// http://www.eclipse.org/org/documents/edl-v10.php.
//
// SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

#ifndef DDS__LOANED_SAMPLE_H
#define DDS__LOANED_SAMPLE_H

#include "dds/ddsc/dds_loaned_sample.h"
#include "dds__types.h"

#if defined(__cplusplus)
extern "C" {
#endif

/**
 * @brief Loan pool
 */
struct dds_loan_pool {
  dds_loaned_sample_t **samples;
  uint32_t n_samples_cap;
  uint32_t n_samples;
};

typedef struct dds_loan_pool dds_loan_pool_t;

/**
 * @brief Create a loan pool
 *
 * @param[out] ppool Gets a pointer to the newly created loan pool
 * @param[in] initial_cap Initial capacity
 * @return a DDS return code
 */
dds_return_t dds_loan_pool_create (dds_loan_pool_t **ppool, uint32_t initial_cap)
  ddsrt_nonnull_all;

/**
 * @brief Free a loan pool
 *
 * Ensures that the containers are cleaned up and all loans are returned
 *
 * @param pool  The loan pool to be freed
 * @return a DDS return code
 */
dds_return_t dds_loan_pool_free (dds_loan_pool_t *pool)
  ddsrt_nonnull_all;

/**
 * @brief Add a loan to be stored in the pool
 *
 * Takes over the reference of the `loaned_sample` passed in
 *
 * @param[in] pool  The loan pool to store the loan with
 * @param[in] loaned_sample   The loaned sample to store
 * @return a DDS return code
 */
dds_return_t dds_loan_pool_add_loan (dds_loan_pool_t *pool, dds_loaned_sample_t *loaned_sample)
  ddsrt_nonnull_all ddsrt_attribute_warn_unused_result;

/**
 * @brief Finds a loan in the loan pool and removes it
 *
 * Finds a loan in the pool, based on a sample pointer.
 *
 * Does not modify loaned sample's reference count.
 *
 * @param[in] pool  Loan pool to find the loan in
 * @param[in] sample_ptr  Pointer of the sample to search for and remove
 * @return A pointer to a loaned sample or null
 */
dds_loaned_sample_t *dds_loan_pool_find_and_remove_loan (dds_loan_pool_t *pool, const void *sample_ptr)
  ddsrt_nonnull_all ddsrt_attribute_warn_unused_result;

/**
 * @brief Gets the first loan from this pool and removes it from the pool
 *
 * @param[in] pool  The loan pool to get the loan from
 * @return Pointer to a loaned sample
  */
dds_loaned_sample_t *dds_loan_pool_get_loan (dds_loan_pool_t *pool)
  ddsrt_nonnull_all ddsrt_attribute_warn_unused_result;

#if defined(__cplusplus)
}
#endif

#endif /* DDS__LOANED_SAMPLE_H */

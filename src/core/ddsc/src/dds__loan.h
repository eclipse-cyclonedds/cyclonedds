// Copyright(c) 2006 to 2023 ZettaScale Technology and others
//
// This program and the accompanying materials are made available under the
// terms of the Eclipse Public License v. 2.0 which is available at
// http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
// v. 1.0 which is available at
// http://www.eclipse.org/org/documents/edl-v10.php.
//
// SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

#ifndef DDS__LOAN_H
#define DDS__LOAN_H

#include "dds/ddsc/dds_loan.h"
#include "dds__types.h"

#if defined(__cplusplus)
extern "C" {
#endif

/**
 * @brief Implementation specific loan manager
 */
typedef struct dds_loan_manager {
  //FIXME map better?
  dds_loaned_sample_t **samples;
  uint32_t n_samples_cap;
  uint32_t n_samples_managed;
  ddsrt_mutex_t mutex;
} dds_loan_manager_t;


/**
 * @brief Generic loaned sample cleanup function
 *
 * Will be called when the loaned sample runs out of refs or is
 * retracted, calls the implementation specific functions
 *
 * @param[in] loaned_sample  A loaned sample
 * @return a DDS return code
 */
dds_return_t dds_loaned_sample_free (dds_loaned_sample_t *loaned_sample);

/**
 * @brief Reset the contents for a sample
 *
 * @param[in] loaned_sample  A loaned sample
 * @return a DDS return code
 */
dds_return_t dds_loaned_sample_reset_sample (dds_loaned_sample_t *loaned_sample);


/**
 * @brief Create a loan manager
 *
 * @param[out] manager Gets a pointer to the newly created loan manager
 * @param[in] initial_cap Initial capacity
 * @return a DDS return code
 */
dds_return_t dds_loan_manager_create (dds_loan_manager_t **manager, uint32_t initial_cap);

/**
 * @brief Free a loan manager
 *
 * Ensures that the containers are cleaned up and all loans are returned
 *
 * @param manager  The loan manager to be freed
 * @return a DDS return code
 */
dds_return_t dds_loan_manager_free (dds_loan_manager_t *manager);

/**
 * @brief Add a loan to be stored by the manager
 *
 * Takes over the reference of the `loaned_sample` passed in
 *
 * @param[in] manager  The loan manager to store the loan with
 * @param[in] loaned_sample   The loaned sample to store
 * @return a DDS return code
 */
dds_return_t dds_loan_manager_add_loan (dds_loan_manager_t *manager, dds_loaned_sample_t *loaned_sample);

/**
 * @brief Remove loan from storage of the manager
 *
 * @param[in] loaned_sample  The loaned sample to be removed
 * @return a DDS return code
 */
dds_return_t dds_loan_manager_remove_loan (dds_loaned_sample_t *loaned_sample);

/**
 * @brief Finds a loan in the loan manager storage
 *
 * Finds a loan in the storage of the provided loan manager, based on
 * a sample pointer.
 *
 * Does not modify loaned sample's reference count.
 *
 * @param[in] manager  Loan manager to find the loan in
 * @param[in] sample_ptr  Pointer of the sample to search for
 * @return A pointer to a loaned sample
 */
dds_loaned_sample_t *dds_loan_manager_find_loan (dds_loan_manager_t *manager, const void *sample_ptr);

/**
 * @brief Gets the first managed loan from this manager and removes it from the manager
 *
 * @param[in] manager  The loan manager to get the loan from
 * @return Pointer to a loaned sample
 */
dds_loaned_sample_t *dds_loan_manager_get_loan (dds_loan_manager_t *manager);

#if defined(__cplusplus)
}
#endif

#endif /* DDS__LOAN_H */

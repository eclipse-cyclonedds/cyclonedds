// Copyright(c) 2023 ZettaScale Technology and others
//
// This program and the accompanying materials are made available under the
// terms of the Eclipse Public License v. 2.0 which is available at
// http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
// v. 1.0 which is available at
// http://www.eclipse.org/org/documents/edl-v10.php.
//
// SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

#ifndef DDS__READ_H
#define DDS__READ_H

#include "dds__types.h"
#include "dds__entity.h"

#if defined (__cplusplus)
extern "C" {
#endif

/** @component read_data */
struct dds_read_collect_sample_arg {
  uint32_t next_idx; /**< next index in ptrs/infos to be filled (initially 0) **/
  void **ptrs; /**< array of pointers to samples/serdatas to be filled **/
  dds_sample_info_t *infos; /**< array of sample infos to be filled **/
  struct dds_loan_pool *loan_pool; /**< loan pool to be used for loaned sample administration **/
  struct dds_loan_pool *heap_loan_cache; /**< pool of cached heap loans */
};

/** @brief Initialize the sample collector state
 * @component read_data
 *
 * @param[out] arg sample collector state to be initialized
 * @param[in] ptrs array of pointers to samples to be filled (collect_sample and
 *            collect_sample_loan) or array to be filled with pointers-to-serdata
 *            (collect_sample_refs)
 * @param[in] infos array of sample infos to be filled
 * @param[in] loan_pool the loan pool the loan will be inserted in (only used by
 *            collect_sample_loan, may be null otherwise)
 * @param[in] heap_loan_cache a cache of returned heap loans (optional; only used by
 *            collect_sample_loan)
 */
void dds_read_collect_sample_arg_init (struct dds_read_collect_sample_arg *arg, void **ptrs, dds_sample_info_t *infos, struct dds_loan_pool *loan_pool, struct dds_loan_pool *heap_loan_cache)
  ddsrt_nonnull ((1, 2, 3));

/** @brief Sample collector that deserializes the samples into ptrs[i]
 * @component read_data
 *
 * It assumes the ptrs and infos arrays are large enough and ptrs are valid and point to an
 * allocated sample.
 *
 * @see dds_read_with_collector_fn_t
 *
 * @retval DDS_RETCODE_OK if deserialization succeded
 * @retval DDS_RETCODE_ERROR if deserialization failed */
dds_return_t dds_read_collect_sample (void *varg, const dds_sample_info_t *si, const struct ddsi_sertype *st, struct ddsi_serdata *sd);

/** @brief Sample collector that deserializes the samples into ptrs[i]
 * @component read_data
 *
 * It assumes the ptrs and infos arrays are large enough and ptrs are not allocated.
 *
 * @see dds_read_with_collector_fn_t
 *
 * @retval DDS_RETCODE_OK if deserialization succeded
 * @retval DDS_RETCODE_ERROR if deserialization failed */
dds_return_t dds_read_collect_sample_loan (void *varg, const dds_sample_info_t *si, const struct ddsi_sertype *st, struct ddsi_serdata *sd);

/** @brief Sample collector that stores a pointer to the serdata and increments its refcount
 * @component read_data
 *
 * It assumes the ptrs and infos arrays are large enough.
 *
 * @see dds_read_with_collector_fn_t
 *
 * @retval DDS_RETCODE_OK (can't fail) */
dds_return_t dds_read_collect_sample_refs (void *varg, const dds_sample_info_t *si, const struct ddsi_sertype *st, struct ddsi_serdata *sd);

#if defined (__cplusplus)
}
#endif

#endif // DDS__READ_H

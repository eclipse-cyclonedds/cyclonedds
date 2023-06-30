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
  uint32_t first_of_inst_idx;     /**< first index in ptrs/infos for current instance (initially 0) **/
  uint32_t next_idx;              /**< next index in ptrs/infos to be filled (initially 0) **/
  dds_instance_handle_t last_iid; /**< last seen instance handle (initially 0) **/
  void **ptrs;                    /**< array of pointers to samples/serdatas to be filled **/
  dds_sample_info_t *infos;       /**< array of sample infos to be filled **/
};

/** @brief Initialize the sample collector state
 * @component read_data
 *
 * @param[out] arg sample collector state to be initialized
 * @param[in] ptrs array of pointers to samples to be filled (collect_sample)
 *            or array to be filled with pointers-to-serdata (collect_sample_refs)
 * @param[in] infos array of sample infos to be filled */
void dds_read_collect_sample_arg_init (struct dds_read_collect_sample_arg *arg, void **ptrs, dds_sample_info_t *infos);

/** @brief Sample collector that deserializes the samples into ptrs[i]
 * @component read_data
 *
 * It assumes the ptrs and infos arrays are large enough.  On instance change it patches
 * the ranks in the sample infos using @ref dds_read_check_and_handle_instance_switch
 *
 * @see dds_read_with_collector_fn_t
 *
 * @retval DDS_RETCODE_OK if deserialization succeded
 * @retval DDS_RETCODE_ERROR if deserialization failed */
dds_return_t dds_read_collect_sample (void *varg, const dds_sample_info_t *si, const struct ddsi_sertype *st, struct ddsi_serdata *sd);

/** @brief Sample collector that stores a pointer to the serdata and increments its refcount
 * @component read_data
 *
 * It assumes the ptrs and infos arrays are large enough.  On instance change it patches
 * the ranks in the sample infos using @ref dds_read_check_and_handle_instance_switch
 *
 * @see dds_read_with_collector_fn_t
 *
 * @retval DDS_RETCODE_OK (can't fail) */
dds_return_t dds_read_collect_sample_refs (void *varg, const dds_sample_info_t *si, const struct ddsi_sertype *st, struct ddsi_serdata *sd);

/** @brief Function to check for instance change and patch ranks in sample infos for preceding instance
 * @component read_data
 *
 * @note This is called automatically by @ref dds_read_collect_sample and @ref
 * dds_read_collect_sample_refs for each sample, but the caller of @ref dds_rhc_read (or
 * @ref dds_rhc_take, or the other routes) needs to call it at the end to patch the final
 * instance.
 *
 * @param[in] arg Current state of the collector
 * @param[in] iid New instance handle, pass in 0 for handling the final instance */
void dds_read_check_and_handle_instance_switch (struct dds_read_collect_sample_arg * const arg, dds_instance_handle_t iid);

#if defined (__cplusplus)
}
#endif

#endif // DDS__READ_H
